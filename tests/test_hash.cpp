#include <gtest/gtest.h>
#include "astro/core/hash.hpp"

using namespace astro::core;

TEST(HashTests, SHA256_KnownValue) {
    auto hash_value = sha256("hello");
    EXPECT_EQ(to_hex(hash_value), 
        "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824");
}

TEST(HashTests, Hash160_KnownValue) {
    auto hash_value = hash160("hello");
    EXPECT_EQ(to_hex(hash_value).substr(0, 10), "b6a9c8c230"); // partial match
}

TEST(HashTests, SHA256_EmptyString) {
  auto hash_value = sha256("");
  EXPECT_EQ(to_hex(hash_value),
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST(HashTests, Hash160_FullKnownValue_Hello) {
  auto hash_value = hash160("hello");
  EXPECT_EQ(to_hex(hash_value), "b6a9c8c230722b7c748331a8b450f05566dc7d0f");
}

TEST(HashTests, Hash160_EmptyString) {
  auto hash_value = hash160("");
  EXPECT_EQ(to_hex(hash_value), "b472a266d0bd89c13706a4132ccfb16f7c3b9fcb");
}

TEST(HashTests, ToHex_FormatsLeadingZeros) {
  std::vector<uint8_t> data{0x00, 0x01, 0x0A, 0xFF};
  EXPECT_EQ(toHex(data), "00010aff");
}