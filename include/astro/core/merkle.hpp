#pragma once
#include <vector>
#include <span>
#include <cstdint>
#include <astro/core/hash.hpp>

namespace astro::core {
  
  struct ProofStep {
    Hash256 sibling;
    bool sibling_on_left;
  };

  struct MerkleProof {
    std::vector<ProofStep> steps;
  };

  Hash256 root(const std::vector<Hash256>& leaves);

  MerkleProof build_proof(const std::vector<Hash256>& leaves, size_t index);

  bool verify_proof(std::span<const uint8_t> leaf_hash, const MerkleProof& proof, const Hash256& expected_root);

  inline bool verify_proof(const Hash256& leaf_hash, const MerkleProof& proof, const Hash256& root) {
    return verify_proof(std::span<const uint8_t>(leaf_hash.data(), leaf_hash.size()), proof, root);
  }
}