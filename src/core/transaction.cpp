#include "astro/core/transaction.hpp"
#include "astro/core/serializer.hpp"
#include "astro/core/keys.hpp"
#include <string_view>

namespace astro::core {

  std::vector<uint8_t> Transaction::serialize(bool for_signing) const {
    ByteWriter writer;

    writer.write_u8(0XA1);
    writer.write_u8(0x01);
    writer.write_u32(1);

    writer.write_u32(version);
    writer.write_u64(nonce);
    writer.write_u64(amount);

    writer.write_bytes(from_pub_pem);
    writer.write_string(to_label);

    if (!for_signing) {
      writer.write_bytes(signature);
    } else {
      writer.write_u32(0);
    }
    return writer.take();
  }

  Hash256 Transaction::tx_hash() const {
    auto tx_bytes = serialize(true);
    return sha256(std::span<const uint8_t>(tx_bytes.data(), tx_bytes.size()));
  }

  void Transaction::sign(std::span<const uint8_t> privkey_pem) {
    auto message = serialize(true);
    std::vector<uint8_t> privkey_bytes(privkey_pem.begin(), privkey_pem.end());
    signature = sign_message(privkey_bytes, std::span<const uint8_t>(message.data(), message.size()));
  }

  bool Transaction::verify() const {
    auto message = serialize(true);
    return verify_message(from_pub_pem, std::span<const uint8_t>(message.data(), message.size()), std::span<const uint8_t>(signature.data(), signature.size()));
  }
}