#ifndef BUFFER_H
#define BUFFER_H

#include <common.h>

// Simple circular buffer with fixed capacity
template <typename T>
class Buffer {
 public:
  explicit Buffer(size_t capacity) noexcept : capacity(capacity) {}

  bool push(const T& v) noexcept {
    if (data.size() >= capacity) return false;
    data.push_back(v);
    return true;
  }

  bool pop(T& v) noexcept {
    if (data.empty()) return false;
    v = data.front();
    data.pop_front();
    return true;
  }

  bool empty() const noexcept { return data.empty(); }
  bool full() const noexcept { return data.size() >= capacity; }

 private:
  std::deque<T> data;
  size_t capacity;
};

#endif  // BUFFER_H