#include <iostream>
#include <filesystem>
#include <span>

#include "astro/storage/block_store.hpp"
#include "astro/core/chain.hpp"
#include "astro/core/keys.hpp"
#include "astro/core/hash.hpp"

using namespace astro::core;
namespace fs = std::filesystem;

int main() {
  if (!crypto_init()) { std::cerr << "OpenSSL init failed\n"; return 1; }

  fs::path data = "./data";
  astro::storage::BlockStore store(data);

  Chain chain(ChainConfig{.difficulty_bits=0});
  chain.restore_from_store(store);
  std::cout << "[💾] restored height: " << chain.height() << "\n";

  if (chain.height() == 0) {
    auto genesis_block = make_genesis_block("Astro: Persisted.", 1700000000ULL);
    auto validation_result = chain.append_and_store(genesis_block, store);
    std::cout << (validation_result.is_valid ? "[+] wrote genesis\n" : "[x] failed genesis\n");
  } else {
    auto key_pair = generate_ec_keypair();
    Transaction transaction; transaction.version=1; transaction.nonce=chain.height(); transaction.amount=1;
    transaction.from_pub_pem=key_pair.pubkey_pem; transaction.to_label="demo"; transaction.sign(key_pair.privkey_pem);
    auto new_block = chain.build_block_from_transactions({transaction}, 1700000000ULL + chain.height());
    auto validation_result = chain.append_and_store(new_block, store);
    std::cout << (validation_result.is_valid ? "[+] appended block\n" : "[x] append failed\n");
  }

  auto tip_hash = chain.tip_hash();
  if (tip_hash) {
    std::cout << "tip: " << to_hex(std::span<const uint8_t>(tip_hash->data(), tip_hash->size())).substr(0,16) << "...\n";
  }
  return 0;
}