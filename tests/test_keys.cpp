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