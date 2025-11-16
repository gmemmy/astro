#include "astro/core/chain.hpp"
#include "astro/core/block.hpp"
#include "astro/core/pow.hpp"
#include "astro/storage/block_store.hpp"
#include <cstdint>

namespace astro::core {
  
  Chain::Chain(ChainConfig config) : config_(config) {};

  std::optional<Hash256> Chain::tip_hash() const {
    if (blocks_.empty()) return std::nullopt;
    return blocks_.back().header.hash();
  }

  const Block* Chain::block_at(size_t index) const {
    if (index >= blocks_.size()) return nullptr;
    return &blocks_[index];
  }

  static bool is_zero_hash(const Hash256& hash) {
    for (auto byte : hash) if (byte != 0) return false;
    return true;
  }

  ValidationResult Chain::validate_block(const Block& block) const {
    const bool is_genesis_candidate = blocks_.empty();

    if (is_genesis_candidate) {
      if (!is_zero_hash(block.header.prev_hash)) {
        return {false, ValidationError::NonZeroPrevHashForGenesis, ~0ull};
      }

      if (!block.transactions.empty()) {
        if (!block.transactions.front().from_pub_pem.empty()) {
          return {false, ValidationError::CoinBaseMisplaced, 0};
        }
        for (size_t i = 1; i < block.transactions.size(); ++i) {
          if (block.transactions[i].from_pub_pem.empty()) return {false, ValidationError::CoinBaseMisplaced, i};
        }
      }
    } else {
      auto parent_hash = blocks_.back().header.hash();
      if (block.header.prev_hash != parent_hash) {
        return {false, ValidationError::BadPrevLink, ~0ull};
      }

      if (block.header.timestamp < blocks_.back().header.timestamp) {
        return {false, ValidationError::NonMonotonicTimestamp, ~0ull};
      }
  
      for (size_t i = 0; i < block.transactions.size(); ++i) {
        if (block.transactions[i].from_pub_pem.empty()) return {false, ValidationError::CoinbaseInNonGenesisBlock, i};
      }
    }

    auto computed_merkle_root = compute_merkle_root(block.transactions);
    if (computed_merkle_root!= block.header.merkle_root) {
      return {false, ValidationError::BadMerkleRoot, ~0ull};
    }

    for (size_t i = 0; i < block.transactions.size(); ++i) {
      const auto& tx =  block.transactions[i];
      // Skip signature verification for an allowed coinbase at genesis (empty from_pub_pem)
      if (is_genesis_candidate && i == 0 && tx.from_pub_pem.empty()) continue;
      if (!tx.verify()) {
        return {false, ValidationError::BadTransactionSignature, i};
      }
    }

    if (config_.difficulty_bits > 0) {
      if (is_genesis_candidate && !config_.enforce_genesis_pow) {
        // No POW check for genesis block if not enforced
      } else {
        auto header_hash = block.header.hash();
        if (!pow::meets_difficulty(config_.difficulty_bits, header_hash)) {
          return {false, ValidationError::InsufficientPOW, ~0ull};
        }
      }
    }
    return {true, ValidationError::None, ~0ull};
  }

  ValidationResult Chain::append_block(const Block& block) {
    auto validation_result = validate_block(block);
    if (!validation_result.is_valid) return validation_result;
    blocks_.push_back(block);
    return validation_result;
  }

  void Chain::restore_from_store(astro::storage::BlockStore& store) {
    auto stored_blocks = store.load_all_blocks();
    for (const auto& block : stored_blocks) {
      auto validation_result = append_block(block);
      if (!validation_result.is_valid) break;
    }
  }

  ValidationResult Chain::append_and_store(const Block& block, astro::storage::BlockStore& store) {
    auto validation_result = validate_block(block);
    if (!validation_result.is_valid) return validation_result;
    try {
      store.append_block(block);
    } catch (...) {
      return {false, ValidationError::None, ~0ull};
    }
    blocks_.push_back(block);
    return validation_result;
  }

  Block Chain::build_block_from_transactions(std::vector<Transaction> transactions, uint64_t timestamp) const {
    Block output;
    output.transactions = std::move(transactions);
    BlockHeader header;
    header.version = 1;
    if (!blocks_.empty()) header.prev_hash = blocks_.back().header.hash();
    header.merkle_root = compute_merkle_root(output.transactions);
    header.timestamp = timestamp;
    header.nonce = 0;
    output.header = header;
    return output;
  }


}