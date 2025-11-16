#pragma once
#include <cstdint>
#include <vector>
#include <optional>
#include "astro/core/block.hpp"
#include "astro/core/transaction.hpp"

namespace astro { namespace storage { class BlockStore; } }

namespace astro::core {

  enum class ValidationError {
    None = 0,
    EmptyChainButNotGenesis,
    NonZeroPrevHashForGenesis,
    BadPrevLink,
    NonMonotonicTimestamp,
    BadMerkleRoot,
    BadTransactionSignature,
    CoinBaseMisplaced,
    CoinbaseInNonGenesisBlock,
    InsufficientPOW,
  };

  struct ValidationResult {
    bool is_valid;
    ValidationError error;
    size_t transaction_index = ~0LL;
  };

  struct ChainConfig {
    uint32_t difficulty_bits = 0;
    bool enforce_genesis_pow = false;
  };

  class Chain {
    public:
      explicit Chain(ChainConfig config = {});

      const ChainConfig& config() const { return config_;}
      void set_difficulty_bits(uint32_t bits) { config_.difficulty_bits = bits; }
      size_t height() const { return blocks_.size();}

      std::optional<Hash256> tip_hash() const;
      const Block* tip() const { return blocks_.empty() ? nullptr : &blocks_.back();}
      const Block* block_at(size_t index) const;

      ValidationResult validate_block(const Block& block) const;

      ValidationResult append_block(const Block& block);

      Block build_block_from_transactions(std::vector<Transaction> transactions, uint64_t timestamp) const;

      // Load blocks from the block store (verifies each via validate_block).
      // If the chain is empty, the first valid block becomes genesis.
      void restore_from_store(astro::storage::BlockStore& store);

      // Validate then append AND persist atomically.
      ValidationResult append_and_store(const Block& block, astro::storage::BlockStore& store);

      const std::vector<Block>& blocks() const { return blocks_; }

    private:
      ChainConfig config_{};
      std::vector<Block> blocks_;
  };
}