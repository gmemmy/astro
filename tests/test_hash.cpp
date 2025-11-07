#include <gtest/gtest.h>
#include "astro/core/hash.hpp"

using namespace astro::core;

TEST(HashTests, SHA256_KnownValue) {
    auto h = sha256("hello");
    EXPECT_EQ(to_hex(h), 
        "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824");
}

TEST(HashTests, Hash160_KnownValue) {
    auto h = hash160("hello");
    EXPECT_EQ(to_hex(h).substr(0, 10), "b6a9c8c230"); // partial match
}