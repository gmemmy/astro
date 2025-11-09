#include <gtest/gtest.h>
#include "astro/core/serializer.hpp"
#
using namespace astro::core;
#
TEST(Serializer, WriteRaw_NoLengthPrefix) {
  ByteWriter writer;
  writer.write_u32(0x01020304u);
  std::vector<uint8_t> raw{0xAA, 0xBB, 0xCC};
  writer.write_raw(std::span<const uint8_t>(raw.data(), raw.size()));
  std::vector<uint8_t> pref{0x10, 0x20};
  writer.write_bytes(std::span<const uint8_t>(pref.data(), pref.size()));
#
  auto out = writer.buffer();
  // Expect: [04 03 02 01] + [AA BB CC] + [02 00 00 00] + [10 20]
  std::vector<uint8_t> expected{
    0x04, 0x03, 0x02, 0x01,
    0xAA, 0xBB, 0xCC,
    0x02, 0x00, 0x00, 0x00,
    0x10, 0x20
  };
  ASSERT_EQ(out.size(), expected.size());
  EXPECT_TRUE(std::equal(out.begin(), out.end(), expected.begin(), expected.end()));
}
