#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include <astro/core/keys.hpp>
#include <astro/core/hash.hpp>
#include <astro/core/transaction.hpp>
#include <astro/core/serializer.hpp>
#include <astro/core/block.hpp>
#include <astro/core/merkle.hpp>
#include <chrono>

using namespace astro::core;

static void print_usage() {
  std::printf(
    "Astro Node CLI\n\n"
    "Usage:\n"
    "  astro-node demo-keys [--curve CURVE] [--message MESSAGE]\n"
    "  astro-node demo-tx   [--amount N] [--nonce N] [--to LABEL]\n"
    "  astro-node demo-genesis\n"
    "  astro-node demo-merkle [--leaves CSV] [--index N]\n\n"
    "Options:\n"
    "  --curve    EC curve name (default: secp256k1)\n"
    "  --message  Message to sign (default: 'astro demo')\n"
    "  --amount   Transaction amount (default: 123)\n"
    "  --nonce    Transaction nonce (default: 1)\n"
    "  --to       Recipient label (default: 'demo-recipient')\n"
    "  --leaves   CSV of leaf strings (default: a,b,c,d,e)\n"
    "  --index    Leaf index for proof (default: 0)\n"
  );
}

int main(int argc, char** argv) {
  if (argc < 2) {
    print_usage();
    return 0;
  }

  const std::string command = argv[1];
  if (command == "demo-keys") {
    std::string curve = "secp256k1";
    std::string message = "astro demo";

    for (int i = 2; i < argc; ++i) {
      std::string arg = argv[i];
      if (arg.rfind("--curve=", 0) == 0) {
        curve = arg.substr(8);
      } else if (arg == "--curve" && i + 1 < argc) {
        curve = argv[++i];
      } else if (arg.rfind("--message=", 0) == 0) {
        message = arg.substr(10);
      } else if (arg == "--message" && i + 1 < argc) {
        message = argv[++i];
      } else if (arg == "-h" || arg == "--help") {
        print_usage();
        return 0;
      } else {
        std::fprintf(stderr, "Unknown option: %s\n", arg.c_str());
        print_usage();
        return 1;
      }
    }

    if (!crypto_init()) {
      std::fprintf(stderr, "crypto_init failed\n");
      return 1;
    }

    try {
      auto key_pair = generate_ec_keypair(curve);

      std::string priv_pem(key_pair.privkey_pem.begin(), key_pair.privkey_pem.end());
      std::string pub_pem(key_pair.pubkey_pem.begin(), key_pair.pubkey_pem.end());

      std::cout << "Curve: " << curve << "\n";
      std::cout << "Message: " << message << "\n\n";

      std::cout << "Private Key (PEM):\n" << priv_pem << "\n";
      std::cout << "Public Key (PEM):\n" << pub_pem << "\n";

      auto signature = sign_message(key_pair.privkey_pem, message);
      std::cout << "Signature (DER hex):\n" << toHex(signature) << "\n";

      bool verified = verify_message(key_pair.pubkey_pem, message, signature);
      std::cout << "Verification: " << (verified ? "OK" : "FAIL") << "\n";
    } catch (const std::exception& ex) {
      std::fprintf(stderr, "Error: %s\n", ex.what());
      return 1;
    }
    return 0;
  }

  if (command == "demo-tx") {
    uint64_t amount = 123;
    uint64_t nonce = 1;
    std::string to = "demo-recipient";

    for (int i = 2; i < argc; ++i) {
      std::string arg = argv[i];
      if (arg.rfind("--amount=", 0) == 0) {
        amount = std::stoull(arg.substr(9));
      } else if (arg == "--amount" && i + 1 < argc) {
        amount = std::stoull(argv[++i]);
      } else if (arg.rfind("--nonce=", 0) == 0) {
        nonce = std::stoull(arg.substr(8));
      } else if (arg == "--nonce" && i + 1 < argc) {
        nonce = std::stoull(argv[++i]);
      } else if (arg.rfind("--to=", 0) == 0) {
        to = arg.substr(5);
      } else if (arg == "--to" && i + 1 < argc) {
        to = argv[++i];
      } else if (arg == "-h" || arg == "--help") {
        print_usage();
        return 0;
      } else {
        std::fprintf(stderr, "Unknown option: %s\n", arg.c_str());
        print_usage();
        return 1;
      }
    }

    if (!crypto_init()) {
      std::fprintf(stderr, "crypto_init failed\n");
      return 1;
    }

    try {
      auto key_pair = generate_ec_keypair();

      Transaction tx;
      tx.version = 1;
      tx.nonce = nonce;
      tx.amount = amount;
      tx.from_pub_pem = key_pair.pubkey_pem; // PEM bytes
      tx.to_label = to;

      auto tx_hash = tx.tx_hash();
      tx.sign(key_pair.privkey_pem);

      std::cout << "tx.hash: " << to_hex(std::span<const uint8_t>(tx_hash.data(), tx_hash.size())) << "\n";
      std::cout << "signature.size: " << tx.signature.size() << " bytes\n";
      std::cout << "verify: " << (tx.verify() ? "OK" : "FAIL") << "\n";

      auto serialized_bytes = tx.serialize(false);
      std::cout << "serialized.len: " << serialized_bytes.size() << "\n";
    } catch (const std::exception& ex) {
      std::fprintf(stderr, "Error: %s\n", ex.what());
      return 1;
    }
    return 0;
  }

  if (command == "demo-genesis") {
    uint64_t now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::system_clock::now().time_since_epoch()).count());
    auto genesis_block = make_genesis_block("Astro: Born from bytes.", now);
    auto header_hash = genesis_block.header.hash();
    std::cout << "genesis.time: " << genesis_block.header.timestamp << "\n";
    std::cout << "genesis.hash: " << to_hex(std::span<const uint8_t>(header_hash.data(), header_hash.size())) << "\n";
    std::cout << "txs: " << genesis_block.transactions.size() << "\n";
    return 0;
  }

  if (command == "demo-merkle") {
    std::string csv = "a,b,c,d,e";
    size_t index = 0;
    for (int i = 2; i < argc; ++i) {
      std::string arg = argv[i];
      if (arg.rfind("--leaves=", 0) == 0) {
        csv = arg.substr(9);
      } else if (arg == "--leaves" && i + 1 < argc) {
        csv = argv[++i];
      } else if (arg.rfind("--index=", 0) == 0) {
        index = static_cast<size_t>(std::stoull(arg.substr(8)));
      } else if (arg == "--index" && i + 1 < argc) {
        index = static_cast<size_t>(std::stoull(argv[++i]));
      } else if (arg == "-h" || arg == "--help") {
        print_usage();
        return 0;
      } else {
        std::fprintf(stderr, "Unknown option: %s\n", arg.c_str());
        print_usage();
        return 1;
      }
    }
    std::vector<std::string> parts;
    {
      std::string cur;
      for (char c : csv) {
        if (c == ',') { parts.push_back(cur); cur.clear(); }
        else { cur.push_back(c); }
      }
      parts.push_back(cur);
    }
    if (parts.empty()) {
      std::fprintf(stderr, "No leaves provided\n");
      return 1;
    }
    std::vector<Hash256> leaves;
    leaves.reserve(parts.size());
    for (const auto& s : parts) leaves.push_back(sha256(s));
    if (index >= leaves.size()) index = leaves.size() - 1;
    auto R = root(leaves);
    std::cout << "leaves: " << leaves.size() << "\n";
    std::cout << "root:   " << to_hex(std::span<const uint8_t>(R.data(), R.size())) << "\n";
    auto proof = build_proof(leaves, index);
    std::cout << "proof.steps: " << proof.steps.size() << "\n";
    bool ok = verify_proof(leaves[index], proof, R);
    std::cout << "verify[" << index << "]: " << (ok ? "OK" : "FAIL") << "\n";
    if (!parts[index].empty()) {
      auto wrong = sha256(std::string(parts[index].size(), parts[index][0] == 'A' ? 'B' : 'A'));
      bool bad = verify_proof(wrong, proof, R);
      std::cout << "verify tampered: " << (bad ? "UNEXPECTED_OK" : "EXPECTED_FAIL") << "\n";
    }
    return 0;
  }

  print_usage();
  return 0;
}