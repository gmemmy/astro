#include "astro/core/merkle.hpp"
#include "astro/core/hash.hpp"
#include <cassert>
#include <cstring>

namespace astro::core {

  static Hash256 hash_pair(std::span<const uint8_t> left, std::span<const uint8_t> right) {
    return hash_concat(left, right);
  }

  static Hash256 empty_root() {
    const uint8_t* empty_hash = nullptr;
    return sha256(std::span<const uint8_t>(empty_hash, static_cast<size_t>(0)));
  }

  Hash256 root(const std::vector<Hash256>& leaves) {
    if (leaves.empty()) return empty_root();
    if (leaves.size() == 1) {
      const Hash256& only = leaves.front();
      return hash_pair(
        std::span<const uint8_t>(only.data(), only.size()),
        std::span<const uint8_t>(only.data(), only.size())
      );
    }
    std::vector<Hash256> level_hashes = leaves;

    while (level_hashes.size() > 1) {
      std::vector<Hash256> next_level;
      next_level.reserve((level_hashes.size() + 1) / 2);
      for (size_t i = 0; i < level_hashes.size(); i += 2) {
        const Hash256& left = level_hashes[i];
        const Hash256& right = (i + 1 < level_hashes.size()) ? level_hashes[i + 1] : level_hashes[i];
        next_level.push_back(hash_pair(
          std::span<const uint8_t>(left.data(), left.size()),
          std::span<const uint8_t>(right.data(), right.size())
        ));
      }
      level_hashes.swap(next_level);
    }
    return level_hashes.front();
  }

  MerkleProof build_proof(const std::vector<Hash256>& leaves, size_t index) {
    MerkleProof proof{};
    if (leaves.empty()) return proof;
    assert(index < leaves.size());

    std::vector<Hash256> level_hashes = leaves;
    size_t proof_index = index;

    while (level_hashes.size() > 1) {
      size_t last_index = level_hashes.size() - 1;
      size_t sibling_index = (proof_index % 2 == 0) ? (proof_index + 1 <= last_index ? proof_index + 1 : proof_index) :
        (proof_index - 1);
      bool sibling_on_left = (proof_index % 2 == 1);

      proof.steps.push_back({
        level_hashes[sibling_index],
        sibling_on_left
      });

      proof_index = proof_index / 2;
      
      std::vector<Hash256> next_level;
      next_level.reserve((level_hashes.size() + 1) / 2);
      for (size_t i = 0; i < level_hashes.size(); i += 2) {
        const Hash256& left = level_hashes[i];
        const Hash256& right = (i + 1 < level_hashes.size()) ? level_hashes[i + 1] : level_hashes[i];
        next_level.push_back(hash_pair(
          std::span<const uint8_t>(left.data(), left.size()),
          std::span<const uint8_t>(right.data(), right.size())
        ));
      }
      level_hashes.swap(next_level);
    }
    return proof;
  }

  bool verify_proof(std::span<const uint8_t> leaf_hash, const MerkleProof& proof, const Hash256& expected_root) {
    Hash256 current_hash{};
    if (leaf_hash.size() == current_hash.size()) {
      std::memcpy(current_hash.data(), leaf_hash.data(), current_hash.size());
    } else {
      current_hash = sha256(leaf_hash);
    }

    if (proof.steps.empty()) {
      // Single-leaf tree: root is H(leaf || leaf)
      Hash256 dup = hash_pair(
        std::span<const uint8_t>(current_hash.data(), current_hash.size()),
        std::span<const uint8_t>(current_hash.data(), current_hash.size())
      );
      return std::equal(dup.begin(), dup.end(), expected_root.begin(), expected_root.end());
    }

    for (const auto& step : proof.steps) {
      if (step.sibling_on_left) {
        current_hash = hash_pair(
          std::span<const uint8_t>(step.sibling.data(), step.sibling.size()),
          std::span<const uint8_t>(current_hash.data(), current_hash.size())
        );
      } else {
        current_hash = hash_pair(
          std::span<const uint8_t>(current_hash.data(), current_hash.size()),
          std::span<const uint8_t>(step.sibling.data(), step.sibling.size())
        );
      }
    }
    return std::equal(current_hash.begin(), current_hash.end(), expected_root.begin(), expected_root.end());
  }
}