#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

#include <functional>
#include <set>

class TCPSender
{
public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( ByteStream&& input, Wrap32 isn, uint64_t initial_RTO_ms )
    : input_( std::move( input ) ), isn_( isn ), initial_RTO_ms_( initial_RTO_ms )
  {}

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage make_empty_message() const;

  /* Receive and process a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Type of the `transmit` function that the push and tick methods can use to send messages */
  using TransmitFunction = std::function<void( const TCPSenderMessage& )>;

  /* Push bytes from the outbound stream */
  void push( const TransmitFunction& transmit );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called */
  void tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit );

  // Accessors
  uint64_t sequence_numbers_in_flight() const;  // For testing: how many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // For testing: how many consecutive retransmissions have happened?
  const Writer& writer() const { return input_.writer(); }
  const Reader& reader() const { return input_.reader(); }
  Writer& writer() { return input_.writer(); }

private:
  Reader& reader() { return input_.reader(); }

  ByteStream input_;
  Wrap32 isn_;
  uint64_t initial_RTO_ms_;

  uint64_t RTO_ { initial_RTO_ms_ };
  uint16_t rcv_wnd_ = 1;
  uint64_t last_abs_ack_received_ = 0;
  uint64_t next_abs_seqno_to_send_ = 0;
  bool SYN_sent_ = false;
  bool FIN_sent_ = false;
  uint64_t timer_ = 0;
  bool timer_running_ = false;
  std::function<bool( const std::pair<uint64_t, TCPSenderMessage>&, const std::pair<uint64_t, TCPSenderMessage>& )>
    compare = []( const std::pair<uint64_t, TCPSenderMessage>& a, const std::pair<uint64_t, TCPSenderMessage>& b ) {
      return a.first < b.first;
    };
  std::set<std::pair<uint64_t, TCPSenderMessage>, decltype( compare )> outstanding_ { compare };
  uint64_t consecutive_retransmissions_ = 0;
};
