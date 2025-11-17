#ifndef LINK_H
#define LINK_H

#include <cstdint>
#include <memory>
#include <string>

#include "../buffer.h"
#include "../tick.h"

namespace Architecture {

/**
 * @brief LinkConnection class with parameterizable buffering
 *
 * Transfers data from source port to destination port with configurable
 * internal buffering to handle data flow mismatches between producer and
 * consumer.
 *
 * Unlike Wire which uses a fixed two-level buffer, Link uses a Buffer
 * component with configurable parameters including:
 * - Buffer depth
 * - FIFO or random access mode
 * - Number of read/write ports
 * - Overflow/underflow checking
 * - Write-through bypass
 * - Read/write latencies
 *
 * - Supports optional latency for simulating link delay
 */
class Link : public TickingConnection {
 public:
  Link(const std::string& name, EventDriven::EventScheduler& scheduler,
       uint64_t period,
       const BufferParameters& buffer_params = BufferParameters())
      : TickingConnection(name, scheduler, period),
        transfers_(0),
        buffer_(std::make_unique<Buffer>(name + "_Buffer", scheduler,
                                         buffer_params)) {}

  virtual ~Link() = default;

  // Getters
  uint64_t getTransfers() const { return transfers_; }

  /**
   * @brief Get the internal buffer
   */
  Buffer* getBuffer() { return buffer_.get(); }
  const Buffer* getBuffer() const { return buffer_.get(); }

  /**
   * @brief Get buffer statistics
   */
  std::string getBufferStatistics() const { return buffer_->getStatistics(); }

  /**
   * @brief Propagate data with parameterized buffering
   *
   * Data flow:
   * 1. Read new data from source port
   * 2. Write to internal buffer if space available
   * 3. If destination ports exist, read from buffer and deliver
   * 4. Otherwise, data stays buffered for later consumption
   *
   * This allows decoupling of producer and consumer rates.
   */
  void propagate() override {
    if (src_ports_.empty()) return;

    // Stage 1: Try to read new data from source
    if (src_ports_[0]->hasData()) {
      auto data = src_ports_[0]->read();
      if (data && data->valid) {
        // Try to write to buffer
        if (!buffer_->write(data)) {
          // Buffer is full, data is dropped
          // Could implement backpressure here if needed
        }
      }
    }

    // Stage 2: Try to deliver data from buffer to destination ports
    if (!dst_ports_.empty() && !buffer_->isEmpty()) {
      auto data = buffer_->read();
      if (data) {
        deliverData(dst_ports_[0], data);
        transfers_++;
      }
    }
  }

  /**
   * @brief Reset the link and its buffer
   */
  void reset() {
    transfers_ = 0;
    buffer_->reset();
  }

  /** @brief Print connection statistics */
  void printStatistics() const {
    std::cout << "\n=== Connection Statistics: " << name_
              << " ===" << std::endl;
    std::cout << "Total transfers: " << transfers_ << std::endl;
    std::cout << buffer_->getStatistics() << std::endl;
  }

 private:
  /** @brief Deliver data to destination (with optional latency) */
  void deliverData(std::shared_ptr<Port> dst_port,
                   std::shared_ptr<DataPacket> data) {
    if (latency_ > 0) {
      auto dst_copy = dst_port;
      auto data_copy = data;
      auto event = std::make_shared<EventDriven::LambdaEvent>(
          scheduler_.getCurrentTime() + latency_,
          [dst_copy, data_copy](EventDriven::EventScheduler&) {
            dst_copy->setData(data_copy);
          },
          -1, name_ + "_Deliver");
      scheduler_.schedule(event);
    } else {
      dst_port->setData(data);
    }
  }

 private:
  uint64_t transfers_;              // Successful transfers count
  std::unique_ptr<Buffer> buffer_;  // Parameterizable buffer for data storage
};

}  // namespace Architecture

#endif  // LINK_H
