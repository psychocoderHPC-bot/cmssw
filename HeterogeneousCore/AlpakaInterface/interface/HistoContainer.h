#ifndef HeterogeneousCore_AlpakaInterface_interface_HistoContainer_h
#define HeterogeneousCore_AlpakaInterface_interface_HistoContainer_h

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include <aV2P/alpaka.hpp>

#include "HeterogeneousCore/AlpakaInterface/interface/AtomicPairCounter.h"
#include "HeterogeneousCore/AlpakaInterface/interface/OneToManyAssoc.h"
#include "HeterogeneousCore/AlpakaInterface/interface/alpakastdAlgorithm.h"
#include "HeterogeneousCore/AlpakaInterface/interface/memory.h"
#include "HeterogeneousCore/AlpakaInterface/interface/prefixScan.h"
#include "HeterogeneousCore/AlpakaInterface/interface/workdivision.h"

namespace cms::alpakatools {

  struct countFromVector {
    template <aV2P::concepts::Acc TAcc, typename Histo, typename View, typename Accessor>
    AV2P_FN_ACC void operator()(const TAcc &acc,
                                  Histo *__restrict__ h,
                                  uint32_t nh,
                                  View v,
                                  Accessor func,
                                  uint32_t const *__restrict__ offsets) const {
      const uint32_t nt = offsets[nh];
      for (uint32_t i : uniform_elements(acc, nt)) {
        auto off = alpaka_std::upper_bound(offsets, offsets + nh + 1, i);
        AV2P_ASSERT_ACC((*off) > 0);
        int32_t ih = off - offsets - 1;
        AV2P_ASSERT_ACC(ih >= 0);
        AV2P_ASSERT_ACC(ih < int(nh));
        h->count(acc, func(v[i]), ih);
      }
    }

    template <aV2P::concepts::Acc TAcc, typename Histo, typename T>
    AV2P_FN_ACC void operator()(const TAcc &acc,
                                  Histo *__restrict__ h,
                                  uint32_t nh,
                                  T const *__restrict__ v,
                                  uint32_t const *__restrict__ offsets) const {
      const uint32_t nt = offsets[nh];
      for (uint32_t i : uniform_elements(acc, nt)) {
        auto off = alpaka_std::upper_bound(offsets, offsets + nh + 1, i);
        AV2P_ASSERT_ACC((*off) > 0);
        int32_t ih = off - offsets - 1;
        AV2P_ASSERT_ACC(ih >= 0);
        AV2P_ASSERT_ACC(ih < int(nh));
        h->count(acc, v[i], ih);
      }
    }
  };

  struct fillFromVector {
    template <aV2P::concepts::Acc TAcc, typename Histo, typename View, typename Accessor>
    AV2P_FN_ACC void operator()(const TAcc &acc,
                                  Histo *__restrict__ h,
                                  uint32_t nh,
                                  View v,
                                  Accessor func,
                                  uint32_t const *__restrict__ offsets) const {
      const uint32_t nt = offsets[nh];
      for (uint32_t i : uniform_elements(acc, nt)) {
        auto off = alpaka_std::upper_bound(offsets, offsets + nh + 1, i);
        AV2P_ASSERT_ACC((*off) > 0);
        int32_t ih = off - offsets - 1;
        AV2P_ASSERT_ACC(ih >= 0);
        AV2P_ASSERT_ACC(ih < int(nh));
        h->fill(acc, func(v[i]), i, ih);
      }
    }

    template <aV2P::concepts::Acc TAcc, typename Histo, typename T>
    AV2P_FN_ACC void operator()(const TAcc &acc,
                                  Histo *__restrict__ h,
                                  uint32_t nh,
                                  T const *__restrict__ v,
                                  uint32_t const *__restrict__ offsets) const {
      const uint32_t nt = offsets[nh];
      for (uint32_t i : uniform_elements(acc, nt)) {
        auto off = alpaka_std::upper_bound(offsets, offsets + nh + 1, i);
        AV2P_ASSERT_ACC((*off) > 0);
        int32_t ih = off - offsets - 1;
        AV2P_ASSERT_ACC(ih >= 0);
        AV2P_ASSERT_ACC(ih < int(nh));
        h->fill(acc, v[i], i, ih);
      }
    }
  };

