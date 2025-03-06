#include <iostream>

#include "arp_message.hh"
#include "debug.hh"
#include "ethernet_frame.hh"
#include "exception.hh"
#include "helpers.hh"
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
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  uint32_t next_hop_ip = next_hop.ipv4_numeric();
  EthernetAddress next_hop_eth;
  Serializer serializer;
  EthernetFrame frame;
  if ( arp_table_.find( next_hop_ip ) == arp_table_.end() ) {
    datagrams_to_send_[next_hop_ip].push( { std::move( dgram ), timer_ } );
    if ( arp_requests_.find( next_hop_ip ) != arp_requests_.end() )
      return;
    ARPMessage arp_request;
    arp_request.opcode = ARPMessage::OPCODE_REQUEST;
    arp_request.sender_ethernet_address = ethernet_address_;
    arp_request.sender_ip_address = ip_address_.ipv4_numeric();
    // arp_request.target_ethernet_address = ETHERNET_BROADCAST;
    arp_request.target_ip_address = next_hop_ip;
    frame.header.dst = ETHERNET_BROADCAST;
    frame.header.src = ethernet_address_;
    frame.header.type = EthernetHeader::TYPE_ARP;
    arp_request.serialize( serializer );
    frame.payload = serializer.finish();
    transmit( frame );
    arp_requests_[next_hop_ip] = timer_;
  } else {
    next_hop_eth = arp_table_[next_hop_ip].first;
    frame.header.dst = next_hop_eth;
    frame.header.src = ethernet_address_;
    frame.header.type = EthernetHeader::TYPE_IPv4;
    dgram.serialize( serializer );
    frame.payload = serializer.finish();
    transmit( frame );
  }
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( EthernetFrame frame )
{
  if ( frame.header.dst != ethernet_address_ && frame.header.dst != ETHERNET_BROADCAST )
    return;
  Parser parser( frame.payload );
  Serializer serializer;
  EthernetFrame frame_to_send;
  if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) {
    InternetDatagram dgram;
    dgram.parse( parser );
    datagrams_received_.push( std::move( dgram ) );
  } else if ( frame.header.type == EthernetHeader::TYPE_ARP ) {
    ARPMessage arp_message;
    arp_message.parse( parser );
    if ( arp_message.opcode == ARPMessage::OPCODE_REQUEST
         && arp_message.target_ip_address == ip_address_.ipv4_numeric() ) {
      ARPMessage arp_reply;
      arp_reply.opcode = ARPMessage::OPCODE_REPLY;
      arp_reply.sender_ethernet_address = ethernet_address_;
      arp_reply.sender_ip_address = ip_address_.ipv4_numeric();
      arp_reply.target_ethernet_address = arp_message.sender_ethernet_address;
      arp_reply.target_ip_address = arp_message.sender_ip_address;
      arp_reply.serialize( serializer );
      frame_to_send.header.dst = arp_message.sender_ethernet_address;
      frame_to_send.header.src = ethernet_address_;
      frame_to_send.header.type = EthernetHeader::TYPE_ARP;
      frame_to_send.payload = serializer.finish();
      transmit( frame_to_send );
    }
    arp_table_[arp_message.sender_ip_address] = { arp_message.sender_ethernet_address, timer_ };
    while ( !datagrams_to_send_[arp_message.sender_ip_address].empty() ) {
      InternetDatagram dgram = std::move( datagrams_to_send_[arp_message.sender_ip_address].front().first );
      datagrams_to_send_[arp_message.sender_ip_address].pop();
      frame_to_send.header.dst = arp_message.sender_ethernet_address;
      frame_to_send.header.src = ethernet_address_;
      frame_to_send.header.type = EthernetHeader::TYPE_IPv4;
      dgram.serialize( serializer );
      frame_to_send.payload = serializer.finish();
      transmit( frame_to_send );
    }
    datagrams_to_send_.erase( arp_message.sender_ip_address );
    arp_requests_.erase( arp_message.sender_ip_address );
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  timer_ += ms_since_last_tick;
  std::erase_if( arp_requests_,
                 [this]( const auto& pair ) { return timer_ - pair.second > ARP_REQUEST_INTERVAL_MS; } );
  std::erase_if( arp_table_, [this]( const auto& pair ) { return timer_ - pair.second.second > ARP_TIMEOUT_MS; } );
  for ( auto& [ip, q] : datagrams_to_send_ )
    while ( !q.empty() && timer_ - q.front().second > ARP_REQUEST_INTERVAL_MS )
      q.pop();
  std::erase_if( datagrams_to_send_, []( const auto& pair ) { return pair.second.empty(); } );
}
