#include <gtest/gtest.h>
#include <atomic>
#include "astro/core/pow.hpp"
#include "astro/core/miner.hpp"
#include "astro/core/keys.hpp"

using namespace astro::core;

TEST(PoW, LeadingZeroBits) {
  Hash256 h{}; // all zero -> 256 bits
  EXPECT_EQ(pow::leading_zero_bits(h), 256u);
  Hash256 h2{}; h2[0] = 0x7F; // 0b01111111 -> 1 leading zero
  EXPECT_EQ(pow::leading_zero_bits(h2), 1u);
}

TEST(PoW, MineAndValidate) {
  ASSERT_TRUE(crypto_init());

  Chain c(ChainConfig{.difficulty_bits=0});
  // genesis (no PoW enforcement)
  auto g = make_genesis_block("g", 1700000000ULL);
  ASSERT_TRUE(c.append_block(g).is_valid);

  // tx
  auto kp = generate_ec_keypair();
  Transaction tx; tx.version=1; tx.nonce=1; tx.amount=1; tx.from_pub_pem=kp.pubkey_pem; tx.to_label="x"; tx.sign(kp.privkey_pem);

  // mine next block at low difficulty (fast)
  std::atomic<bool> cancel{false};
  auto mined = mine_block(c, {tx}, /*difficulty_bits=*/12, cancel, nullptr, 10'000);

  // should pass validation when difficulty is set
  c.set_difficulty_bits(12);
  auto vr = c.append_block(mined);
  EXPECT_TRUE(vr.is_valid);
}