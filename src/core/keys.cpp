#include "astro/core/keys.hpp"
#include <openssl/evp.h>
#include <openssl/core_names.h>
#include <openssl/params.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/crypto.h>

#include <stdexcept>
#include <memory>

namespace astro::core {

  namespace  {

    void throw_openssl_error(const std::string& context) {
      unsigned long err = ERR_get_error();
      char err_buf[256]{0};
      ERR_error_string_n(err, err_buf, sizeof(err_buf));
      throw std::runtime_error(context + ": " + err_buf);
    }

    using EVP_PKEY_Ptr = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
    using EVP_PKEY_CTX_Ptr = std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>;
    using EVP_MD_CTX_Ptr = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;

    std::vector<uint8_t> export_key_pem(EVP_PKEY* key, bool is_private) {
      BIO* bio = BIO_new(BIO_s_mem());
      if (!bio) throw_openssl_error("BIO_new");
      std::unique_ptr<BIO, decltype(&BIO_free)> bio_guard(bio, &BIO_free);

      if (is_private) {
        if (PEM_write_bio_PrivateKey(bio, key, nullptr, nullptr, 0, nullptr, nullptr) != 1) {
          throw_openssl_error("PEM_write_bio_PrivateKey");
        }
      } else {
        if (PEM_write_bio_PUBKEY(bio, key) != 1) {
          throw_openssl_error("PEM_write_bio_PUBKEY");
        }
      }

      char* data = nullptr;
      long len = BIO_get_mem_data(bio, &data);
      if (len <= 0) throw_openssl_error("BIO_get_mem_data");

      return std::vector<uint8_t>(data, data + len);
    }
  }

  bool crypto_init() {
    if (OPENSSL_init_crypto(0, nullptr) != 1) {
      return false;
    }
    ERR_load_crypto_strings();
    return true;
  }
 
  void crypto_shutdown() {
    OPENSSL_cleanup();
  }

  KeyPair generate_ec_keypair(const std::string& curve_name) {
    EVP_PKEY_Ptr key_handle(nullptr, &EVP_PKEY_free);
    EVP_PKEY_CTX_Ptr keygen_ctx(nullptr, &EVP_PKEY_CTX_free);

    keygen_ctx.reset(EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr));
    if (!keygen_ctx) throw_openssl_error("EVP_PKEY_CTX_new_from_name");

    if (EVP_PKEY_keygen_init(keygen_ctx.get()) <= 0) throw_openssl_error("EVP_PKEY_keygen_init");

    OSSL_PARAM params[2] = {
      OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME, 
        const_cast<char*>(curve_name.c_str()), 0),
        OSSL_PARAM_construct_end()
    };

    if (EVP_PKEY_CTX_set_params(keygen_ctx.get(), params) <= 0) throw_openssl_error("EVP_PKEY_CTX_set_params");

    EVP_PKEY* generated_key = nullptr;
    if (EVP_PKEY_keygen(keygen_ctx.get(), &generated_key) <= 0) throw_openssl_error("EVP_PKEY_keygen");
    key_handle.reset(generated_key);

    auto priv_pem = export_key_pem(key_handle.get(), true);
    auto pub_pem = export_key_pem(key_handle.get(), false);

    return {std::move(priv_pem), std::move(pub_pem)};
  }

  std::vector<uint8_t> sign_message(const std::vector<uint8_t>& privkey_pem, 
    std::span<const uint8_t> message) {
      BIO* bio = BIO_new_mem_buf(privkey_pem.data(), static_cast<int>(privkey_pem.size()));
      std::unique_ptr<BIO, decltype(&BIO_free)> bio_guard(bio, &BIO_free);

      EVP_PKEY* raw_key = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
      EVP_PKEY_Ptr key_handle(raw_key, &EVP_PKEY_free);

      EVP_MD_CTX_Ptr sign_ctx(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
      if (!sign_ctx) throw_openssl_error("EVP_MD_CTX_new");

      if (EVP_DigestSignInit(sign_ctx.get(), nullptr, EVP_sha256(), nullptr, key_handle.get()) <= 0)
        throw_openssl_error("EVP_DigestSignInit");

      if (EVP_DigestSignUpdate(sign_ctx.get(), message.data(), message.size()) <= 0)
          throw_openssl_error("EVP_DigestSignUpdate");

      size_t sig_len = 0;
      if (EVP_DigestSignFinal(sign_ctx.get(), nullptr, &sig_len) <= 0)
        throw_openssl_error("EVP_DigestSignalFinal (get length)");

      std::vector<uint8_t> signature(sig_len);
      if (EVP_DigestSignFinal(sign_ctx.get(), signature.data(), &sig_len) <= 0)
        throw_openssl_error("EVP_DigestSignalFinal (get signature)");
      signature.resize(sig_len);

      return signature;
    };

  bool verify_message(const std::vector<uint8_t>& pubkey_pem, std::span<const uint8_t> message,
    std::span<const uint8_t> signature) {
      BIO* bio = BIO_new_mem_buf(pubkey_pem.data(), static_cast<int>(pubkey_pem.size()));
      if (!bio) throw_openssl_error("BIO_new_mem_buf");
      std::unique_ptr<BIO, decltype(&BIO_free)> bio_guard(bio, &BIO_free);

      EVP_PKEY* raw_key = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
      if (!raw_key) throw_openssl_error("PEM_read_bio_PUBKEY");
      EVP_PKEY_Ptr key_handle(raw_key, &EVP_PKEY_free);

      EVP_MD_CTX_Ptr verify_ctx(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
      if (!verify_ctx) throw_openssl_error("EVP_MD_CTX_new");

      if (EVP_DigestVerifyInit(verify_ctx.get(), nullptr, EVP_sha256(), nullptr, key_handle.get() ) <= 0)
        throw_openssl_error("EVP_DigestVerifyInit");

      if (EVP_DigestVerifyUpdate(verify_ctx.get(), message.data(), message.size()) <= 0)
        throw_openssl_error("EVP_DigestVerifyUpdate");

      int result = EVP_DigestVerifyFinal(verify_ctx.get(), signature.data(), signature.size());
      return (result == 1);
    }

  }