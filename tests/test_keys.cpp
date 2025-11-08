#include <gtest/gtest.h>
#include "astro/core/keys.hpp"

using namespace astro::core;

TEST(KeysRoundTrip, SignVerifyOK) {
    ASSERT_TRUE(crypto_init());
    auto key_pair = generate_ec_keypair();
    std::string message = "astro test message";
    auto signature = sign_message(key_pair.privkey_pem, message);
    EXPECT_TRUE(verify_message(key_pair.pubkey_pem, message, signature));
}

TEST(KeysTamper, VerifyFailsOnWrongMsg) {
    ASSERT_TRUE(crypto_init());
    auto key_pair = generate_ec_keypair();
    std::string message = "astro test message";
    auto signature = sign_message(key_pair.privkey_pem, message);
    EXPECT_FALSE(verify_message(key_pair.pubkey_pem, "different message", signature));
}

TEST(KeysAltCurve, SignVerify_Prime256v1) {
  ASSERT_TRUE(crypto_init());
  auto key_pair = generate_ec_keypair("prime256v1");
  const std::string message = "curve test";
  auto signature = sign_message(key_pair.privkey_pem, message);
  EXPECT_TRUE(verify_message(key_pair.pubkey_pem, message, signature));
}

TEST(KeysMismatch, VerifyFailsWithDifferentPublicKey) {
  ASSERT_TRUE(crypto_init());
  auto key_pair_1 = generate_ec_keypair();
  auto key_pair_2 = generate_ec_keypair();
  const std::string message = "astro mismatch";
  auto signature = sign_message(key_pair_1.privkey_pem, message);
  EXPECT_FALSE(verify_message(key_pair_2.pubkey_pem, message, signature));
}

TEST(KeysTamperSignature, VerifyFailsWhenSignatureAltered) {
  ASSERT_TRUE(crypto_init());
  auto key_pair = generate_ec_keypair();
  const std::string message = "astro tamper";
  auto signature = sign_message(key_pair.privkey_pem, message);
  ASSERT_FALSE(signature.empty());
  signature[0] ^= 0x01; // flip one bit
  EXPECT_FALSE(verify_message(key_pair.pubkey_pem, message, signature));
}

TEST(KeysInvalidPEM, SignThrowsOnInvalidPrivateKey) {
  ASSERT_TRUE(crypto_init());
  std::vector<uint8_t> bogus_priv{ 'n','o','t','-','a','-','k','e','y' };
  const std::string message = "astro invalid";
  EXPECT_THROW({ auto _ = sign_message(bogus_priv, message); (void)_; }, std::runtime_error);
}

TEST(KeysInvalidSig, VerifyFalseOnTruncatedSignature) {
  ASSERT_TRUE(crypto_init());
  auto key_pair = generate_ec_keypair();
  const std::string message = "astro trunc";
  auto signature = sign_message(key_pair.privkey_pem, message);
  ASSERT_GE(signature.size(), static_cast<size_t>(2));
  signature.resize(signature.size() / 2);
  EXPECT_FALSE(verify_message(key_pair.pubkey_pem, message, signature));
}

TEST(CryptoInit, IdempotentCallsSucceed) {
  EXPECT_TRUE(crypto_init());
  EXPECT_TRUE(crypto_init());
}