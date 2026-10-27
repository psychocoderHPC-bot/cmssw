#ifndef HeterogeneousCore_AlpakaInterface_interface_AtomicPairCounter_h
#define HeterogeneousCore_AlpakaInterface_interface_AtomicPairCounter_h

#include <cstdint>

#include <aV2P/alpaka.hpp>

namespace cms::alpakatools {

  class AtomicPairCounter {
  public:
    using DoubleWord = uint64_t;

    AV2P_FN_HOST_ACC constexpr AtomicPairCounter() : counter_{0} {}
    AV2P_FN_HOST_ACC constexpr AtomicPairCounter(uint32_t first, uint32_t second) : counter_{pack(first, second)} {}
    AV2P_FN_HOST_ACC constexpr AtomicPairCounter(DoubleWord values) : counter_{values} {}

    AV2P_FN_HOST_ACC constexpr AtomicPairCounter& operator=(DoubleWord values) {
      counter_.as_doubleword = values;
      return *this;
    }

    struct Counters {
      uint32_t first;   // in a "One to Many" association is the number of "One"
      uint32_t second;  // in a "One to Many" association is the total number of associations
    };

    AV2P_FN_HOST_ACC constexpr Counters get() const { return counter_.as_counters; }

    // atomically add as_counters, and return the previous value
    template <aV2P::concepts::Acc TAcc>
    AV2P_FN_ACC AV2P_FN_INLINE constexpr Counters add(const TAcc& acc, Counters c) {
      Packer value{pack(c.first, c.second)};
      Packer ret{0};
      ret.as_doubleword =
          aV2P::atomicAdd(acc, &counter_.as_doubleword, value.as_doubleword, aV2P::hierarchy::Blocks{});
      return ret.as_counters;
    }

    // atomically increment first and add i to second, and return the previous value
    template <aV2P::concepts::Acc TAcc>
    AV2P_FN_ACC AV2P_FN_INLINE Counters constexpr inc_add(const TAcc& acc, uint32_t i) {
      return add(acc, {1u, i});
    }

  private:
    union Packer {
      DoubleWord as_doubleword;
      Counters as_counters;
      constexpr Packer(DoubleWord _as_doubleword) : as_doubleword(_as_doubleword) { ; };
      constexpr Packer(Counters _as_counters) : as_counters(_as_counters) { ; };
    };

    // pack two uint32_t values in a DoubleWord (aka uint64_t)
    // this is needed because in c++17 a union can only be aggregate-initialised to its first type
    // it can be probably removed with c++20, and replace with a designated initialiser
    static constexpr DoubleWord pack(uint32_t first, uint32_t second) {
      Packer ret{0};
      ret.as_counters = {first, second};
      return ret.as_doubleword;
    }

    Packer counter_;
  };

}  // namespace cms::alpakatools

#endif  // HeterogeneousCore_AlpakaInterface_interface_AtomicPairCounter_h
