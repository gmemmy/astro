#pragma once
#include <atomic>
#include <functional>
#include "astro/core/chain.hpp"

namespace astro::core {
  using MinerProgressCallback = std::function<void(uint64_t, uint32_t, const std::string&)>;

// This attempts to mine a block from transactions for the given chain's tip.
// Mutates block header.nonce and may bump header.timestamp periodically.
// Returns a valid block that meets chain.config().difficulty_bits, or throws on cancel.
  Block mine_block(const Chain& chain, std::vector<Transaction> transactions, 
                  uint32_t difficulty_bits, std::atomic<bool>& cancel_flag,
                  MinerProgressCallback on_progress = nullptr, uint64_t tick_every_ms = 50000);
}