  template <aV2P::concepts::Acc TAcc, typename Histo, typename View, typename Accessor, typename TQueue>
  AV2P_FN_INLINE void fillManyFromVector(Histo *__restrict__ h,
                                           uint32_t nh,
                                           View v,
                                           Accessor func,
                                           uint32_t const *__restrict__ offsets,
                                           uint32_t totSize,
                                           uint32_t nthreads,
                                           TQueue &queue) {
    Histo::template launchZero<TAcc>(h, queue);
    const auto workDiv = make_workdiv<TAcc>(divide_up_by(totSize, nthreads), nthreads);

    aV2P::exec<TAcc>(queue, workDiv, countFromVector(), h, nh, v, func, offsets);
    Histo::template launchFinalize<TAcc>(h, queue);

    aV2P::exec<TAcc>(queue, workDiv, fillFromVector(), h, nh, v, func, offsets);
  }

  template <aV2P::concepts::Acc TAcc, typename Histo, typename View, typename Accessor, typename TQueue>
  AV2P_FN_INLINE void fillManyFromVector(Histo *__restrict__ h,
                                           typename Histo::View hv,
                                           uint32_t nh,
                                           View v,
                                           Accessor func,
                                           uint32_t const *__restrict__ offsets,
                                           uint32_t totSize,
                                           uint32_t nthreads,
                                           TQueue &queue) {
    Histo::template launchZero<TAcc>(hv, queue);
    const auto workDiv = make_workdiv<TAcc>(divide_up_by(totSize, nthreads), nthreads);

    aV2P::exec<TAcc>(queue, workDiv, countFromVector(), h, nh, v, func, offsets);
    Histo::template launchFinalize<TAcc>(h, queue);

    aV2P::exec<TAcc>(queue, workDiv, fillFromVector(), h, nh, v, func, offsets);
  }

  template <aV2P::concepts::Acc TAcc, typename Histo, typename T, typename TQueue>
  AV2P_FN_INLINE void fillManyFromVector(Histo *__restrict__ h,
                                           uint32_t nh,
                                           T const *__restrict__ v,
                                           uint32_t const *__restrict__ offsets,
                                           uint32_t totSize,
                                           uint32_t nthreads,
                                           TQueue &queue) {
    Histo::template launchZero<TAcc>(h, queue);
    const auto workDiv = make_workdiv<TAcc>(divide_up_by(totSize, nthreads), nthreads);

    aV2P::exec<TAcc>(queue, workDiv, countFromVector(), h, nh, v, offsets);
    Histo::template launchFinalize<TAcc>(h, queue);

    aV2P::exec<TAcc>(queue, workDiv, fillFromVector(), h, nh, v, offsets);
  }

  template <aV2P::concepts::Acc TAcc, typename Histo, typename T, typename TQueue>
  AV2P_FN_INLINE void fillManyFromVector(Histo *__restrict__ h,
                                           typename Histo::View hv,
                                           uint32_t nh,
                                           T const *__restrict__ v,
                                           uint32_t const *__restrict__ offsets,
                                           uint32_t totSize,
                                           uint32_t nthreads,
                                           TQueue &queue) {
    Histo::template launchZero<TAcc>(hv, queue);
    const auto workDiv = make_workdiv<TAcc>(divide_up_by(totSize, nthreads), nthreads);

    aV2P::exec<TAcc>(queue, workDiv, countFromVector(), h, nh, v, offsets);
    Histo::template launchFinalize<TAcc>(h, queue);

    aV2P::exec<TAcc>(queue, workDiv, fillFromVector(), h, nh, v, offsets);
  }

  // iteratate over N bins left and right of the one containing "v"
  template <aV2P::concepts::Acc TAcc, typename Hist, typename V, typename Func>
  AV2P_FN_ACC AV2P_FN_INLINE void forEachInBins(const TAcc &acc, Hist const &hist, V value, int n, Func func) {
    int bs = Hist::bin(value);
    int be = aV2P::math::min(acc, int(Hist::nbins() - 1), bs + n);
    bs = aV2P::math::max(acc, 0, bs - n);
    AV2P_ASSERT_ACC(be >= bs);
    for (auto pj = hist.begin(bs); pj < hist.end(be); ++pj) {
      func(*pj);
    }
  }

