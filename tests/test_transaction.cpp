#include <gtest/gtest.h>
#include "astro/core/transaction.hpp"
#include "astro/core/keys.hpp"
#include "astro/core/hash.hpp"
#include "astro/core/serializer.hpp"

using namespace astro::core;

TEST(Transaction, RoundTripAndVerify) {
  ASSERT_TRUE(crypto_init());
  auto kp = generate_ec_keypair();

  Transaction tx;
  tx.version = 1;
  tx.nonce   = 42;
  tx.amount  = 1000;
  tx.from_pub_pem = kp.pubkey_pem;   // PEM bytes
  tx.to_label     = "alice";

  // hash before signing is stable
  auto h1 = tx.tx_hash();

  // sign
  tx.sign(kp.privkey_pem);
  ASSERT_FALSE(tx.signature.empty());
  EXPECT_TRUE(tx.verify());

  // serialize/deserialize round-trip
  auto bytes = tx.serialize(false);
  // For now we don't have a full deserialize API—use ByteReader to spot-check
  ByteReader r(bytes);
  EXPECT_EQ(r.read_u8(), 0xA1);
  EXPECT_EQ(r.read_u8(), 0x01);
  EXPECT_EQ(r.read_u32(), 1u);
  EXPECT_EQ(r.read_u32(), 1u);
  EXPECT_EQ(r.read_u64(), 42u);
  EXPECT_EQ(r.read_u64(), 1000u);
  auto pub = r.read_bytes();
  EXPECT_FALSE(pub.empty());
  auto to  = r.read_string();
  EXPECT_EQ(to, "alice");
  auto sig = r.read_bytes();
  EXPECT_FALSE(sig.empty());

  // any byte flip should break verification
  bytes.back() ^= 0x01;
  ByteReader r2(bytes);
  r2.read_u8(); r2.read_u8(); r2.read_u32(); r2.read_u32(); r2.read_u64(); r2.read_u64();
  auto pub2 = r2.read_bytes();
  auto to2  = r2.read_string();
  auto sig2 = r2.read_bytes();

  Transaction tampered;
  tampered.version = 1;
  tampered.nonce = 42;
  tampered.amount = 1000;
  tampered.from_pub_pem = pub2;
  tampered.to_label = to2;
  tampered.signature = sig2;

  EXPECT_FALSE(tampered.verify());
}

TEST(Transaction, HashDeterminism) {
  ASSERT_TRUE(crypto_init());
  auto kp = generate_ec_keypair();

  Transaction a, b;
  a.version = b.version = 1;
  a.nonce = b.nonce = 7;
  a.amount = b.amount = 55;
  a.from_pub_pem = b.from_pub_pem = kp.pubkey_pem;
  a.to_label = b.to_label = "bob";

  auto hA = a.tx_hash();
  auto hB = b.tx_hash();
  EXPECT_EQ(to_hex(std::span<const uint8_t>(hA.data(), hA.size())),
            to_hex(std::span<const uint8_t>(hB.data(), hB.size())));
}