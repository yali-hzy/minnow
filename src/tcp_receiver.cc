#include "tcp_receiver.hh"
#include "debug.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  if ( message.RST ) {
    reassembler_.reader().set_error();
    return;
  }
  if ( message.SYN ) {
    zero_point_ = message.seqno;
    SYN_received_ = true;
    FIN_received_ = false;
  }
  if ( !SYN_received_ )
    return;
  uint64_t first_index = message.seqno.unwrap( zero_point_, reassembler_.writer().bytes_pushed() );
  if ( !message.SYN ) {
    if ( first_index == 0 )
      return;
    first_index--;
  }
  reassembler_.insert( first_index, message.payload, message.FIN );
  if ( message.FIN ) {
    FIN_received_ = true;
    last_index_ = first_index + message.payload.size();
  }
}

TCPReceiverMessage TCPReceiver::send() const
{
  TCPReceiverMessage msg;
  if ( SYN_received_ ) {
    uint32_t absolute_ackno = reassembler_.writer().bytes_pushed() + 1;
    if ( FIN_received_ && absolute_ackno == last_index_ + 1 )
      absolute_ackno++;
    msg.ackno = zero_point_ + absolute_ackno;
  }
  msg.window_size = min( 65535ul, reassembler_.writer().available_capacity() );
  msg.RST = reassembler_.writer().has_error();
  return msg;
}
