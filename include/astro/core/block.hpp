#pragma once
#include <cstdint>
#include <vector>
#include <span>
#include <string>
#include "astro/core/hash.hpp"
#include "astro/core/transaction.hpp"

namespace astro::core {
  struct BlockHeader {
    uint32_t version = 1;
    Hash256 prev_hash{};
    Hash256 merkle_root{};
    uint64_t timestamp = 0;
    uint64_t nonce = 0;

    std::vector<uint8_t> serialize() const;

    Hash256 hash() const;
  };

  struct Block {
    BlockHeader header;
    std::vector<Transaction> transactions;

    // Serialize block as: header bytes + u32 num_txs + each tx (full, incl. signature)
    std::vector<uint8_t> serialize() const;
  };

  // ------- Utilities -------
  Hash256 compute_merkle_root(const std::vector<Transaction>& transactions);
  Hash256 empty_merkle_root();

  Block make_genesis_block(std::string genesis_note, uint64_t unix_time);

  bool basic_block_sanity(const Block& block, bool is_genesis=false);
}