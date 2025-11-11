#pragma once
#include <cstdint>
#include "astro/core/hash.hpp"

namespace astro::core::pow {
  uint32_t leading_zero_bits(const Hash256& hash);

  inline bool meets_difficulty(uint32_t difficulty_bits, const Hash256& hash) {
    return leading_zero_bits(hash) >= difficulty_bits;
  }

}