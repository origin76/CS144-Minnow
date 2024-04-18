#include <iostream>

#include "arp_message.hh"
#include "exception.hh"
#include "network_interface.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  if ( arp_table_.contains( next_hop.ipv4_numeric() ) ) {
    EthernetFrame frame{
      .header = {
        .dst = arp_table_[next_hop.ipv4_numeric()],
        .src = ethernet_address_,
        .type = frame.header.TYPE_IPv4,
      },
      .payload = serialize(dgram),
    };
    transmit( frame );
  } else {
    bool f = true;
    if ( retransmit_arp_queue_.contains( next_hop.ipv4_numeric() ) ) {
      if ( timer_ - retransmit_arp_queue_[next_hop.ipv4_numeric()] < 5000 ) {
        f = false;
      }
    }
    if ( f ) {
      ARPMessage arp_msg {
        .opcode = ARPMessage::OPCODE_REQUEST,
        .sender_ethernet_address = ethernet_address_,
        .sender_ip_address = ip_address_.ipv4_numeric(),
        .target_ethernet_address = 0,
        .target_ip_address = next_hop.ipv4_numeric(),
      };
      EthernetFrame arp_frame{
      .header = {
        .dst = ETHERNET_BROADCAST,
        .src = ethernet_address_,
        .type = arp_frame.header.TYPE_ARP,
      },
      .payload = serialize(arp_msg),
      };
      transmit( arp_frame );
      retransmit_arp_queue_.erase( next_hop.ipv4_numeric() );
      retransmit_arp_queue_.emplace( next_hop.ipv4_numeric(), timer_ );
    }
    EthernetFrame wait_frame{
      .header = {
        .dst = ETHERNET_BROADCAST,
        .src = ethernet_address_,
        .type = wait_frame.header.TYPE_IPv4,
      },
      .payload = serialize(dgram),
    };
    wait_queue_.emplace( next_hop.ipv4_numeric(), wait_frame );
  }
  (void)dgram;
  (void)next_hop;
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  if ( frame.header.dst != ethernet_address_ && frame.header.dst != ETHERNET_BROADCAST ) {
    return;
  }
  if ( frame.header.type == frame.header.TYPE_ARP ) {
    ARPMessage arp_msg;
    auto success = parse( arp_msg, frame.payload );
    if ( !success ) {
      return;
    }
    arp_timestamp_[timer_] = arp_msg.sender_ip_address;
    arp_table_[arp_msg.sender_ip_address] = arp_msg.sender_ethernet_address;
    if ( wait_queue_.contains( arp_msg.sender_ip_address ) ) {
      auto range = wait_queue_.equal_range( arp_msg.sender_ip_address );
      for ( auto i = range.first; i != range.second; i++ ) {
        i->second.header.dst = arp_msg.sender_ethernet_address;
        transmit( i->second );
      }
      wait_queue_.erase( range.first, range.second );
    }
    if ( arp_msg.target_ip_address == ip_address_.ipv4_numeric() && arp_msg.opcode == ARPMessage::OPCODE_REQUEST ) {
      ARPMessage reply {
        .opcode = ARPMessage::OPCODE_REPLY,
        .sender_ethernet_address = ethernet_address_,
        .sender_ip_address = ip_address_.ipv4_numeric(),
        .target_ethernet_address = arp_msg.sender_ethernet_address,
        .target_ip_address = arp_msg.sender_ip_address,
      };
      EthernetFrame arp_frame{
        .header = {
          .dst = arp_msg.sender_ethernet_address,
          .src = ethernet_address_,
          .type = frame.header.TYPE_ARP,
        },
        .payload = serialize(reply),
      };
      transmit( arp_frame );
    }
  } else if ( frame.header.type == frame.header.TYPE_IPv4 ) {
    InternetDatagram ip_msg;
    auto success = parse( ip_msg, frame.payload );
    if ( !success ) {
      return;
    }
    datagrams_received_.push( ip_msg );
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  timer_ += ms_since_last_tick;
  while ( !arp_timestamp_.empty() ) {
    auto [time, ip] = *arp_timestamp_.begin();
    if ( timer_ - time > 30000 ) {
      arp_timestamp_.erase( arp_timestamp_.begin() );
      arp_table_.erase( ip );
    } else {
      break;
    }
  }
}
