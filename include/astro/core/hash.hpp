#pragma once
#include <array>
#include <span>
#include <string>

namespace astro::core {
  using Hash256 = std::array<uint8_t, 32>;
  using Hash160 = std::array<uint8_t, 20>;

  auto sha256(std::span<const uint8_t> data) -> Hash256;
  auto hash160(std::span<const uint8_t> data) -> Hash160;
  auto hash_concat(std::span<const uint8_t> left, std::span<const uint8_t> right) -> Hash256;

  inline auto sha256(const std::string& data) -> Hash256 {
    return sha256(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(data.data()), data.size()));
  }
  inline auto hash160(const std::string& data) -> Hash160 {
    return hash160(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(data.data()), data.size()));
  }

  auto toHex(std::span<const uint8_t> data) -> std::string;
  inline std::string to_hex(std::span<const uint8_t> data) { return toHex(data); }
}