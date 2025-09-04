#ifndef BUFFER_H
#define BUFFER_H

#include <common.h>

// Simple circular buffer with fixed capacity
template <typename T>
class Buffer {
 public:
  explicit Buffer(size_t capacity) noexcept : cap(capacity), data(capacity) {}

  bool push(const T& v) noexcept {
    if (is_full()) return false;
    data[tail] = v;
    tail = (tail + 1) % cap;
    ++sz;
    return true;
  }

  bool pop() noexcept {
    if (is_empty()) return false;
    head = (head + 1) % cap;
    --sz;
    return true;
  }

  T& front() noexcept { return data[head]; }
  const T& front() const noexcept { return data[head]; }

  bool is_empty() const noexcept { return sz == 0; }
  bool is_full() const noexcept { return sz == cap; }
  size_t size() const noexcept { return sz; }
  size_t capacity() const noexcept { return cap; }

 private:
  size_t cap;
  std::vector<T> data;
  size_t head = 0, tail = 0, sz = 0;
};

#endif  // BUFFER_H