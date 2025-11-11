#include <cstdint>
#include <iostream>
#include <atomic>
#include <chrono>
#include <ctime>
#include "astro/core/miner.hpp"
#include "astro/core/keys.hpp"
#include "astro/core/hash.hpp"

using namespace astro::core;

int main(int argc, char** argv) {
  if (!crypto_init()) { std::cerr << "OpenSSL init failed" << std::endl; return 1; }

  uint32_t difficulty_bits = 18;

  if (argc >= 2) difficulty_bits = static_cast<uint32_t>(std::atoi(argv[1]));

  Chain chain(ChainConfig{.difficulty_bits=0});
  auto genesis = make_genesis_block("Astro Born", static_cast<uint64_t>(std::chrono::seconds(std::time(nullptr)).count()));
  auto validation_result = chain.append_block(genesis);
  if (!validation_result.is_valid) { std::cerr << "Failed to append genesis block" << std::endl; return 2; }

  auto key_pair = generate_ec_keypair();
  Transaction transaction;
  transaction.version = 1;
  transaction.nonce = 1;
  transaction.amount = 42;
  transaction.from_pub_pem = key_pair.pubkey_pem;
  transaction.to_label = "darth vader";
  transaction.sign(key_pair.privkey_pem);

  std::atomic<bool> cancel_flag{false};
  auto time_point0 = std::chrono::steady_clock::now();
  uint64_t last_attempts = 0;

  auto on_progress = [&](uint64_t attempts, uint32_t leading_zeros, const std::string hash_hex) {
    auto delta = std::chrono::duration<double>(std::chrono::steady_clock::now() - time_point0).count();
    double rate = attempts / std::max(delta, 1e-9);
    std::cout << "\r[⚙] attempts=" << attempts
              << " lz=" << leading_zeros
              << " rate=" << static_cast<int>(rate/1000.0) << " KH/s"
              << " hash=" << hash_hex.substr(0, 10) << "..."
              << std::flush;
    last_attempts = attempts;
  };

  auto block = mine_block(chain, {transaction}, difficulty_bits, cancel_flag, on_progress, 25'000);

  auto duration = std::chrono::duration<double>(std::chrono::steady_clock::now() - time_point0).count();
  std::cout << "\n[✅] found in " << duration << "s, attempts=" << last_attempts << "\n";

  chain.set_difficulty_bits(difficulty_bits);
  auto result = chain.append_block(block);
  if (!result.is_valid) { std::cerr << "Failed to append block" << std::endl; return 3; }

  auto header_hash = block.header.hash();
  std::cout << "height: " << chain.height() << " hash: " << to_hex(std::span<const uint8_t>(header_hash.data(), header_hash.size())).substr(0, 16) << "...\n";

  return 0;
}