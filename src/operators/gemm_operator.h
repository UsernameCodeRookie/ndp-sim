#ifndef GEMM_OPERATOR_H
#define GEMM_OPERATOR_H

#include <chrono>

#include "operator_base.h"
#include "tile_config.h"

namespace Operators {

/**
 * @brief GEMM (General Matrix Multiply) Operator
 *
 * Performs C = A * B where:
 * - A is M x K matrix
 * - B is K x N matrix
 * - C is M x N matrix (output)
 *
 * Supports tiled computation for efficient memory access and parallelization
 */
template <typename T = int>
class GEMMOperator : public OperatorBase {
 public:
  GEMMOperator(const std::string& name = "GEMM")
      : OperatorBase(name), use_tiling_(false), tile_config_(16, 16, 16) {}

  /**
   * @brief Set input tensors
   */
  void setInputs(const Tensor<T>& A, const Tensor<T>& B) {
    if (A.shape().rank() != 2 || B.shape().rank() != 2) {
      throw std::runtime_error("GEMM requires 2D tensors");
    }
    if (A.shape()[1] != B.shape()[0]) {
      throw std::runtime_error(
          "Matrix dimensions don't match for multiplication");
    }

    A_ = A;
    B_ = B;

    M_ = A.shape()[0];
    K_ = A.shape()[1];
    N_ = B.shape()[1];

    // Allocate output
    C_ = Tensor<T>(TensorShape{M_, N_}, 0);
  }

  /**
   * @brief Enable tiled computation
   */
  void enableTiling(const TileConfig& config) {
    use_tiling_ = true;
    tile_config_ = config;
  }

  /**
   * @brief Disable tiling (use naive implementation)
   */
  void disableTiling() { use_tiling_ = false; }

