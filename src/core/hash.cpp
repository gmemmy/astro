#include <astro/core/hash.hpp>
#include <openssl/evp.h>
#include <span>

namespace astro::core {

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
}