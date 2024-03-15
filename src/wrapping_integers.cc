#include "wrapping_integers.hh"
#include <iostream>

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return Wrap32 { static_cast<uint32_t>( ( ( n + zero_point.raw_value_ ) % ( 1ul << 32 ) ) ) };
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  uint64_t tmp = raw_value_ >= zero_point.raw_value_ ? 0 : ( 1ul << 32 );
  uint64_t base = raw_value_ + tmp - zero_point.raw_value_;
    if (base + ( 1ul << 31 ) < checkpoint){
      base += ((checkpoint - base + (1ul << 31)) / (1ul << 32)) * (1ul << 32);
    }
  return base;
}
