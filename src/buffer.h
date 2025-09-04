#ifndef BUFFER_H
#define BUFFER_H

#include <common.h>

// Simple circular buffer with fixed capacity
template <typename T>
class Buffer {
 public:
  explicit Buffer(size_t capacity) noexcept
      : capacity(capacity), data(capacity) {}

  bool push(const T& v) noexcept {
    if (is_full()) return false;
    data[tail] = v;
    tail = (tail + 1) % capacity;
    ++size;
    return true;
  }

  bool pop() noexcept {
    if (is_empty()) return false;
    head = (head + 1) % capacity;
    --size;
    return true;
  }

  T& front() noexcept { return data[head]; }
  const T& front() const noexcept { return data[head]; }

  bool is_empty() const noexcept { return size == 0; }
  bool is_full() const noexcept { return size == capacity; }
  size_t size() const noexcept { return size; }
  size_t capacity() const noexcept { return capacity; }

 private:
  size_t capacity;
  std::vector<T> data;
  size_t head = 0, tail = 0, size = 0;
};

#endif  // BUFFER_H