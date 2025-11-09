#include <gtest/gtest.h>
#include "astro/core/block.hpp"
#include "astro/core/hash.hpp"
#include <cstring>
#
using namespace astro::core;
#
static uint32_t read_le_u32(const std::vector<uint8_t>& buf, size_t offset) {
  return static_cast<uint32_t>(buf[offset]) |
         (static_cast<uint32_t>(buf[offset+1]) << 8) |
         (static_cast<uint32_t>(buf[offset+2]) << 16) |
         (static_cast<uint32_t>(buf[offset+3]) << 24);
}
#
static uint64_t read_le_u64(const std::vector<uint8_t>& buf, size_t offset) {
  uint64_t v = 0;
  for (int i = 0; i < 8; ++i) v |= (static_cast<uint64_t>(buf[offset + i]) << (8 * i));
  return v;
}
#
TEST(BlockHeaderSerialize, UsesRaw32ByteHashes_NoLengthPrefix) {
  BlockHeader header;
  header.version = 0x01020304u;
  for (size_t i = 0; i < header.prev_hash.size(); ++i) header.prev_hash[i] = static_cast<uint8_t>(i);
  for (size_t i = 0; i < header.merkle_root.size(); ++i) header.merkle_root[i] = static_cast<uint8_t>(0xFF - i);
  header.timestamp = 0x0102030405060708ULL;
  header.nonce = 0xA1A2A3A4A5A6A7A8ULL;
#
  auto bytes = header.serialize();
  ASSERT_EQ(bytes.size(), 84u); // 4 + 32 + 32 + 8 + 8
#
  EXPECT_EQ(read_le_u32(bytes, 0), header.version);
  EXPECT_TRUE(std::equal(bytes.begin() + 4, bytes.begin() + 36, header.prev_hash.begin()));
  EXPECT_TRUE(std::equal(bytes.begin() + 36, bytes.begin() + 68, header.merkle_root.begin()));
  EXPECT_EQ(read_le_u64(bytes, 68), header.timestamp);
  EXPECT_EQ(read_le_u64(bytes, 76), header.nonce);
}
#
TEST(BlockSerialize, HeaderIsRawPrefix_AndTxsLengthPrefixed) {
  // Build a header and two simple transactions (no signatures needed)
  Block block;
  block.header.version = 2;
  block.header.prev_hash = {};
  block.header.merkle_root = {};
  block.header.timestamp = 123456789ULL;
  block.header.nonce = 42ULL;
#
  Transaction tx1;
  tx1.version = 1; tx1.nonce = 1; tx1.amount = 10;
  tx1.from_pub_pem = {}; tx1.to_label = "a";
#
  Transaction tx2;
  tx2.version = 1; tx2.nonce = 2; tx2.amount = 20;
  tx2.from_pub_pem = {}; tx2.to_label = "bb";
#
  block.transactions = {tx1, tx2};
#
  auto header_bytes = block.header.serialize();
  auto tx1_bytes = tx1.serialize(false);
  auto tx2_bytes = tx2.serialize(false);
#
  auto block_bytes = block.serialize();
#
  // Prefix must equal header bytes exactly
  ASSERT_GE(block_bytes.size(), header_bytes.size() + 4u);
  EXPECT_TRUE(std::equal(block_bytes.begin(), block_bytes.begin() + header_bytes.size(), header_bytes.begin()));
#
  // Next u32 is number of txs
  size_t offset = header_bytes.size();
  ASSERT_EQ(read_le_u32(block_bytes, offset), 2u);
  offset += 4;
#
  // First tx
  ASSERT_EQ(read_le_u32(block_bytes, offset), tx1_bytes.size());
  offset += 4;
  ASSERT_TRUE(std::equal(block_bytes.begin() + offset, block_bytes.begin() + offset + tx1_bytes.size(), tx1_bytes.begin()));
  offset += tx1_bytes.size();
#
  // Second tx
  ASSERT_EQ(read_le_u32(block_bytes, offset), tx2_bytes.size());
  offset += 4;
  ASSERT_TRUE(std::equal(block_bytes.begin() + offset, block_bytes.begin() + offset + tx2_bytes.size(), tx2_bytes.begin()));
  offset += tx2_bytes.size();
#
  // Tail consumed
  EXPECT_EQ(offset, block_bytes.size());
}
#
TEST(BlockSerialize, ZeroTransactions) {
  Block block;
  block.header.version = 1;
  block.header.prev_hash = {};
  block.header.merkle_root = {};
  block.header.timestamp = 0;
  block.header.nonce = 0;
  block.transactions = {};
#
  auto header_bytes = block.header.serialize();
  auto block_bytes = block.serialize();
#
  ASSERT_EQ(block_bytes.size(), header_bytes.size() + 4u);
  EXPECT_TRUE(std::equal(block_bytes.begin(), block_bytes.begin() + header_bytes.size(), header_bytes.begin()));
  EXPECT_EQ(read_le_u32(block_bytes, header_bytes.size()), 0u);
}
