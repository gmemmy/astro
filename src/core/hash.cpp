#include <astro/core/hash.hpp>
#include <openssl/evp.h>
#include <span>
#include <sstream>
#include <iomanip>
#include <vector>

namespace astro::core {
  auto toHex(std::span<const uint8_t> data) -> std::string {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (auto b : data) oss << std::setw(2) << static_cast<int>(b);
    return oss.str();
  }

  auto sha256(std::span<const uint8_t> data) -> Hash256 {
    Hash256 out{};
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return out;
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
      EVP_MD_CTX_free(ctx);
      return out;
    }
    if (EVP_DigestUpdate(ctx, data.data(), data.size()) != 1) {
      EVP_MD_CTX_free(ctx);
      return out;
    }
    unsigned int len = static_cast<unsigned int>(out.size());
    if (EVP_DigestFinal_ex(ctx, out.data(), &len) != 1) {
      EVP_MD_CTX_free(ctx);
      return Hash256{};
    }
    EVP_MD_CTX_free(ctx);
    return out;
  }

  auto hash160(std::span<const uint8_t> data) -> Hash160 {
    const auto sha = sha256(data);
    Hash160 out{};
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return out;
    if (EVP_DigestInit_ex(ctx, EVP_ripemd160(), nullptr) != 1) {
      EVP_MD_CTX_free(ctx);
      return out;
    }
    if (EVP_DigestUpdate(ctx, sha.data(), sha.size()) != 1) {
      EVP_MD_CTX_free(ctx);
      return out;
    }
    unsigned int len = static_cast<unsigned int>(out.size());
    if (EVP_DigestFinal_ex(ctx, out.data(), &len) != 1) {
      EVP_MD_CTX_free(ctx);
      return Hash160{};
    }
    EVP_MD_CTX_free(ctx);
    return out;
  }

  auto hash_concat(std::span<const uint8_t> left, std::span<const uint8_t> right) -> Hash256 {
    std::vector<uint8_t> combined;
    combined.reserve(left.size() + right.size());
    combined.insert(combined.end(), left.begin(), left.end());
    combined.insert(combined.end(), right.begin(), right.end());
    return sha256(std::span<const uint8_t>(combined.data(), combined.size()));
  }
}