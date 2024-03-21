#include "tcp_sender.hh"
#include "tcp_config.hh"
#include <iostream>

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return last_send_ - last_ack_;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return consecutive_retransmissions_;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  if ( input_.reader().is_finished() && ( window_size_ > sequence_numbers_in_flight() ) && ( !has_FIN ) ) {
    auto msg = make_empty_message();
    msg.SYN = !has_SYN;
    msg.FIN = true;
    Send_queue.push( msg );
    last_send_ += msg.sequence_length();
    has_FIN = true;
  } else {
    auto to_read = min( input_.reader().bytes_buffered(),
                        static_cast<uint64_t>( window_size_ - sequence_numbers_in_flight() ) );
    to_read = min( to_read, static_cast<uint64_t>( window_size_ ) );
    if ( to_read == 0 ) {
      auto msg = make_empty_message();
      msg.SYN = !has_SYN;
      if ( !has_SYN ) {
        has_SYN = true;
        Send_queue.push( msg );
        last_send_ += msg.sequence_length();
      } else {
        if ( window_size_ == 0 && ( !zero_windowsize_flag_ ) ) {
          if ( ( last_send_ == last_ack_ ) && input_.reader().is_finished() ) {
            msg.FIN = true;
          } else {
            std::string data( ' ', 1 );
            read( input_.reader(), 1, data );
            msg.payload = data;
          }
          Send_queue.push( msg );
          last_send_ += msg.sequence_length();
          zero_windowsize_flag_ = true;
        }
      }
    }
    while ( to_read > 0 ) {
      auto length = min( to_read, TCPConfig::MAX_PAYLOAD_SIZE );
      std::string data( ' ', length );
      read( input_.reader(), length, data );
      TCPSenderMessage msg = { .seqno = Wrap32::wrap( last_send_, isn_ ),
                               .SYN = !has_SYN,
                               .payload = data,
                               .FIN = input_.reader().is_finished(),
                               .RST = input_.has_error() };
      if ( !has_SYN )
        has_SYN = true;
      if ( msg.sequence_length() + sequence_numbers_in_flight() > window_size_ ) {
        msg.FIN = false;
      }
      Send_queue.push( msg );
      last_send_ += msg.sequence_length();
      to_read -= length;
    }
  }
  while ( !Send_queue.empty() ) {
    transmit( Send_queue.front() );
    if ( !timer_running_ ) {
      timer_running_ = true;
      timer_ = 0;
    }
    if ( Send_queue.front().FIN )
      has_FIN = true;
    Retransmit_queue.push( Send_queue.front() );
    Send_queue.pop();
  };
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  TCPSenderMessage msg = { .seqno = Wrap32::wrap( last_send_, isn_ ),
                           .SYN = false,
                           .payload = string(),
                           .FIN = false,
                           .RST = input_.has_error() };
  return msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  window_size_ = msg.window_size;
  if ( msg.RST ) {
    input_.set_error();
  }
  if ( msg.ackno.has_value() ) {
    auto recv_ack = msg.ackno.value().unwrap( isn_, last_ack_ );
    if ( recv_ack > last_send_ ) {
      return;
    }
    if ( recv_ack > last_ack_ ) {
      zero_windowsize_flag_ = false;
      last_ack_ = recv_ack;
      RTO_ms_ = initial_RTO_ms_;
      consecutive_retransmissions_ = 0;
      if ( timer_running_ && ( sequence_numbers_in_flight() != 0 ) ) {
        timer_ = 0;
      }
    }
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  if ( timer_running_ ) {
    timer_ += ms_since_last_tick;
  }
  if ( timer_ >= RTO_ms_ ) {
    while ( !Retransmit_queue.empty() ) {
      auto msg = Retransmit_queue.front();
      if ( msg.seqno.unwrap( isn_, last_send_ ) + msg.sequence_length() > last_ack_ ) {
        transmit( msg );
        if ( window_size_ != 0 ) {
          RTO_ms_ = 2 * RTO_ms_;
        }
        timer_ = 0;
        consecutive_retransmissions_++;
        break;
      } else {
        Retransmit_queue.pop();
      }
    }
  }
}
