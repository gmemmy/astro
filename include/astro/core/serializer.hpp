#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <stdexcept>
#include <type_traits>

namespace astro::core {

  struct SerializeError : std::runtime_error {
    using std::runtime_error::runtime_error;
  };

  class ByteWriter {
    public:
      void write_u8(uint8_t value) { buffer_.push_back(value); }
      void write_u32(uint32_t value) { write_le_value(value); }
      void write_u64(uint64_t value) { write_le_value(value); }

      void write_raw(std::span<const uint8_t> bytes) {
        buffer_.insert(buffer_.end(), bytes.begin(), bytes.end());
      }

      void write_bytes(std::span<const uint8_t> bytes) {
        write_u32(static_cast<uint32_t>(bytes.size()));
        buffer_.insert(buffer_.end(), bytes.begin(), bytes.end());
      }
      void write_string(std::string_view str) {
        write_u32(static_cast<uint32_t>(str.size()));
        buffer_.insert(buffer_.end(), str.begin(), str.end());
      } 

      const std::vector<uint8_t>& buffer() const { return buffer_; }
      std::vector<uint8_t> take() { return std::move(buffer_); }

    private:
      template <class T> void write_le_value(T value) {
        for (size_t i = 0; i < sizeof(T); ++i) buffer_.push_back(static_cast<uint8_t>((value >> (8*i)) & 0xFF));
      }
      std::vector<uint8_t> buffer_;
   };

   class ByteReader {
    public:
      explicit ByteReader(std::span<const uint8_t> src) : src_(src) {}

      uint8_t read_u8() { return read_value<uint8_t>(); }
      uint32_t read_u32() { return read_value<uint32_t>(); }
      uint64_t read_u64() { return read_value<uint64_t>(); }

      std::vector<uint8_t> read_bytes() {
        auto len = read_u32();
        ensure(len <= remaining_bytes());
        std::vector<uint8_t> result(src_.begin() +pos_, src_.begin()+pos_+len);
        pos_ += len;
        return result;
      }

      std::string read_string() {
        auto len = read_u32();
        ensure(len <= remaining_bytes());
        std::string result(reinterpret_cast<const char*>(src_.data()+pos_), len);
        pos_ += len;
        return result;
      }

      size_t remaining_bytes() const { return src_.size() - pos_; }
    
    private:
      template <class T> T read_value() {
        ensure(remaining_bytes() >= sizeof(T));
        static_assert(std::is_integral_v<T> && std::is_unsigned_v<T>, "T must be unsigned integral");
        T value = 0;
        for (size_t i = 0; i < sizeof(T); ++i) {
          value |= (static_cast<T>(src_[pos_++]) << (8 * i));
        }
        return value;
      }
      void ensure(bool condition) { if (!condition) throw SerializeError("deserialize: truncated/invalid buffer");}

      std::span<const uint8_t> src_;
      size_t pos_ = 0;
   };
}