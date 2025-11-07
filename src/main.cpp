#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include <astro/core/keys.hpp>
#include <astro/core/hash.hpp>

using namespace astro::core;

static void print_usage() {
  std::printf(
    "Astro Node CLI\n\n"
    "Usage:\n"
    "  astro-node demo-keys [--curve CURVE] [--message MESSAGE]\n\n"
    "Options:\n"
    "  --curve    EC curve name (default: secp256k1)\n"
    "  --message  Message to sign (default: 'astro demo')\n"
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

  print_usage();
  return 0;
}