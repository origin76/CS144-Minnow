#include "tcp_receiver.hh"
#include <iostream>

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  if ( message.RST ) {
    reassembler_.reader().set_error();
  }
  if ( message.SYN ) {
    ISN = message.seqno;
  }
  if ( ISN.has_value() ) {
    auto stream_idx = message.seqno.unwrap( ISN.value(), cp );
    if ( !message.SYN ) {
      stream_idx -= 1;
    }
    auto old_cnt = reassembler_.writer().bytes_pushed();
    reassembler_.insert( stream_idx, message.payload, message.FIN );
    auto offset = reassembler_.writer().bytes_pushed() - old_cnt;
    cp += offset;
    cp += message.SYN;
    if (reassembler_.writer().is_closed()){
      cp += 1;
    }
  }
}

TCPReceiverMessage TCPReceiver::send() const
{
  TCPReceiverMessage message;
  if ( ISN.has_value() ) {
    message.ackno = Wrap32::wrap( cp, ISN.value() );
  }
  if ( reassembler_.reader().has_error() || reassembler_.writer().has_error()) {
    message.RST = true;
  }
  message.window_size = min( reassembler_.writer().available_capacity(), 65535ul );
  return message;
}
