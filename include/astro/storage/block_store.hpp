#pragma once
#include <cstdint>
#include <vector>
#include <filesystem>
#include <astro/core/block.hpp>

namespace astro::storage {
  struct RecordHeader {
    uint32_t magic;
    uint64_t version;
    uint16_t kind;
    uint64_t length;
  };

  class BlockStore {
    public:
      explicit BlockStore(std::filesystem::path root_path);
      ~BlockStore();

      void append_block(const astro::core::Block& block);

      std::vector<astro::core::Block> load_all_blocks();

      const std::filesystem::path& directory() const { return root_path_; }
      const std::filesystem::path& log_path() const { return log_path_; }

      private:
        void open_write_log();
        void close_write_log();
        void fsync_fd();
        std::filesystem::path root_path_;
        std::filesystem::path log_path_;
        int log_fd = -1;
  };
}