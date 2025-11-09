#include "astro/core/block.hpp"
#include "astro/core/serializer.hpp"
#include "astro/core/hash.hpp"
#include "astro/core/merkle.hpp"

#include <cstring>

namespace astro::core {

  std::vector<uint8_t> BlockHeader::serialize() const {
    ByteWriter writer;
    writer.write_u32(version);

    writer.write_raw(std::span<const uint8_t>(prev_hash.data(), prev_hash.size()));

    writer.write_raw(std::span<const uint8_t>(merkle_root.data(), merkle_root.size()));

    writer.write_u64(timestamp);
    writer.write_u64(nonce);
    return writer.take();
  }

  Hash256 BlockHeader::hash() const {
    auto header_bytes = serialize();
    return sha256(std::span<const uint8_t>(header_bytes.data(), header_bytes.size()));
  }

  std::vector<uint8_t> Block::serialize() const {
    ByteWriter writer;

    auto header_bytes = header.serialize();
    writer.write_raw(std::span<const uint8_t>(header_bytes.data(), header_bytes.size()));

    writer.write_u32(static_cast<uint32_t>(transactions.size()));
    for (const auto& tx : transactions) {
      auto tx_bytes = tx.serialize(false);
      writer.write_u32(static_cast<uint32_t>(tx_bytes.size()));
      writer.write_raw(std::span<const uint8_t>(tx_bytes.data(), tx_bytes.size()));
    }
    return writer.take();
  }

  Hash256 empty_merkle_root() {
    const uint8_t* empty_hash = nullptr;
    return sha256(std::span<const uint8_t>(empty_hash, static_cast<size_t>(0)));
  }

  Hash256 compute_merkle_root(const std::vector<Transaction>& transactions) {
   std::vector<Hash256> leaves;
   leaves.reserve(transactions.size());
   for (const auto& tx : transactions) leaves.push_back(tx.tx_hash());
   return root(leaves);
  }

  Block make_genesis_block(std::string genesis_note, uint64_t unix_time) {
    Transaction coinbase;
    coinbase.version = 1;
    coinbase.nonce = 0;
    coinbase.amount = 0;
    coinbase.from_pub_pem = {};
    coinbase.to_label = std::move(genesis_note);
    coinbase.signature = {};

    Block block;
    block.transactions = {coinbase};

    BlockHeader header;
    header.version = 1;
    header.prev_hash = {};
    header.merkle_root = compute_merkle_root(block.transactions);
    header.timestamp = unix_time;
    header.nonce = 0;

    block.header = header;
    return block;
  }

  static bool is_zero_hash(const Hash256& hash) {
    for (auto c : hash) if (c != 0) return false;
    return true;
  }

  bool basic_block_sanity(const Block& block, bool is_genesis) {
    if (block.header.merkle_root != compute_merkle_root(block.transactions)) return false;

    if (is_genesis) {
      if (!is_zero_hash(block.header.prev_hash)) return false;
    }
    return true;
  }
}