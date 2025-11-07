#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include <astro/core/keys.hpp>
#include <astro/core/hash.hpp>
#include <astro/core/transaction.hpp>
#include <astro/core/serializer.hpp>

using namespace astro::core;

static void print_usage() {
  std::printf(
    "Astro Node CLI\n\n"
    "Usage:\n"
    "  astro-node demo-keys [--curve CURVE] [--message MESSAGE]\n"
    "  astro-node demo-tx   [--amount N] [--nonce N] [--to LABEL]\n\n"
    "Options:\n"
    "  --curve    EC curve name (default: secp256k1)\n"
    "  --message  Message to sign (default: 'astro demo')\n"
    "  --amount   Transaction amount (default: 123)\n"
    "  --nonce    Transaction nonce (default: 1)\n"
    "  --to       Recipient label (default: 'demo-recipient')\n"
  );
}

int main(int argc, char** argv) {
  if (argc < 2) {
    print_usage();
    return 0;
  }

  const std::string cmd = argv[1];
  if (cmd == "demo-keys") {
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
      auto kp = generate_ec_keypair(curve);

      std::string priv_pem(kp.privkey_pem.begin(), kp.privkey_pem.end());
      std::string pub_pem(kp.pubkey_pem.begin(), kp.pubkey_pem.end());

      std::cout << "Curve: " << curve << "\n";
      std::cout << "Message: " << message << "\n\n";

      std::cout << "Private Key (PEM):\n" << priv_pem << "\n";
      std::cout << "Public Key (PEM):\n" << pub_pem << "\n";

      auto sig = sign_message(kp.privkey_pem, message);
      std::cout << "Signature (DER hex):\n" << toHex(sig) << "\n";

      bool ok = verify_message(kp.pubkey_pem, message, sig);
      std::cout << "Verification: " << (ok ? "OK" : "FAIL") << "\n";
    } catch (const std::exception& ex) {
      std::fprintf(stderr, "Error: %s\n", ex.what());
      return 1;
    }
    return 0;
  }

  if (cmd == "demo-tx") {
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
      auto kp = generate_ec_keypair();

      Transaction tx;
      tx.version = 1;
      tx.nonce = nonce;
      tx.amount = amount;
      tx.from_pub_pem = kp.pubkey_pem; // PEM bytes
      tx.to_label = to;

      auto h = tx.tx_hash();
      tx.sign(kp.privkey_pem);

      std::cout << "tx.hash: " << to_hex(std::span<const uint8_t>(h.data(), h.size())) << "\n";
      std::cout << "signature.size: " << tx.signature.size() << " bytes\n";
      std::cout << "verify: " << (tx.verify() ? "OK" : "FAIL") << "\n";

      auto bytes = tx.serialize(false);
      std::cout << "serialized.len: " << bytes.size() << "\n";
    } catch (const std::exception& ex) {
      std::fprintf(stderr, "Error: %s\n", ex.what());
      return 1;
    }
    return 0;
  }

  print_usage();
  return 0;
}