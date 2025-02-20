#include "wrapping_integers.hh"
#include "debug.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return Wrap32( static_cast<uint32_t>( n + zero_point.raw_value_ ) );
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  uint64_t result = ( checkpoint >> 32 << 32 ) + raw_value_ - zero_point.raw_value_;
  if ( raw_value_ < zero_point.raw_value_ )
    result += 1ull << 32;
  const uint64_t candidata2 = result - ( 1ull << 32 );
  const uint64_t candidata3 = result + ( 1ull << 32 );
  if ( result >= ( 1ull << 32 )
       && min( candidata2 - checkpoint, checkpoint - candidata2 )
            < min( result - checkpoint, checkpoint - result ) )
    result = candidata2;
  if ( min( candidata3 - checkpoint, checkpoint - candidata3 ) < min( result - checkpoint, checkpoint - result ) )
    result = candidata3;
  return result;
}
