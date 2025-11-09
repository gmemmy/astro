#include <gtest/gtest.h>
#include "astro/core/hash.hpp"
#
using namespace astro::core;
#
static std::vector<uint8_t> concat_vecs(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
  std::vector<uint8_t> out;
  out.reserve(a.size() + b.size());
  out.insert(out.end(), a.begin(), a.end());
  out.insert(out.end(), b.begin(), b.end());
  return out;
}
#
TEST(HashConcat, HandlesEmptyInputs) {
  std::vector<uint8_t> empty;
  auto got = hash_concat(std::span<const uint8_t>(empty.data(), empty.size()),
                         std::span<const uint8_t>(empty.data(), empty.size()));
  auto expected = sha256(std::span<const uint8_t>(empty.data(), empty.size()));
  EXPECT_EQ(to_hex(std::span<const uint8_t>(got.data(), got.size())),
            to_hex(std::span<const uint8_t>(expected.data(), expected.size())));
}
#
TEST(HashConcat, LeftEmptyEqualsSha256Right) {
  std::vector<uint8_t> empty;
  std::vector<uint8_t> right{'a','b','c'};
  auto got = hash_concat(std::span<const uint8_t>(empty.data(), empty.size()),
                         std::span<const uint8_t>(right.data(), right.size()));
  auto expected = sha256(std::span<const uint8_t>(right.data(), right.size()));
  EXPECT_EQ(to_hex(std::span<const uint8_t>(got.data(), got.size())),
            to_hex(std::span<const uint8_t>(expected.data(), expected.size())));
}
#
TEST(HashConcat, RightEmptyEqualsSha256Left) {
  std::vector<uint8_t> left{'x','y'};
  std::vector<uint8_t> empty;
  auto got = hash_concat(std::span<const uint8_t>(left.data(), left.size()),
                         std::span<const uint8_t>(empty.data(), empty.size()));
  auto expected = sha256(std::span<const uint8_t>(left.data(), left.size()));
  EXPECT_EQ(to_hex(std::span<const uint8_t>(got.data(), got.size())),
            to_hex(std::span<const uint8_t>(expected.data(), expected.size())));
}
#
TEST(HashConcat, MatchesManualConcatThenSha256) {
  std::vector<uint8_t> left{'1','2','3'};
  std::vector<uint8_t> right{'4','5'};
  auto got = hash_concat(std::span<const uint8_t>(left.data(), left.size()),
                         std::span<const uint8_t>(right.data(), right.size()));
  auto combined = concat_vecs(left, right);
  auto expected = sha256(std::span<const uint8_t>(combined.data(), combined.size()));
  EXPECT_EQ(to_hex(std::span<const uint8_t>(got.data(), got.size())),
            to_hex(std::span<const uint8_t>(expected.data(), expected.size())));
}
#
TEST(HashConcat, OrderMatters) {
  std::vector<uint8_t> left{'A'};
  std::vector<uint8_t> right{'B'};
  auto ab = hash_concat(std::span<const uint8_t>(left.data(), left.size()),
                        std::span<const uint8_t>(right.data(), right.size()));
  auto ba = hash_concat(std::span<const uint8_t>(right.data(), right.size()),
                        std::span<const uint8_t>(left.data(), left.size()));
  EXPECT_NE(to_hex(std::span<const uint8_t>(ab.data(), ab.size())),
            to_hex(std::span<const uint8_t>(ba.data(), ba.size())));
}
