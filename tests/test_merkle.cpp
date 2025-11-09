#include <gtest/gtest.h>
#include "astro/core/merkle.hpp"
#include "astro/core/hash.hpp"

using namespace astro::core;

static Hash256 hash_of(const std::string& s) {
  return sha256(s);
}

TEST(Merkle, EmptyAndSingle) {
  std::vector<Hash256> leaves;
  auto root_empty = root(leaves);
  // empty root should equal sha256("")
  const uint8_t* p=nullptr;
  auto empty = sha256(std::span<const uint8_t>(p, (size_t)0));
  EXPECT_EQ(to_hex(std::span<const uint8_t>(root_empty.data(), root_empty.size())),
            to_hex(std::span<const uint8_t>(empty.data(), empty.size())));

  leaves.push_back(hash_of("a"));
  auto root_single = root(leaves);
  // single leaf -> parent of (a,a)
  std::vector<Hash256> two{ leaves[0], leaves[0] };
  auto duplicated_pair_root = root(two);
  EXPECT_EQ(to_hex(std::span<const uint8_t>(root_single.data(), root_single.size())),
            to_hex(std::span<const uint8_t>(duplicated_pair_root.data(), duplicated_pair_root.size())));
}

TEST(Merkle, SmallSetsDeterministic) {
  std::vector<Hash256> set_two{ hash_of("a"), hash_of("b") };
  auto root_two = root(set_two);

  // reorder leaves -> different root
  std::vector<Hash256> set_two_swapped{ hash_of("b"), hash_of("a") };
  auto root_two_swapped = root(set_two_swapped);
  EXPECT_NE(to_hex(std::span<const uint8_t>(root_two.data(), root_two.size())),
            to_hex(std::span<const uint8_t>(root_two_swapped.data(), root_two_swapped.size())));

  std::vector<Hash256> set_three{ hash_of("a"), hash_of("b"), hash_of("c") };
  auto root_three = root(set_three);
  // change any leaf -> different root
  set_three[2] = hash_of("x");
  auto root_three_changed = root(set_three);
  EXPECT_NE(to_hex(std::span<const uint8_t>(root_three.data(), root_three.size())),
            to_hex(std::span<const uint8_t>(root_three_changed.data(), root_three_changed.size())));
}

TEST(Merkle, ProofBuildAndVerify) {
  std::vector<Hash256> leaves{ hash_of("a"), hash_of("b"), hash_of("c"), hash_of("d"), hash_of("e") };
  auto expected_root = root(leaves);

  for (size_t i = 0; i < leaves.size(); ++i) {
    auto proof = build_proof(leaves, i);
    EXPECT_TRUE(verify_proof(leaves[i], proof, expected_root));
  }

  // Tamper the leaf
  auto proof0 = build_proof(leaves, 0);
  auto wrong_leaf = hash_of("A");
  EXPECT_FALSE(verify_proof(wrong_leaf, proof0, expected_root));
}