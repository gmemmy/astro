#include "astro/core/pow.hpp"

namespace astro::core::pow {
  static inline uint32_t leading_zero_bits_one_byte(uint8_t x) {
    // count leading zeros in one byte
    if (x == 0) return 8;
    uint32_t n = 0;
    if ((x & 0xF0) == 0) { n +=4; x <<= 4; }
    if ((x & 0xC0) == 0) { n +=2; x <<= 2; }
    if ((x & 0x80) == 0) { n +=1; }
    return n;
  }

  uint32_t leading_zero_bits(const Hash256& hash) {
    uint32_t z = 0;
    for (size_t i = 0; i < hash.size(); ++i) {
      if (hash[i] == 0) { z += 8; continue;}
      z += leading_zero_bits_one_byte(hash[i]);
      break;
    }
    return z;
  }
}