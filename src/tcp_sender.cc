#include "tcp_sender.hh"
#include "debug.hh"
#include "tcp_config.hh"

using namespace std;

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::sequence_numbers_in_flight() const
{
  uint64_t count = 0;
  for ( const auto& p : outstanding_ )
    count += p.second.sequence_length();
  return count;
}

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::consecutive_retransmissions() const
{
  return consecutive_retransmissions_;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  while ( next_abs_seqno_to_send_ < last_abs_ack_received_ + max( 1, static_cast<int>( rcv_wnd_ ) )
          && !FIN_sent_ ) {
    TCPSenderMessage msg;
    msg.seqno = Wrap32::wrap( next_abs_seqno_to_send_, isn_ );
    const uint64_t bytes_to_read = min( min( TCPConfig::MAX_PAYLOAD_SIZE, reader().bytes_buffered() ),
                                        last_abs_ack_received_ + max( 1, static_cast<int>( rcv_wnd_ ) )
                                          - next_abs_seqno_to_send_ - !SYN_sent_ );
    msg.SYN = !SYN_sent_;
    SYN_sent_ = true;
    if ( bytes_to_read ) {
      msg.payload = reader().peek().substr( 0, bytes_to_read );
      reader().pop( bytes_to_read );
    }
    if ( next_abs_seqno_to_send_ + msg.sequence_length()
           < last_abs_ack_received_ + max( 1, static_cast<int>( rcv_wnd_ ) )
         && reader().is_finished() ) {
      msg.FIN = true;
      FIN_sent_ = true;
    }
    msg.RST = reader().has_error();
    if ( msg.sequence_length() ) {
      transmit( msg );
      outstanding_.insert( { next_abs_seqno_to_send_, msg } );
      if ( !timer_running_ ) {
        timer_running_ = true;
        timer_ = 0;
      }
    }
    next_abs_seqno_to_send_ += msg.sequence_length();
    // If we have nothing to send, 1) we have sent a FIN, 2) the window is full,
    // or 3) the reader has no more buffered bytes, don't try to send more.
    if ( reader().bytes_buffered() == 0 )
      break;
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  TCPSenderMessage msg;
  msg.seqno = Wrap32::wrap( next_abs_seqno_to_send_, isn_ );
  msg.RST = reader().has_error();
  return msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  rcv_wnd_ = msg.window_size;
  if ( msg.RST )
    reader().set_error();
  if ( !msg.ackno.has_value() )
    return;
  const uint64_t abs_ackno = msg.ackno->unwrap( isn_, reader().bytes_popped() );
  if ( abs_ackno > next_abs_seqno_to_send_ || abs_ackno <= last_abs_ack_received_ )
    return;
  auto it = outstanding_.lower_bound( { abs_ackno, TCPSenderMessage() } );
  if ( it != outstanding_.end() && it->first == abs_ackno ) {
    outstanding_.erase( outstanding_.begin(), it );
  } else if ( it != outstanding_.begin() ) {
    --it;
    if ( it->first + it->second.sequence_length() <= abs_ackno )
      ++it;
    outstanding_.erase( outstanding_.begin(), it );
  }
  if ( outstanding_.empty() )
    timer_running_ = false;
  last_abs_ack_received_ = abs_ackno;
  RTO_ = initial_RTO_ms_;
  if ( !outstanding_.empty() )
    timer_ = 0;
  consecutive_retransmissions_ = 0;
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  if ( !timer_running_ )
    return;
  timer_ += ms_since_last_tick;
  if ( timer_ >= RTO_ ) {
    transmit( outstanding_.begin()->second );
    if ( rcv_wnd_ > 0 ) {
      RTO_ *= 2;
      ++consecutive_retransmissions_;
    }
    timer_running_ = true;
    timer_ = 0;
  }
}
