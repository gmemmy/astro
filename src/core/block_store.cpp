#include "astro/storage/block_store.hpp"
#include "astro/core/serializer.hpp"
#include "astro/core/hash.hpp"
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <system_error>
#include <cstring>

#ifndef _WIN32
  #include <unistd.h>
  #include <fcntl.h>
#endif

namespace fs = std::filesystem;
using namespace astro::core;

namespace astro::storage {
  static constexpr uint32_t MAGIC = 0x41535452; // "ASTR"
  static constexpr uint64_t VER = 1;
  static constexpr uint16_t KIND_BLOCK = 1;

  static Transaction parse_transaction(std::span<const uint8_t> bytes) {
    ByteReader reader(bytes);
    (void)reader.read_u8(); // 0xA1
    (void)reader.read_u8(); // 0x01
    (void)reader.read_u32(); // reserved/schema
    Transaction tx;
    tx.version = reader.read_u32();
    tx.nonce = reader.read_u64();
    tx.amount = reader.read_u64();
    tx.from_pub_pem = reader.read_bytes();
    tx.to_label = reader.read_string();
    tx.signature = reader.read_bytes();
    return tx;
  }
  BlockStore::BlockStore(fs::path root_path) : root_path_(std::move(root_path)) {
    if (!fs::exists(root_path_)) fs::create_directories(root_path_);
    log_path_ = root_path_ / "chain.log";
    open_write_log();
  }

  BlockStore::~BlockStore() { close_write_log(); }

  void BlockStore::open_write_log() {
    #ifndef _WIN32
      log_fd = ::open(log_path_.c_str(), O_CREAT | O_APPEND | O_WRONLY, 0644);
      if (log_fd < 0) throw std::system_error(errno, std::generic_category(), "open write log");
    #else
      log_fd = 1;
    #endif
  }

  void BlockStore::close_write_log() {
    #ifndef _WIN32
      if (log_fd >= 0) { ::close(log_fd); log_fd = -1; }
    #endif
  }

  void BlockStore::fsync_fd() {
    #ifndef _WIN32
      if (log_fd >= 0) ::fsync(log_fd);
    #endif
  }

  void BlockStore::append_block(const Block& block) {
    auto payload = block.serialize();
    auto check = sha256(std::span<const uint8_t>(payload.data(), payload.size()));

    RecordHeader header{MAGIC, VER, KIND_BLOCK, static_cast<uint64_t>(payload.size())};
    
    std::ofstream out(log_path_, std::ios::binary | std::ios::app);
    if (!out) throw std::runtime_error("BlockStore: open append failed");

    out.write(reinterpret_cast<const char*>(&header.magic), sizeof(header.magic));
    out.write(reinterpret_cast<const char*>(&header.version), sizeof(header.version));
    out.write(reinterpret_cast<const char*>(&header.kind), sizeof(header.kind));
    out.write(reinterpret_cast<const char*>(&header.length), sizeof(header.length));
    out.write(reinterpret_cast<const char*>(payload.data()), payload.size());
    out.write(reinterpret_cast<const char*>(check.data()), check.size());

    if (!out.good()) throw std::runtime_error("BlockStore: write failed");
    out.flush();
    fsync_fd();
  }

  static uint64_t read_u64(std::istream& in) {
    uint64_t v; in.read(reinterpret_cast<char*>(&v), sizeof(v)); return v;
  }

  static uint32_t read_u32(std::istream& in) {
    uint32_t v; in.read(reinterpret_cast<char*>(&v), sizeof(v)); return v;
  }

  static uint16_t read_u16(std::istream& in) {
    uint16_t v; in.read(reinterpret_cast<char*>(&v), sizeof(v)); return v;
  }
  
  std::vector<Block> BlockStore::load_all_blocks() {
    std::vector<Block> out;
    if (!fs::exists(log_path_)) return out;

    std::ifstream in(log_path_, std::ios::binary);
    if(!in) throw std::runtime_error("BlockStore: open read failed");

    while (in.peek() != std::char_traits<char>::eof()) {
      uint32_t magic = read_u32(in);
      if (!in) break;
      uint64_t version = read_u64(in);
      uint16_t kind = read_u16(in);
      uint64_t length = read_u64(in);
      if (!in) break;

      if (magic != MAGIC || version != VER || kind != KIND_BLOCK) {
        break;
      }

      std::vector<uint8_t> payload;
      payload.resize(length);
      in.read(reinterpret_cast<char*>(payload.data()), payload.size());
      if (!in) break;

      Hash256 check{};
      in.read(reinterpret_cast<char*>(check.data()), check.size());
      if (!in) break;

      auto calculated_check = sha256(std::span<const uint8_t>(payload.data(), payload.size()));
      if (calculated_check != check) {
        break;
      }

      // Reconstructing via serializer primitives to match Block::serialize
      ByteReader reader(payload);
      BlockHeader header;
      header.version = reader.read_u32();
      for (size_t i = 0; i < header.prev_hash.size(); ++i) header.prev_hash[i] = reader.read_u8();
      for (size_t i = 0; i < header.merkle_root.size(); ++i) header.merkle_root[i] = reader.read_u8();
      header.timestamp = reader.read_u64();
      header.nonce = reader.read_u64();

      Block block;
      block.header = header;

      auto num_txs = reader.read_u32();
      block.transactions.reserve(num_txs);
      for (uint32_t i=0; i<num_txs; ++i) {
        auto tx_bytes = reader.read_bytes();
        Transaction tx = parse_transaction(std::span<const uint8_t>(tx_bytes.data(), tx_bytes.size()));
        block.transactions.push_back(std::move(tx));
      }

      out.push_back(std::move(block));
    }
    return out;
  }
}