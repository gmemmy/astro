#include <gtest/gtest.h>
#include "astro/core/block.hpp"
#include "astro/core/keys.hpp"
#include "astro/core/hash.hpp"

using namespace astro::core;

TEST(BlockHeader, SerializeAndHashDeterminism) {
  BlockHeader header;
  header.version = 1;
  header.prev_hash = {}; 
  header.merkle_root = {};
  header.timestamp = 1700000000ULL;
  header.nonce = 123;

  auto header_hash_1 = header.hash();
  auto header_hash_2 = header.hash();
  EXPECT_EQ(to_hex(std::span<const uint8_t>(header_hash_1.data(), header_hash_1.size())),
            to_hex(std::span<const uint8_t>(header_hash_2.data(), header_hash_2.size())));
}

TEST(Block, GenesisBuildsAndValidates) {
  uint64_t timestamp = 1700000000ULL;
  auto genesis_block = make_genesis_block("Astro Genesis", timestamp);

  // header basic expectations
  EXPECT_EQ(genesis_block.header.version, 1u);
  EXPECT_EQ(genesis_block.transactions.size(), 1u);
  EXPECT_TRUE(basic_block_sanity(genesis_block, /*is_genesis=*/true));

  // prev hash of genesis must be zero
  Hash256 zero{};
  EXPECT_EQ(genesis_block.header.prev_hash, zero);

  // hash must be stable
  auto genesis_hash_1 = genesis_block.header.hash();
  auto genesis_hash_2 = genesis_block.header.hash();
  EXPECT_EQ(to_hex(std::span<const uint8_t>(genesis_hash_1.data(), genesis_hash_1.size())),
            to_hex(std::span<const uint8_t>(genesis_hash_2.data(), genesis_hash_2.size())));
}

TEST(Block, MerkleRootFromTransactions) {
  ASSERT_TRUE(crypto_init());
  auto key_pair = generate_ec_keypair();

  Transaction tx_a;
  tx_a.version = 1; tx_a.nonce = 1; tx_a.amount = 10;
  tx_a.from_pub_pem = key_pair.pubkey_pem; tx_a.to_label = "alice";
  tx_a.sign(key_pair.privkey_pem);

  Transaction tx_b = tx_a;
  tx_b.nonce = 2; tx_b.amount = 20;
  tx_b.sign(key_pair.privkey_pem);

  std::vector<Transaction> transactions{tx_a, tx_b};
  auto merkle_root_1 = compute_merkle_root(transactions);

  // Any change in a tx changes the root
  transactions[1].amount = 21;
  auto merkle_root_2 = compute_merkle_root(transactions);
  EXPECT_NE(to_hex(std::span<const uint8_t>(merkle_root_1.data(), merkle_root_1.size())),
            to_hex(std::span<const uint8_t>(merkle_root_2.data(), merkle_root_2.size())));
}