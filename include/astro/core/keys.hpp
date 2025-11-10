#pragma once
#include <vector>
#include <cstdint>
#include <string>
#include <span>

#include <astro/core/hash.hpp>

namespace astro::core {
  /**
  * Simple container for a private/public key pair in PEM encoding.
  * - pubkey_pem: PEM-encoded public key bytes
  * - privkey_pem: PEM-encoded private key bytes
  */
  struct KeyPair {
    std::vector<uint8_t> privkey_pem;
    std::vector<uint8_t> pubkey_pem;
  };

/**
 * Initialize crypto subsystem; must be called once at startup.
 * Returns true on success.
 */
 bool crypto_init();
 
 /**
  * Cleanup crypto subsystem resources; optional but recommended before process exit.
  */
 void crypto_shutdown();

/**
 * Generate a new EC keypair using the named curve (e.g., "secp256k1").
 * Throws std::runtime_error on failure.
 */ 
 KeyPair generate_ec_keypair(const std::string& curve_name = "secp256k1");


/**
 * Sign raw message bytes using the private key in PEM.
 * Produces a DER-encoded signature (ECDSA, SHA-256 digest).
 */
 std::vector<uint8_t> sign_message(const std::vector<uint8_t>& privkey_pem,
  std::span<const uint8_t> message);


/**
 * Verify a signature against a public key and message.
 * Returns true if valid.
 */
 bool verify_message(const std::vector<uint8_t>& pubkey_pem,
  std::span<const uint8_t> message, std::span<const uint8_t> signature);

/**
 * Convenience overloads for std::string
 */
  inline std::vector<uint8_t> sign_message(const std::vector<uint8_t>& privkey_pem,
    const std::string& message) {
    return sign_message(privkey_pem, std::span<const uint8_t>(
      reinterpret_cast<const uint8_t*>(message.data()), message.size()));
  }

  inline bool verify_message(const std::vector<uint8_t>& pubkey_pem,
    const std::string& message, std::span<const uint8_t> signature) {
    return verify_message(pubkey_pem, std::span<const uint8_t>(
      reinterpret_cast<const uint8_t*>(message.data()), message.size()), signature);
  }
}