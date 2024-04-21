#include "router.hh"

#include <iostream>
#include <limits>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

  auto cur = root_.get();
  if ( prefix_length == 0 ) {
    if (!root_){
      root_ = make_unique<TrieNode>();
    }
    root_->interface_num = interface_num;
    if ( next_hop.has_value() )
      root_->next_hop = next_hop;
    return;
  }
  for ( auto i = 1; i <= prefix_length; i++ ) {
    auto bit = route_prefix & ( 1 << ( 32 - i ) );
    if (bit){
      if (cur->right == nullptr){
        cur->right = make_unique<TrieNode>();
      }
      cur = cur->right.get();
    } else {
      if (cur->left == nullptr){
        cur->left = make_unique<TrieNode>();
      }
      cur = cur->left.get();
    }
    if ( i == prefix_length ){
      cur->interface_num = interface_num;
      if ( next_hop.has_value() )
        cur->next_hop = next_hop;
    }
  }
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{ 
  for ( auto& i : _interfaces ) {
    while ( !i->datagrams_received().empty() ) {
      datagram_buffer_.push( i->datagrams_received().front() );
      i->datagrams_received().pop();
    }
  }
  queue<InternetDatagram> datagram_buffer_copy;
  while ( !datagram_buffer_.empty() ) {
    InternetDatagram& d = datagram_buffer_.front();
    if (d.header.ttl == 0){
      datagram_buffer_.pop();
      continue;
    }
    if ( !match( d ) ) {
      d.header.ttl--;
      datagram_buffer_copy.push( d );
    }
    datagram_buffer_.pop();
  }
  datagram_buffer_ = move(datagram_buffer_copy);
}

bool Router::match( InternetDatagram& d )
{ 
  auto ip = d.header.dst;
  auto cur = root_.get();
  optional<size_t> infce;
  optional<Address> addr; 
  if (root_->interface_num.has_value()) {
    infce = root_->interface_num;
    addr = root_->next_hop;
  }
  for ( auto i = 1; i <= 32; i++ ) {
    auto bit = ip & ( 1 << ( 32 - i ) );
    cur = bit ? cur->right.get() : cur->left.get();
    if ( cur == nullptr ) {
      break;
    }
    if ( cur->interface_num.has_value() ) {
      infce = cur->interface_num;
      addr = cur->next_hop;
    }
  }
  if (!infce.has_value()){
    cout << "DEBUG: no match " << Address::from_ipv4_numeric( ip ).ip() << "\n";
    return false;
  }
  cout << "DEBUG: match " << Address::from_ipv4_numeric( ip ).ip() << " => " << infce.value() << "\n";
  d.header.ttl--;
  if( d.header.ttl == 0 ){
    return true;
  }
  d.header.compute_checksum();
  if (addr.has_value()){
    interface(infce.value())->send_datagram( d, addr.value() );
  }
  else {
    interface(infce.value())->send_datagram( d, Address::from_ipv4_numeric( d.header.dst ));
  }
  return true;
}