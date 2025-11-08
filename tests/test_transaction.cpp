#include <gtest/gtest.h>
#include "astro/core/transaction.hpp"
#include "astro/core/keys.hpp"
#include "astro/core/hash.hpp"
#include "astro/core/serializer.hpp"

using namespace astro::core;

TEST(Transaction, RoundTripAndVerify) {
  ASSERT_TRUE(crypto_init());
  auto key_pair = generate_ec_keypair();

  Transaction transaction;
  transaction.version = 1;
  transaction.nonce   = 42;
  transaction.amount  = 1000;
  transaction.from_pub_pem = key_pair.pubkey_pem;   // PEM bytes
  transaction.to_label     = "alice";

  // hash before signing is stable
  auto tx_hash_1 = transaction.tx_hash();

  // sign
  transaction.sign(key_pair.privkey_pem);
  ASSERT_FALSE(transaction.signature.empty());
  EXPECT_TRUE(transaction.verify());

  // serialize/deserialize round-trip
  auto serialized_bytes = transaction.serialize(false);
  // For now we don't have a full deserialize API—use ByteReader to spot-check
  ByteReader reader(serialized_bytes);
  EXPECT_EQ(reader.read_u8(), 0xA1);
  EXPECT_EQ(reader.read_u8(), 0x01);
  EXPECT_EQ(reader.read_u32(), 1u);
  EXPECT_EQ(reader.read_u32(), 1u);
  EXPECT_EQ(reader.read_u64(), 42u);
  EXPECT_EQ(reader.read_u64(), 1000u);
  auto pubkey_bytes = reader.read_bytes();
  EXPECT_FALSE(pubkey_bytes.empty());
  auto to_label  = reader.read_string();
  EXPECT_EQ(to_label, "alice");
  auto signature_bytes = reader.read_bytes();
  EXPECT_FALSE(signature_bytes.empty());

  // any byte flip should break verification
  serialized_bytes.back() ^= 0x01;
  ByteReader reader2(serialized_bytes);
  reader2.read_u8(); reader2.read_u8(); reader2.read_u32(); reader2.read_u32(); reader2.read_u64(); reader2.read_u64();
  auto pubkey_bytes_2 = reader2.read_bytes();
  auto to_label_2  = reader2.read_string();
  auto signature_bytes_2 = reader2.read_bytes();

  Transaction tampered_transaction;
  tampered_transaction.version = 1;
  tampered_transaction.nonce = 42;
  tampered_transaction.amount = 1000;
  tampered_transaction.from_pub_pem = pubkey_bytes_2;
  tampered_transaction.to_label = to_label_2;
  tampered_transaction.signature = signature_bytes_2;

  EXPECT_FALSE(tampered_transaction.verify());
}

TEST(Transaction, HashDeterminism) {
  ASSERT_TRUE(crypto_init());
  auto key_pair2 = generate_ec_keypair();

  Transaction tx_a, tx_b;
  tx_a.version = tx_b.version = 1;
  tx_a.nonce = tx_b.nonce = 7;
  tx_a.amount = tx_b.amount = 55;
  tx_a.from_pub_pem = tx_b.from_pub_pem = key_pair2.pubkey_pem;
  tx_a.to_label = tx_b.to_label = "bob";

  auto tx_hash_a = tx_a.tx_hash();
  auto tx_hash_b = tx_b.tx_hash();
  EXPECT_EQ(to_hex(std::span<const uint8_t>(tx_hash_a.data(), tx_hash_a.size())),
            to_hex(std::span<const uint8_t>(tx_hash_b.data(), tx_hash_b.size())));
}