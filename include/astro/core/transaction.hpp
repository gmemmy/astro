#pragma once
#include <cstdint>
#include <vector>
#include <span>
#include <string>
#include <astro/core/hash.hpp>

namespace astro::core {

 struct Transaction {
  uint16_t version = 1;
  uint64_t nonce = 0;
  uint64_t amount = 0;
  
  std::vector<uint8_t> from_pub_pem;
  std::string to_label;

  std::vector<uint8_t> signature;

  std::vector<uint8_t> serialize(bool for_signing=false) const;

  Hash256 tx_hash() const;

  void sign(std::span<const uint8_t> privkey_pem);

  bool verify() const;
  
 };

}