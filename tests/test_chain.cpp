#include <gtest/gtest.h>
#include <chrono>
#include "astro/core/chain.hpp"
#include "astro/core/keys.hpp"

using namespace astro::core;

static uint64_t now_sec() {
  return static_cast<uint64_t>(
    std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::system_clock::now().time_since_epoch()
    ).count()
  );
}

TEST(Chain, AppendGenesisThenValidBlock) {
  ASSERT_TRUE(crypto_init());
  Chain c;
  uint64_t t0 = now_sec();

  // Build genesis with a single coinbase-like tx (no signer)
  Transaction coinbase;
  coinbase.to_label = "genesis-note";
  Block g = make_genesis_block("genesis-note", t0);
  auto r0 = c.append_block(g);
  ASSERT_TRUE(r0.is_valid);
  EXPECT_EQ(c.height(), 1u);

  // Next block with a signed tx
  auto kp = generate_ec_keypair();
  ASSERT_FALSE(kp.privkey_pem.empty());
  ASSERT_FALSE(kp.pubkey_pem.empty());
  Transaction tx;
  tx.version = 1; tx.nonce = 1; tx.amount = 10;
  tx.from_pub_pem = kp.pubkey_pem; tx.to_label = "darth vader";
  tx.sign(kp.privkey_pem);

  Block b1 = c.build_block_from_transactions({tx}, t0 + 1);
  auto r1 = c.append_block(b1);
  ASSERT_TRUE(r1.is_valid);
  EXPECT_EQ(c.height(), 2u);
  EXPECT_TRUE(c.tip_hash().has_value());
}

TEST(Chain, RejectsBadPrevLink) {
  ASSERT_TRUE(crypto_init());
  Chain c;
  uint64_t t = now_sec();
  auto g = make_genesis_block("g", t);
  ASSERT_TRUE(c.append_block(g).is_valid);

  // Build a block that points to zero (wrong prev)
  auto kp = generate_ec_keypair();
  ASSERT_FALSE(kp.privkey_pem.empty());
  ASSERT_FALSE(kp.pubkey_pem.empty());
  Transaction tx; tx.version=1; tx.nonce=1; tx.amount=1; tx.from_pub_pem=kp.pubkey_pem; tx.to_label="x"; tx.sign(kp.privkey_pem);

  Block bad = c.build_block_from_transactions({tx}, t+1);
  bad.header.prev_hash = {};
  auto r = c.append_block(bad);
  EXPECT_FALSE(r.is_valid);
  EXPECT_EQ(r.error, ValidationError::BadPrevLink);
}

TEST(Chain, RejectsBadMerkleOrSig) {
  ASSERT_TRUE(crypto_init());
  Chain c;
  uint64_t t = now_sec();
  ASSERT_TRUE(c.append_block(make_genesis_block("g", t)).is_valid);

  auto kp = generate_ec_keypair();
  ASSERT_FALSE(kp.privkey_pem.empty());
  ASSERT_FALSE(kp.pubkey_pem.empty());
  Transaction tx; tx.version=1; tx.nonce=1; tx.amount=1; tx.from_pub_pem=kp.pubkey_pem; tx.to_label="x"; tx.sign(kp.privkey_pem);
  Block b = c.build_block_from_transactions({tx}, t+1);

  // Tamper merkle
  b.header.merkle_root = {};
  auto r1 = c.append_block(b);
  EXPECT_FALSE(r1.is_valid);
  EXPECT_EQ(r1.error, ValidationError::BadMerkleRoot);

  // Fix merkle, break signature
  b.header.merkle_root = compute_merkle_root(b.transactions);
  ASSERT_FALSE(b.transactions[0].signature.empty());
  b.transactions[0].signature[0] ^= 0x01;
  auto r2 = c.append_block(b);
  EXPECT_FALSE(r2.is_valid);
  EXPECT_EQ(r2.error, ValidationError::BadTransactionSignature);
}

TEST(Chain, RejectsCoinbaseInNonGenesis) {
  ASSERT_TRUE(crypto_init());
  Chain c;
  uint64_t t = now_sec();
  ASSERT_TRUE(c.append_block(make_genesis_block("g", t)).is_valid);

  Transaction cb;
  cb.to_label = "illegal";
  Block b = c.build_block_from_transactions({cb}, t+1);

  auto r = c.append_block(b);
  EXPECT_FALSE(r.is_valid);
  EXPECT_EQ(r.error, ValidationError::CoinbaseInNonGenesisBlock);
} 