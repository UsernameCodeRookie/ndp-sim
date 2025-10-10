#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "common.h"

template <typename T>
struct Port;

template <typename T, typename = void>
struct PayloadTraits {
  static bool isValid(const T&) noexcept { return true; }
  static bool isReady(const T&) noexcept { return true; }
  static void setReady(T&, bool) noexcept {}
};

template <typename T>
struct PayloadTraits<T, std::void_t<decltype(T::valid), decltype(T::ready)>> {
  static bool isValid(const T& p) noexcept { return p.valid; }
  static bool isReady(const T& p) noexcept { return p.ready; }
  static void setReady(T& p, bool r) noexcept { p.ready = r; }
};

enum class CastMode { SingleCast, Broadcast };

template <typename T, CastMode MODE>
struct IProtocol {
  virtual ~IProtocol() = default;

  virtual void execute(
      const std::vector<std::shared_ptr<Port<T>>>& srcs,
      const std::vector<std::shared_ptr<Port<T>>>& dsts) noexcept = 0;
};

template <typename T, CastMode MODE>
class ProtocolValidReady : public IProtocol<T, MODE> {
 public:
  void execute(
      const std::vector<std::shared_ptr<Port<T>>>& srcs,
      const std::vector<std::shared_ptr<Port<T>>>& dsts) noexcept override {
    for (auto& s : srcs) {
      if (!s || !s->valid()) continue;

      T sdata{};
      if (!s->peek(sdata)) continue;
      if (!PayloadTraits<T>::isValid(sdata)) continue;

      bool anyAccepted = false;

      if constexpr (MODE == CastMode::SingleCast) {
        for (auto& d : dsts) {
          if (!d) continue;
          if (PayloadTraits<T>::isReady(sdata)) {
            if (d->write(sdata)) {
              anyAccepted = true;
              break;
            }
          }
        }
      } else {  // Broadcast mode
        for (auto& d : dsts) {
          if (d) {
            d->write(sdata);
            anyAccepted = true;
          }
        }
      }

      if (anyAccepted) {
        T tmp;
        s->read(tmp);  // consume from source
      }
    }
  }
};

template <typename T>
using ValidReadyProtocol = ProtocolValidReady<T, CastMode::SingleCast>;

template <typename T>
using ValidReadyBroadcastProtocol = ProtocolValidReady<T, CastMode::Broadcast>;

template <typename DataT>
struct ValidReadyData {
  DataT data{};
  bool valid = true;
  bool ready = true;

  ValidReadyData() = default;
  ValidReadyData(const DataT& d, bool v = true, bool r = true)
      : data(d), valid(v), ready(r) {}
};

#endif  // PROTOCOL_H