  // iteratate over bins containing all values in window wmin, wmax
  template <typename Hist, typename V, typename Func>
  AV2P_FN_ACC AV2P_FN_INLINE void forEachInWindow(Hist const &hist, V wmin, V wmax, Func const &func) {
    auto bs = Hist::bin(wmin);
    auto be = Hist::bin(wmax);
    AV2P_ASSERT_ACC(be >= bs);
    for (auto pj = hist.begin(bs); pj < hist.end(be); ++pj) {
      func(*pj);
    }
  }

  template <typename T,                         // the type of the discretized input values
            FlexiStorageBase::size_type NBINS,  // number of bins
            // max number of element. If cms::alpakatools::kDynamicSize is initialized at runtime using external storage
            FlexiStorageBase::size_type SIZE,
            uint32_t S = sizeof(T) * 8,  // number of significant bits in T
            typename I = uint32_t,  // type stored in the container (usually an index in a vector of the input values)
            FlexiStorageBase::size_type NHISTS = 1  // number of histos stored
            >
  class HistoContainer : public OneToManyAssocRandomAccess<I, NHISTS * NBINS + 1, SIZE> {
  public:
    using Base = OneToManyAssocRandomAccess<I, NHISTS * NBINS + 1, SIZE>;
    using View = typename Base::View;
    using Counter = typename Base::Counter;
    using value_type = typename Base::value_type;
    using size_type = typename Base::size_type;
    using UT = typename std::make_unsigned<T>::type;

    static constexpr uint32_t ilog2(uint32_t v) {
      constexpr uint32_t b[] = {0x2, 0xC, 0xF0, 0xFF00, 0xFFFF0000};
      constexpr uint32_t s[] = {1, 2, 4, 8, 16};

      uint32_t r = 0;  // result of log2(v) will go here
      for (auto i = 4; i >= 0; i--)
        if (v & b[i]) {
          v >>= s[i];
          r |= s[i];
        }
      return r;
    }

    static constexpr size_type nhists() { return NHISTS; }
    static constexpr size_type nbins() { return NBINS; }
    static constexpr size_type totbins() { return NHISTS * NBINS + 1; }
    static constexpr uint32_t sizeT() { return S; }
    static constexpr uint32_t nbits() { return ilog2(NBINS - 1) + 1; }

    static constexpr size_type histOff(size_type nh) { return NBINS * nh; }

    static constexpr UT bin(T t) {
      constexpr uint32_t shift = sizeT() - nbits();
      constexpr uint32_t mask = (1 << nbits()) - 1;
      return (t >> shift) & mask;
    }

    template <aV2P::concepts::Acc TAcc>
    AV2P_FN_ACC AV2P_FN_INLINE void count(const TAcc &acc, T t) {
      size_type b = bin(t);
      AV2P_ASSERT_ACC(b < nbins());
      Base::atomicIncrement(acc, this->off[b]);
    }

    template <aV2P::concepts::Acc TAcc>
    AV2P_FN_ACC AV2P_FN_INLINE void fill(const TAcc &acc, T t, value_type j) {
      size_type b = bin(t);
      AV2P_ASSERT_ACC(b < nbins());
      auto w = Base::atomicDecrement(acc, this->off[b]);
      AV2P_ASSERT_ACC(w > 0);
      this->content[w - 1] = j;
    }

    template <aV2P::concepts::Acc TAcc>
    AV2P_FN_ACC AV2P_FN_INLINE void count(const TAcc &acc, T t, size_type nh) {
      size_type b = bin(t);
      AV2P_ASSERT_ACC(b < nbins());
      b += histOff(nh);
      AV2P_ASSERT_ACC(b < totbins());
      Base::atomicIncrement(acc, this->off[b]);
    }

    template <aV2P::concepts::Acc TAcc>
    AV2P_FN_ACC AV2P_FN_INLINE void fill(const TAcc &acc, T t, value_type j, size_type nh) {
      size_type b = bin(t);
      AV2P_ASSERT_ACC(b < nbins());
      b += histOff(nh);
      AV2P_ASSERT_ACC(b < totbins());
      auto w = Base::atomicDecrement(acc, this->off[b]);
      AV2P_ASSERT_ACC(w > 0);
      this->content[w - 1] = j;
    }
  };
}  // namespace cms::alpakatools
#endif  // HeterogeneousCore_AlpakaInterface_interface_HistoContainer_h
