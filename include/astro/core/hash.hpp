#pragma once
#include <array>
#include <span>
#include <string>

namespace astro::core {
  using Hash256 = std::array<uint8_t, 32>;
  using Hash160 = std::array<uint8_t, 20>;

  auto sha256(std::span<const uint8_t> data) -> Hash256;
  auto hash160(std::span<const uint8_t> data) -> Hash160;

  auto sha256(const std::string& data) -> Hash256;
  auto hash160(const std::string& data) -> Hash160;

  auto toHex(std::span<const uint8_t> data) -> std::string;
}