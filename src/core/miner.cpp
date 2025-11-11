#include "astro/core/miner.hpp"
#include "astro/core/pow.hpp"
#include "astro/core/hash.hpp"
#include <span>
#include <chrono>
#include <cstdint>


namespace astro::core {
  
  Block mine_block(const Chain& chain, std::vector<Transaction> transactions, 
                  uint32_t difficulty_bits, std::atomic<bool>& cancel_flag,
                  MinerProgressCallback on_progress, uint64_t tick_every_ms) {
        
    uint64_t now = static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count()
    );
    Block block = chain.build_block_from_transactions(std::move(transactions), now); 

    uint64_t attempts = 0;
    uint64_t last_transaction_bump = 0;

    for (uint64_t nonce = 0; !cancel_flag; ++nonce) {
      block.header.nonce = nonce;
      auto hash = block.header.hash();
      auto leading_zeros = pow::leading_zero_bits(hash);

      if (leading_zeros >= difficulty_bits) {
        // Found a valid block
        return block;
      }

      if (on_progress && (++attempts % tick_every_ms == 0)) {
        on_progress(attempts, leading_zeros, to_hex(std::span<const uint8_t>(hash.data(), hash.size())));
      }

      if (nonce % 1000000 == 0) {
        uint64_t new_timestamp = static_cast<uint64_t>(
          std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
          ).count()
        );
        if (new_timestamp > last_transaction_bump) {
          block.header.timestamp = new_timestamp;
          last_transaction_bump = new_timestamp;
        }
      }
    }

    throw std::runtime_error("Mining cancelled");
  }
}