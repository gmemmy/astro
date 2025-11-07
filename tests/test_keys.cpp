#include <gtest/gtest.h>
#include "astro/core/keys.hpp"

using namespace astro::core;

TEST(KeysRoundTrip, SignVerifyOK) {
    ASSERT_TRUE(crypto_init());
    auto kp = generate_ec_keypair();
    std::string message = "astro test message";
    auto signature = sign_message(kp.privkey_pem, message);
    EXPECT_TRUE(verify_message(kp.pubkey_pem, message, signature));
}

TEST(KeysTamper, VerifyFailsOnWrongMsg) {
    ASSERT_TRUE(crypto_init());
    auto kp = generate_ec_keypair();
    std::string message = "astro test message";
    auto signature = sign_message(kp.privkey_pem, message);
    EXPECT_FALSE(verify_message(kp.pubkey_pem, "different message", signature));
}

TEST(KeysAltCurve, SignVerify_Prime256v1) {
  ASSERT_TRUE(crypto_init());
  auto kp = generate_ec_keypair("prime256v1");
  const std::string msg = "curve test";
  auto sig = sign_message(kp.privkey_pem, msg);
  EXPECT_TRUE(verify_message(kp.pubkey_pem, msg, sig));
}

TEST(KeysMismatch, VerifyFailsWithDifferentPublicKey) {
  ASSERT_TRUE(crypto_init());
  auto kp1 = generate_ec_keypair();
  auto kp2 = generate_ec_keypair();
  const std::string msg = "astro mismatch";
  auto sig = sign_message(kp1.privkey_pem, msg);
  EXPECT_FALSE(verify_message(kp2.pubkey_pem, msg, sig));
}

TEST(KeysTamperSignature, VerifyFailsWhenSignatureAltered) {
  ASSERT_TRUE(crypto_init());
  auto kp = generate_ec_keypair();
  const std::string msg = "astro tamper";
  auto sig = sign_message(kp.privkey_pem, msg);
  ASSERT_FALSE(sig.empty());
  sig[0] ^= 0x01; // flip one bit
  EXPECT_FALSE(verify_message(kp.pubkey_pem, msg, sig));
}

TEST(KeysInvalidPEM, SignThrowsOnInvalidPrivateKey) {
  ASSERT_TRUE(crypto_init());
  std::vector<uint8_t> bogus_priv{ 'n','o','t','-','a','-','k','e','y' };
  const std::string msg = "astro invalid";
  EXPECT_THROW({ auto _ = sign_message(bogus_priv, msg); (void)_; }, std::runtime_error);
}

TEST(KeysInvalidSig, VerifyFalseOnTruncatedSignature) {
  ASSERT_TRUE(crypto_init());
  auto kp = generate_ec_keypair();
  const std::string msg = "astro trunc";
  auto sig = sign_message(kp.privkey_pem, msg);
  ASSERT_GE(sig.size(), static_cast<size_t>(2));
  sig.resize(sig.size() / 2);
  EXPECT_FALSE(verify_message(kp.pubkey_pem, msg, sig));
}

TEST(CryptoInit, IdempotentCallsSucceed) {
  EXPECT_TRUE(crypto_init());
  EXPECT_TRUE(crypto_init());
}