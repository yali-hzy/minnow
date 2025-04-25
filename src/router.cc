#include "router.hh"
#include "debug.hh"

#include <iostream>

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

  trie.insert( route_prefix, prefix_length, next_hop, interface_num );
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  for ( auto& interface_ : interfaces_ )
    while ( !interface_->datagrams_received().empty() ) {
      InternetDatagram dgram = move( interface_->datagrams_received().front() );
      interface_->datagrams_received().pop();
      const uint32_t dst = dgram.header.dst;
      const uint8_t ttl = dgram.header.ttl;
      optional<Address> next_hop;
      optional<size_t> interface_num;
      tie( next_hop, interface_num ) = trie.longest_prefix_match( dst );
      if ( !interface_num.has_value() || ttl <= 1 )
        return;
      dgram.header.ttl--;
      Address next_hop_address = next_hop.has_value() ? next_hop.value() : Address::from_ipv4_numeric( dst );
      interface( interface_num.value() )->send_datagram( move( dgram ), next_hop_address );
    }
}

std::pair<std::optional<Address>, std::optional<size_t>> Router::Trie::longest_prefix_match(
  const uint32_t address ) const
{
  std::shared_ptr<TrieNode> current = root;
  std::pair<std::optional<Address>, std::optional<size_t>> result;
  if ( current->interface_num.has_value() )
    result = { current->next_hop, current->interface_num };
  for ( int i = 31; i >= 0; i-- ) {
    const bool bit = ( address >> i ) & 1;
    if ( current->children[bit] == nullptr )
      return result;
    current = current->children[bit];
    if ( current->interface_num.has_value() )
      result = { current->next_hop, current->interface_num };
  }
  return result;
}

void Router::Trie::insert( const uint32_t route_prefix,
                           const uint8_t prefix_length,
                           const optional<Address> next_hop,
                           const size_t interface_num )
{
  std::shared_ptr<TrieNode> current = root;
  for ( int i = 31; i >= 32 - prefix_length; i-- ) {
    const bool bit = ( route_prefix >> i ) & 1;
    if ( current->children[bit] == nullptr )
      current->children[bit] = std::make_shared<TrieNode>();
    current = current->children[bit];
  }
  current->next_hop = next_hop;
  current->interface_num = interface_num;
}