#pragma once

#include "exception.hh"
#include "network_interface.hh"

#include <optional>

// \brief A router that has multiple network interfaces and
// performs longest-prefix-match routing between them.
class Router
{
public:
  // Add an interface to the router
  // \param[in] interface an already-constructed network interface
  // \returns The index of the interface after it has been added to the router
  size_t add_interface( std::shared_ptr<NetworkInterface> interface )
  {
    interfaces_.push_back( notnull( "add_interface", std::move( interface ) ) );
    return interfaces_.size() - 1;
  }

  // Access an interface by index
  std::shared_ptr<NetworkInterface> interface( const size_t N ) { return interfaces_.at( N ); }

  // Add a route (a forwarding rule)
  void add_route( uint32_t route_prefix,
                  uint8_t prefix_length,
                  std::optional<Address> next_hop,
                  size_t interface_num );

  // Route packets between the interfaces
  void route();

private:
  // The router's collection of network interfaces
  std::vector<std::shared_ptr<NetworkInterface>> interfaces_ {};

  class Trie
  {
  public:
    Trie() : root( std::make_shared<TrieNode>() ) {}
    std::pair<std::optional<Address>, std::optional<size_t>> longest_prefix_match( uint32_t address ) const;
    void insert( uint32_t route_prefix,
                 uint8_t prefix_length,
                 std::optional<Address> next_hop,
                 size_t interface_num );

  private:
    class TrieNode
    {
    public:
      std::array<std::shared_ptr<TrieNode>, 2> children {};
      std::optional<Address> next_hop {};
      std::optional<size_t> interface_num {};
    };
    std::shared_ptr<TrieNode> root;
  } trie {};
};
