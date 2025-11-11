#ifndef PRECISION_H
#define PRECISION_H

#include <bit>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>
#include <type_traits>

namespace PrecisionDetail {

#if defined(__cpp_lib_bit_cast) && __cpp_lib_bit_cast >= 201806L
using std::bit_cast;
#else
template <typename To, typename From>
std::enable_if_t<(sizeof(To) == sizeof(From)) &&
                     std::is_trivially_copyable_v<To> &&
                     std::is_trivially_copyable_v<From>,
                 To> bit_cast(const From& source) noexcept {
  To destination;
  std::memcpy(&destination, &source, sizeof(To));
  return destination;
}
#endif

}  // namespace PrecisionDetail

struct Int32PrecisionTraits {
  using ValueType = int32_t;
  using AccumulatorType = int64_t;

  static constexpr const char* name() { return "int32"; }

  static ValueType zeroValue() { return 0; }
  static AccumulatorType zeroAccumulator() { return 0; }

  static AccumulatorType accumulate(AccumulatorType acc, ValueType a,
                                    ValueType b) {
    return acc +
           static_cast<AccumulatorType>(a) * static_cast<AccumulatorType>(b);
  }

  static ValueType fromAccumulator(AccumulatorType acc) {
    return static_cast<ValueType>(acc);
  }

  static int32_t encode(ValueType value) { return static_cast<int32_t>(value); }

  static ValueType decode(int32_t raw) { return static_cast<ValueType>(raw); }

  static std::string toString(ValueType value) { return std::to_string(value); }
};

struct Float32PrecisionTraits {
  using ValueType = float;
  using AccumulatorType = float;

  static constexpr const char* name() { return "fp32"; }

  static ValueType zeroValue() { return 0.0f; }
  static AccumulatorType zeroAccumulator() { return 0.0f; }

  static AccumulatorType accumulate(AccumulatorType acc, ValueType a,
                                    ValueType b) {
    return acc +
           static_cast<AccumulatorType>(a) * static_cast<AccumulatorType>(b);
  }

  static ValueType fromAccumulator(AccumulatorType acc) {
    return static_cast<ValueType>(acc);
  }

  static int32_t encode(ValueType value) {
    return PrecisionDetail::bit_cast<int32_t>(value);
  }

  static ValueType decode(int32_t raw) {
    return PrecisionDetail::bit_cast<ValueType>(raw);
  }

  static std::string toString(ValueType value) {
    std::ostringstream oss;
    oss.setf(std::ios::fixed, std::ios::floatfield);
    oss << std::setprecision(6) << value;
    return oss.str();
  }
};

template <typename T>
struct DefaultPrecisionTrait;

template <>
struct DefaultPrecisionTrait<int> {
  using type = Int32PrecisionTraits;
};

template <>
struct DefaultPrecisionTrait<float> {
  using type = Float32PrecisionTraits;
};

#endif  // PRECISION_H