  /**
   * @brief Perform GEMM computation
   */
  void compute() override {
    if (A_.size() == 0 || B_.size() == 0) {
      throw std::runtime_error("Inputs not set");
    }

    // Reset output buffer
    C_.fill(0);

    auto start = std::chrono::high_resolution_clock::now();

    if (use_tiling_) {
      computeTiled();
    } else {
      computeNaive();
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    computation_time_ms_ = elapsed.count();

    if (verbose_) {
      std::cout << "GEMM computation completed in " << computation_time_ms_
                << " ms\n";
    }
  }

  /**
   * @brief Get output tensor
   */
  const Tensor<T>& getOutput() const { return C_; }
  Tensor<T>& getOutput() { return C_; }

  /**
   * @brief Get computation time
   */
  double getComputationTime() const { return computation_time_ms_; }

  std::string getOpType() const override { return "GEMM"; }

  void printInfo() const override {
    OperatorBase::printInfo();
    std::cout << "  Input A: " << A_.shape().toString() << "\n";
    std::cout << "  Input B: " << B_.shape().toString() << "\n";
    std::cout << "  Output C: " << C_.shape().toString() << "\n";
    std::cout << "  Use Tiling: " << (use_tiling_ ? "Yes" : "No") << "\n";
    if (use_tiling_) {
      tile_config_.print();
    }
    std::cout << "  FLOPs: " << (2ULL * M_ * N_ * K_) << "\n";
  }

 private:
  /**
   * @brief Naive triple-loop GEMM implementation
   */
  void computeNaive() {
    if (verbose_) {
      std::cout << "Computing GEMM (naive): (" << M_ << "x" << K_ << ") * ("
                << K_ << "x" << N_ << ")\n";
    }

    for (size_t i = 0; i < M_; ++i) {
      for (size_t j = 0; j < N_; ++j) {
        T sum = 0;
        for (size_t k = 0; k < K_; ++k) {
          sum += A_.at(i, k) * B_.at(k, j);
        }
        C_.at(i, j) = sum;
      }
    }
  }

  /**
   * @brief Tiled GEMM implementation for better cache locality
   */
  void computeTiled() {
    if (verbose_) {
      std::cout << "Computing GEMM (tiled): (" << M_ << "x" << K_ << ") * ("
                << K_ << "x" << N_ << ")\n";
      tile_config_.print();
    }

    TileIterator iterator(M_, N_, K_, tile_config_);

    if (verbose_) {
      iterator.print();
    }

    size_t total_tiles = 0;

    // Iterate over tiles in M dimension
    for (size_t m_tile = 0; m_tile < iterator.getMTiles(); ++m_tile) {
      size_t m_start, m_end;
      iterator.getTileRangeM(m_tile, m_start, m_end);

      // Iterate over tiles in N dimension
      for (size_t n_tile = 0; n_tile < iterator.getNTiles(); ++n_tile) {
        size_t n_start, n_end;
        iterator.getTileRangeN(n_tile, n_start, n_end);

        // Iterate over tiles in K dimension
        for (size_t k_tile = 0; k_tile < iterator.getKTiles(); ++k_tile) {
          size_t k_start, k_end;
          iterator.getTileRangeK(k_tile, k_start, k_end);

          total_tiles++;

          if (verbose_) {
            std::cout << "Processing tile [" << m_tile << "," << n_tile << ","
                      << k_tile << "]: M[" << m_start << ":" << m_end << "], N["
                      << n_start << ":" << n_end << "], K[" << k_start << ":"
                      << k_end << "]\n";
          }

          // Compute tile
          computeTileGEMM(m_start, m_end, n_start, n_end, k_start, k_end);
        }
      }
    }

    if (verbose_) {
      std::cout << "Total tiles processed: " << total_tiles << "\n";
    }
  }

  /**
   * @brief Compute a single tile of GEMM
   */
  void computeTileGEMM(size_t m_start, size_t m_end, size_t n_start,
                       size_t n_end, size_t k_start, size_t k_end) {
    for (size_t i = m_start; i < m_end; ++i) {
      for (size_t j = n_start; j < n_end; ++j) {
        T sum = C_.at(i, j);  // Accumulate (important for K tiling)
        for (size_t k = k_start; k < k_end; ++k) {
          sum += A_.at(i, k) * B_.at(k, j);
        }
        C_.at(i, j) = sum;
      }
    }
  }

  // Input/output tensors
  Tensor<T> A_;
  Tensor<T> B_;
  Tensor<T> C_;

  // Matrix dimensions
  size_t M_, N_, K_;

  // Tiling configuration
  bool use_tiling_;
  TileConfig tile_config_;

  // Performance metrics
  double computation_time_ms_;
};

/**
 * @brief Verify GEMM result correctness
 */
template <typename T>
bool verifyGEMM(const Tensor<T>& A, const Tensor<T>& B, const Tensor<T>& C,
                bool verbose = false) {
  size_t M = A.shape()[0];
  size_t K = A.shape()[1];
  size_t N = B.shape()[1];

  if (C.shape()[0] != M || C.shape()[1] != N) {
    std::cerr << "Output shape mismatch\n";
    return false;
  }

  bool all_correct = true;
  size_t error_count = 0;
  const size_t max_errors_to_print = 10;

  for (size_t i = 0; i < M; ++i) {
    for (size_t j = 0; j < N; ++j) {
      T expected = 0;
      for (size_t k = 0; k < K; ++k) {
        expected += A.at(i, k) * B.at(k, j);
      }

      if (C.at(i, j) != expected) {
        all_correct = false;
        error_count++;
        if (verbose && error_count <= max_errors_to_print) {
          std::cerr << "Error at C[" << i << "][" << j << "]: got "
                    << C.at(i, j) << ", expected " << expected << "\n";
        }
      }
    }
  }

  if (verbose) {
    if (all_correct) {
      std::cout << "GEMM verification: PASSED\n";
    } else {
      std::cout << "GEMM verification: FAILED (errors: " << error_count << "/"
                << M * N << ")\n";
    }
  }

  return all_correct;
}

}  // namespace Operators

#endif  // GEMM_OPERATOR_H
