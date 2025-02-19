#include "reassembler.hh"
#include "debug.hh"

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  if ( is_last_substring ) {
    last_index_ = first_index + data.size();
  }
  if ( first_index < next_index_ ) {
    if ( first_index + data.size() <= next_index_ )
      return;
    data = data.substr( next_index_ - first_index );
    first_index = next_index_;
  }
  if ( output_.writer().available_capacity() < data.size() + first_index - next_index_ ) {
    if ( output_.writer().available_capacity() <= first_index - next_index_ )
      return;
    data.resize( output_.writer().available_capacity() - first_index + next_index_ );
  }
  auto it = pending_.upper_bound( first_index );
  if ( it != pending_.begin() ) {
    --it;
    if ( it->first + it->second.size() > first_index ) {
      // Overlapping with previous
      const uint64_t overlap = it->first + it->second.size() - first_index;
      if ( overlap >= data.size() ) {
        // Fully covered by previous
        return;
      }
      pending_[it->first] = it->second + data.substr( overlap );
    } else {
      it = pending_.insert( { first_index, data } ).first;
    }
  } else {
    it = pending_.insert( { first_index, data } ).first;
  }
  // Now we have the new data in 'it'
  // Check if we can merge with next
  while ( it != pending_.end() ) {
    auto current = it;
    ++it;
    if ( it != pending_.end() && current->first + current->second.size() >= it->first ) {
      // Overlapping with next
      const uint64_t overlap = current->first + current->second.size() - it->first;
      if ( overlap < it->second.size() ) {
        pending_[current->first] = current->second + it->second.substr( overlap );
      }
      pending_.erase( it );
      it = current;
    } else {
      it = current;
      break;
    }
  }
  // Now we have the new data in 'it' and it doesn't overlap with next
  // Check if we can write to output
  if ( it->first == next_index_ ) {
    output_.writer().push( it->second );
    next_index_ += it->second.size();
    pending_.erase( it );
  }
  if ( next_index_ == last_index_ ) {
    output_.writer().close();
  }
}

// How many bytes are stored in the Reassembler itself?
// This function is for testing only; don't add extra state to support it.
uint64_t Reassembler::count_bytes_pending() const
{
  uint64_t count = 0;
  for ( const auto& [index, data] : pending_ ) {
    count += data.size();
  }
  return count;
}
