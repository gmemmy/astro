#include <gtest/gtest.h>
#include <filesystem>
#include <span>
#include "astro/storage/block_store.hpp"
#include "astro/core/chain.hpp"
#include "astro/core/keys.hpp"
#include "astro/core/hash.hpp"

using namespace astro::core;
namespace fs = std::filesystem;

static fs::path tmpdir(const char* name) {
  auto p = fs::temp_directory_path() / (std::string("astro_") + name);
  fs::remove_all(p);
  fs::create_directories(p);
  return p;
}

TEST(Store, AppendRestoreRoundTrip) {
  ASSERT_TRUE(crypto_init());
  Chain c(ChainConfig{.difficulty_bits=0});
  auto dir = tmpdir("store");
  astro::storage::BlockStore store(dir);

  // genesis
  auto g = make_genesis_block("g", 1700000000ULL);
  ASSERT_TRUE(c.append_and_store(g, store).is_valid);

  // one signed block
  auto kp = generate_ec_keypair();
  Transaction tx; tx.version=1; tx.nonce=1; tx.amount=7; tx.from_pub_pem=kp.pubkey_pem; tx.to_label="x"; tx.sign(kp.privkey_pem);
  auto b1 = c.build_block_from_transactions({tx}, 1700000001ULL);
  ASSERT_TRUE(c.append_and_store(b1, store).is_valid);

  // new chain instance, restore
  Chain c2(ChainConfig{.difficulty_bits=0});
  c2.restore_from_store(store);
  EXPECT_EQ(c2.height(), 2u);
  auto tip1 = c.tip_hash();
  auto tip2 = c2.tip_hash();
  ASSERT_TRUE(tip1.has_value() && tip2.has_value());
  EXPECT_EQ(to_hex(std::span<const uint8_t>(tip1->data(), tip1->size())).substr(0,16),
            to_hex(std::span<const uint8_t>(tip2->data(), tip2->size())).substr(0,16));
}