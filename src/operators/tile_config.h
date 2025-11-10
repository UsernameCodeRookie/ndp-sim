#ifndef TILE_CONFIG_H
#define TILE_CONFIG_H

#include <algorithm>
#include <iostream>
#include <vector>

namespace Operators {

/**
 * @brief Tile configuration for tiled operations
 */
struct TileConfig {
  size_t tile_m;  // Tile size in M dimension
  size_t tile_n;  // Tile size in N dimension
  size_t tile_k;  // Tile size in K dimension (for GEMM)

  TileConfig(size_t m = 16, size_t n = 16, size_t k = 16)
      : tile_m(m), tile_n(n), tile_k(k) {}

  void print() const {
    std::cout << "Tile Config: [M=" << tile_m << ", N=" << tile_n
              << ", K=" << tile_k << "]\n";
  }
};

/**
 * @brief Tile iterator for managing tiled computation
 */
class TileIterator {
 public:
  TileIterator(size_t total_m, size_t total_n, size_t total_k,
               const TileConfig& config)
      : total_m_(total_m),
        total_n_(total_n),
        total_k_(total_k),
        config_(config),
        m_tiles_(computeTileCount(total_m, config.tile_m)),
        n_tiles_(computeTileCount(total_n, config.tile_n)),
        k_tiles_(computeTileCount(total_k, config.tile_k)) {}

  size_t getTotalTiles() const { return m_tiles_ * n_tiles_ * k_tiles_; }
  size_t getMTiles() const { return m_tiles_; }
  size_t getNTiles() const { return n_tiles_; }
  size_t getKTiles() const { return k_tiles_; }

  // Get tile start and end indices for M dimension
  void getTileRangeM(size_t tile_idx, size_t& start, size_t& end) const {
    start = tile_idx * config_.tile_m;
    end = std::min(start + config_.tile_m, total_m_);
  }

  // Get tile start and end indices for N dimension
  void getTileRangeN(size_t tile_idx, size_t& start, size_t& end) const {
    start = tile_idx * config_.tile_n;
    end = std::min(start + config_.tile_n, total_n_);
  }

  // Get tile start and end indices for K dimension
  void getTileRangeK(size_t tile_idx, size_t& start, size_t& end) const {
    start = tile_idx * config_.tile_k;
    end = std::min(start + config_.tile_k, total_k_);
  }

  const TileConfig& getConfig() const { return config_; }

  void print() const {
    std::cout << "TileIterator: Total[" << total_m_ << "x" << total_n_ << "x"
              << total_k_ << "], Tiles[" << m_tiles_ << "x" << n_tiles_ << "x"
              << k_tiles_ << "]\n";
    config_.print();
  }

 private:
  static size_t computeTileCount(size_t total, size_t tile_size) {
    return (total + tile_size - 1) / tile_size;
  }

  size_t total_m_, total_n_, total_k_;
  TileConfig config_;
  size_t m_tiles_, n_tiles_, k_tiles_;
};

}  // namespace Operators

#endif  // TILE_CONFIG_H
