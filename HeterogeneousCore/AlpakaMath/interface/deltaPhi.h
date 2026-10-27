#ifndef HeterogeneousCore_AlpakaMath_deltaPhi_h
#define HeterogeneousCore_AlpakaMath_deltaPhi_h

#include <aV2P/alpaka.hpp>

namespace cms::alpakatools {

  // reduce to [-pi,pi]
  template <aV2P::concepts::Acc TAcc, std::floating_point T>
  AV2P_FN_HOST_ACC inline T reducePhiRange(TAcc const& acc, T x) {
    constexpr T o2pi = T{1.} / (T{2.} * std::numbers::pi_v<T>);
    if (aV2P::math::abs(acc, x) <= std::numbers::pi_v<T>)
      return x;
    T n = aV2P::math::round(acc, x * o2pi);
    return x - n * T{2.} * std::numbers::pi_v<T>;
  }

  template <aV2P::concepts::Acc TAcc, typename T>
  AV2P_FN_HOST_ACC inline T phi(TAcc const& acc, T x, T y) {
    return reducePhiRange(acc, std::numbers::pi_v<T> + aV2P::math::atan2(acc, -y, -x));
  }

  template <aV2P::concepts::Acc TAcc, typename T>
  AV2P_FN_HOST_ACC inline T deltaPhi(TAcc const& acc, T x1, T y1, T x2, T y2) {
    return aV2P::math::atan2(acc, x1 * y2 - x2 * y1, x1 * x2 + y1 * y2);
  }

  template <aV2P::concepts::Acc TAcc, typename T>
  AV2P_FN_HOST_ACC inline T deltaPhi(TAcc const& acc, T phi1, T phi2) {
    return reducePhiRange(acc, phi1 - phi2);
  }

}  // namespace cms::alpakatools

#endif
