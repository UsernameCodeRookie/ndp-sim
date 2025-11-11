#ifndef GEMM_OPERATOR_H
#define GEMM_OPERATOR_H

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <type_traits>

#include "../components/tpu.h"
#include "base.h"
#include "tile.h"

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
 * Can run on CPU (fallback) or TPU (hardware accelerated)
 */
template <typename T = int,
          typename PrecisionTraits = typename DefaultPrecisionTrait<T>::type>
class GEMMOperator : public OperatorBase {
 public:
  GEMMOperator(const std::string& name = "GEMM")
      : OperatorBase(name), use_tiling_(false), tile_config_(16, 16, 16) {}

  using Traits = PrecisionTraits;
  using ValueType = typename Traits::ValueType;
  using AccumulatorType = typename Traits::AccumulatorType;

  static_assert(std::is_same_v<T, ValueType>,
                "GEMMOperator data type must match precision trait value type");

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

    if (isTPUBound()) {
      // Use TPU hardware acceleration
      computeOnTPU();
    } else {
      // CPU fallback
      if (use_tiling_) {
        computeTiled();
      } else {
        computeNaive();
      }
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    computation_time_ms_ = elapsed.count();

    if (verbose_) {
      std::cout << "GEMM computation completed in " << computation_time_ms_
                << " ms";
      if (isTPUBound()) {
        std::cout << " (TPU accelerated)";
      } else {
        std::cout << " (CPU " << (use_tiling_ ? "tiled" : "naive") << ")";
      }
      std::cout << "\n";
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
    if (!isTPUBound()) {
      std::cout << "  Use Tiling: " << (use_tiling_ ? "Yes" : "No") << "\n";
      if (use_tiling_) {
        tile_config_.print();
      }
    }
    std::cout << "  FLOPs: " << (2ULL * M_ * N_ * K_) << "\n";
  }

 private:
  /**
   * @brief Compute GEMM on TPU hardware using event-driven execution
   *
   * This uses a hybrid approach:
   * - Memory operations and high-level compute are event-driven
   * - Dense computation loop runs synchronously within compute event
   * - This avoids scheduling millions of individual MAC events
   */
  void computeOnTPU() {
    auto tpu_base = getTPU();
    if (!tpu_base) {
      throw std::runtime_error("TPU not bound");
    }

    auto tpu =
        std::dynamic_pointer_cast<SystolicArrayTPU<PrecisionTraits>>(tpu_base);
    if (!tpu) {
      throw std::runtime_error(
          "Bound TPU precision does not match GEMM operator traits");
    }

    if (verbose_) {
      std::cout << "Computing GEMM on TPU (event-driven): (" << M_ << "x" << K_
                << ") * (" << K_ << "x" << N_ << ") ["
                << tpu->getPrecisionName() << "]\n";
    }

    // Memory layout
    const uint32_t BASE_ADDR_A = 0x1000;
    const uint32_t BASE_ADDR_B = 0x2000;
    const uint32_t BASE_ADDR_C = 0x3000;

    // Get scheduler and timing parameters
    auto& scheduler = tpu->getScheduler();
    const uint64_t MEMORY_WRITE_CYCLES = tpu->getMemoryWriteLatency();
    const uint64_t MEMORY_READ_CYCLES = tpu->getMemoryReadLatency();
    const uint64_t MAC_COMPUTE_CYCLES = tpu->getMACLatency();

    // Flatten input matrices
    std::vector<ValueType> A_flat(M_ * K_);
    std::vector<ValueType> B_flat(K_ * N_);

    for (size_t i = 0; i < M_; ++i) {
      for (size_t j = 0; j < K_; ++j) {
        A_flat[i * K_ + j] = static_cast<ValueType>(A_.at(i, j));
      }
    }

    for (size_t i = 0; i < K_; ++i) {
      for (size_t j = 0; j < N_; ++j) {
        B_flat[i * N_ + j] = static_cast<ValueType>(B_.at(i, j));
      }
    }

    uint64_t event_time = 0;
    bool* completion_flag = new bool(false);

    // Step 1: Schedule data loading event
    event_time += A_flat.size() * MEMORY_WRITE_CYCLES;
    scheduler.scheduleAt(
        event_time,
        [tpu, BASE_ADDR_A, A_flat](EventDriven::EventScheduler& sched) {
          tpu->writeMemoryBlock(BASE_ADDR_A, A_flat);
          auto& tracer = EventDriven::Tracer::getInstance();
          tracer.traceMemoryWrite(sched.getCurrentTime(), tpu->getName(),
                                  BASE_ADDR_A, static_cast<int>(A_flat[0]));
        },
        0, "LoadMatrixA");

    event_time += B_flat.size() * MEMORY_WRITE_CYCLES;
    scheduler.scheduleAt(
        event_time,
        [tpu, BASE_ADDR_B, B_flat](EventDriven::EventScheduler& sched) {
          tpu->writeMemoryBlock(BASE_ADDR_B, B_flat);
          auto& tracer = EventDriven::Tracer::getInstance();
          tracer.traceMemoryWrite(sched.getCurrentTime(), tpu->getName(),
                                  BASE_ADDR_B, static_cast<int>(B_flat[0]));
        },
        0, "LoadMatrixB");

    // Step 2: Schedule MAC reset
    event_time += 10;
    scheduler.scheduleAt(
        event_time,
        [tpu](EventDriven::EventScheduler&) { tpu->resetAllMACs(); }, 0,
        "ResetMACs");

    // Step 3: Schedule GEMM computation as single event
    // Computation time: M * N * K * (2*MEM_READ + MAC_COMPUTE)
    uint64_t compute_cycles =
        M_ * N_ * K_ * (2 * MEMORY_READ_CYCLES + MAC_COMPUTE_CYCLES);
    event_time += compute_cycles;

    auto C_accum = std::make_shared<std::vector<AccumulatorType>>(
        M_ * N_, Traits::zeroAccumulator());

    size_t array_size = tpu->getArraySize();
    size_t M = M_, N = N_, K = K_;
    bool verbose = verbose_;

    scheduler.scheduleAt(
        event_time,
        [tpu, BASE_ADDR_A, BASE_ADDR_B, M, N, K, C_accum, array_size,
         verbose](EventDriven::EventScheduler& sched) {
          auto& tracer = EventDriven::Tracer::getInstance();
          uint64_t start_time = sched.getCurrentTime();

          // Perform GEMM computation
          for (size_t k = 0; k < K; ++k) {
            for (size_t i = 0; i < M; ++i) {
              for (size_t j = 0; j < N; ++j) {
                auto a_val = tpu->readMemory(BASE_ADDR_A + i * K + k);
                auto b_val = tpu->readMemory(BASE_ADDR_B + k * N + j);

                (*C_accum)[i * N + j] =
                    Traits::accumulate((*C_accum)[i * N + j], a_val, b_val);

                // Trace only first few elements
                if (i < 2 && j < 2 && k < 2) {
                  tracer.traceMAC(
                      start_time,
                      tpu->getName() + "_MAC_" + std::to_string(i) + "_" +
                          std::to_string(j),
                      static_cast<int>(
                          Traits::fromAccumulator((*C_accum)[i * N + j])),
                      static_cast<int>(a_val), static_cast<int>(b_val));
                }

                // Use hardware MAC for elements within array bounds
                if (i < array_size && j < array_size) {
                  auto mac = tpu->getMAC(i, j);
                  if (mac) {
                    mac->setInputA(a_val);
                    mac->setInputB(b_val);
                  }
                }
              }
            }
          }

          if (verbose) {
            std::cout << "  [" << sched.getCurrentTime()
                      << "] GEMM computation completed\n";
          }
        },
        0, "ComputeGEMM");

    // Step 4: Schedule result writeback
    event_time += 10;
    scheduler.scheduleAt(
        event_time,
        [tpu, C_accum, BASE_ADDR_C, M, N,
         verbose](EventDriven::EventScheduler& sched) {
          std::vector<ValueType> C_values(M * N);
          for (size_t idx = 0; idx < C_values.size(); ++idx) {
            C_values[idx] = Traits::fromAccumulator((*C_accum)[idx]);
          }
          tpu->writeMemoryBlock(BASE_ADDR_C, C_values);

          if (verbose) {
            std::cout << "  [" << sched.getCurrentTime()
                      << "] Results written to memory\n";
          }
        },
        0, "WriteResults");

    // Step 5: Schedule final readback
    event_time += M_ * N_ * MEMORY_READ_CYCLES;
    scheduler.scheduleAt(
        event_time,
        [this, tpu, BASE_ADDR_C,
         completion_flag](EventDriven::EventScheduler& sched) {
          std::vector<ValueType> C_result =
              tpu->readMemoryBlock(BASE_ADDR_C, M_ * N_);

          for (size_t i = 0; i < M_; ++i) {
            for (size_t j = 0; j < N_; ++j) {
              C_.at(i, j) = static_cast<T>(C_result[i * N_ + j]);
            }
          }

          *completion_flag = true;

          if (verbose_) {
            std::cout << "  [" << sched.getCurrentTime()
                      << "] Results read back, GEMM completed\n";
          }
        },
        0, "ReadResults");

    // Step 6: Run the scheduler
    scheduler.run(event_time + 100);

    if (!*completion_flag) {
      std::cerr << "[Warning] GEMM did not complete\n";
    }

    delete completion_flag;
  }
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
        ValueType sum = Traits::zeroValue();
        for (size_t k = 0; k < K_; ++k) {
          sum += static_cast<ValueType>(A_.at(i, k)) *
                 static_cast<ValueType>(B_.at(k, j));
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
        ValueType sum = C_.at(i, j);  // Accumulate (important for K tiling)
        for (size_t k = k_start; k < k_end; ++k) {
          sum += static_cast<ValueType>(A_.at(i, k)) *
                 static_cast<ValueType>(B_.at(k, j));
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

      bool mismatch = false;
      if constexpr (std::is_floating_point_v<T>) {
        T diff = std::fabs(C.at(i, j) - expected);
        T tolerance =
            static_cast<T>(1e-3) * static_cast<T>(std::max<size_t>(1, K));
        mismatch = diff > tolerance;
      } else {
        mismatch = (C.at(i, j) != expected);
      }

      if (mismatch) {
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
