#ifndef HeterogeneousCore_AlpakaInterface_interface_OneToManyAssoc_h
#define HeterogeneousCore_AlpakaInterface_interface_OneToManyAssoc_h

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include <aV2P/alpaka.hpp>

#include "HeterogeneousCore/AlpakaInterface/interface/AtomicPairCounter.h"
#include "HeterogeneousCore/AlpakaInterface/interface/FlexiStorage.h"
#include "HeterogeneousCore/AlpakaInterface/interface/prefixScan.h"
#include "HeterogeneousCore/AlpakaInterface/interface/workdivision.h"

namespace cms::alpakatools {

  template <
      // type stored in the container (usually an index in a vector of the input values)
      typename I,
      // number of "Ones"  +1. If cms::alpakatools::kDynamicSize is initialized at runtime using external storage
      FlexiStorageBase::size_type ONES,
      // max number of element. If cms::alpakatools::kDynamicSize is initialized at runtime using external storage
      FlexiStorageBase::size_type SIZE>
  class OneToManyAssocBase {
  public:
    using Counter = uint32_t;

    using CountersOnly = OneToManyAssocBase<I, ONES, 0>;

    using size_type = typename FlexiStorageBase::size_type;  // type of the "ones" / keys
    using value_type = I;                                    // type of the "many" / values / content

    struct View {
      OneToManyAssocBase *assoc = nullptr;
      Counter *offStorage = nullptr;
      value_type *contentStorage = nullptr;
      size_type offSize = kDynamicSize;
      size_type contentSize = kDynamicSize;
    };

    static constexpr size_type ctNOnes() { return ONES; }
    constexpr size_type totOnes() const { return off.capacity(); }
    constexpr size_type nOnes() const { return totOnes() - 1; }
    static constexpr size_type ctCapacity() { return SIZE; }
    constexpr size_type capacity() const { return content.capacity(); }

    AV2P_FN_HOST_ACC void initStorage(View view) {
      AV2P_ASSERT_ACC(view.assoc == this);
      if constexpr (ctCapacity() == kDynamicSize) {
        AV2P_ASSERT_ACC(view.contentStorage);
        AV2P_ASSERT_ACC(view.contentSize > 0);
        content.init(view.contentStorage, view.contentSize);
      }
      if constexpr (ctNOnes() == kDynamicSize) {
        AV2P_ASSERT_ACC(view.offStorage);
        AV2P_ASSERT_ACC(view.offSize > 0);
        off.init(view.offStorage, view.offSize);
      }
    }

    AV2P_FN_HOST_ACC void zero() {
      for (size_type i = 0; i < totOnes(); ++i) {
        off[i] = 0;
      }
    }

    template <aV2P::concepts::Acc TAcc>
    AV2P_FN_ACC AV2P_FN_INLINE void add(const TAcc &acc, CountersOnly const &co) {
      for (size_type i = 0; i < totOnes(); ++i) {
        aV2P::atomicAdd(acc, off.data() + i, co.off[i], aV2P::hierarchy::Blocks{});
      }
    }

    template <aV2P::concepts::Acc TAcc>
    AV2P_FN_ACC AV2P_FN_INLINE static Counter atomicIncrement(const TAcc &acc, Counter &x) {
      return aV2P::atomicAdd(acc, &x, static_cast<Counter>(1), aV2P::hierarchy::Blocks{});
    }

    template <aV2P::concepts::Acc TAcc>
    AV2P_FN_ACC AV2P_FN_INLINE static Counter atomicDecrement(const TAcc &acc, Counter &x) {
      return aV2P::atomicSub(acc, &x, static_cast<Counter>(1), aV2P::hierarchy::Blocks{});
    }

    template <aV2P::concepts::Acc TAcc>
    AV2P_FN_ACC AV2P_FN_INLINE void count(const TAcc &acc, size_type b) {
      AV2P_ASSERT_ACC(b < nOnes());
      atomicIncrement(acc, off[b]);
    }

    template <aV2P::concepts::Acc TAcc>
    AV2P_FN_ACC AV2P_FN_INLINE void fill(const TAcc &acc, size_type b, value_type j) {
      AV2P_ASSERT_ACC(b < nOnes());
      auto w = atomicDecrement(acc, off[b]);
      AV2P_ASSERT_ACC(w > 0);
      content[w - 1] = j;
    }

    // this MUST BE DONE in a single block (or in two kernels!)
    struct zeroAndInit {
      template <aV2P::concepts::Acc TAcc>
      AV2P_FN_ACC void operator()(const TAcc &acc, View view) const {
        AV2P_ASSERT_ACC((1 == aV2P::getWorkDiv<aV2P::Grid, aV2P::Blocks>(acc)[0]));
        AV2P_ASSERT_ACC((0 == aV2P::getIdx<aV2P::Grid, aV2P::Blocks>(acc)[0]));
        auto h = view.assoc;
        if (cms::alpakatools::once_per_block(acc)) {
          h->psws = 0;
          h->initStorage(view);
        }
        aV2P::syncBlockThreads(acc);
        for (size_type i : cms::alpakatools::independent_group_elements(acc, h->totOnes())) {
          h->off[i] = 0;
        }
      }
    };

    template <aV2P::concepts::Acc TAcc, typename TQueue>
    AV2P_FN_INLINE static void launchZero(OneToManyAssocBase *h, TQueue &queue) {
      View view = {h, nullptr, nullptr, kDynamicSize, kDynamicSize};
      launchZero<TAcc>(view, queue);
    }

    template <aV2P::concepts::Acc TAcc, typename TQueue>
    AV2P_FN_INLINE static void launchZero(View view, TQueue &queue) {
      if constexpr (ctCapacity() == kDynamicSize) {
        AV2P_ASSERT_ACC(view.contentStorage);
        AV2P_ASSERT_ACC(view.contentSize > 0);
      }
      if constexpr (ctNOnes() == kDynamicSize) {
        AV2P_ASSERT_ACC(view.offStorage);
        AV2P_ASSERT_ACC(view.offSize > 0);
      }
      if constexpr (!requires_single_thread_per_block_v<TAcc>) {
        auto nthreads = 1024;
        auto nblocks = 1;  // MUST BE ONE as memory is initialize in thread 0 (alternative is two kernels);
        auto workDiv = cms::alpakatools::make_workdiv<TAcc>(nblocks, nthreads);
        aV2P::exec<TAcc>(queue, workDiv, zeroAndInit{}, view);
      } else {
        auto h = view.assoc;
        AV2P_ASSERT_ACC(h);
        h->initStorage(view);
        h->zero();
        h->psws = 0;
      }
    }

    constexpr Counter size() const { return off[totOnes() - 1]; }
    constexpr Counter size(size_type b) const { return off[b + 1] - off[b]; }

    constexpr value_type const *begin() const { return content.data(); }
    constexpr value_type const *end() const { return begin() + size(); }

    constexpr value_type const *begin(size_type b) const { return content.data() + off[b]; }
    constexpr value_type const *end(size_type b) const { return content.data() + off[b + 1]; }

    FlexiStorage<Counter, ONES> off;
    FlexiStorage<value_type, SIZE> content;
    int32_t psws;  // prefix-scan working space
  };

  template <
      // type stored in the container (usually an index in a vector of the input values)
      typename I,
      // number of "Ones"  +1. If cms::alpakatools::kDynamicSize is initialized at runtime using external storage
      FlexiStorageBase::size_type ONES,
      // max number of element. If cms::alpakatools::kDynamicSize is initialized at runtime using external storage
      FlexiStorageBase::size_type SIZE>
  class OneToManyAssocSequential : public OneToManyAssocBase<I, ONES, SIZE> {
  public:
    using size_type = typename OneToManyAssocBase<I, ONES, SIZE>::size_type;
    using value_type = typename OneToManyAssocBase<I, ONES, SIZE>::value_type;

    template <aV2P::concepts::Acc TAcc>
    AV2P_FN_HOST_ACC AV2P_FN_INLINE size_type
    bulkFill(const TAcc &acc, AtomicPairCounter &apc, value_type const *v, size_type n) {
      auto c = apc.inc_add(acc, n);
      if (c.first >= this->nOnes())  // overflow!
        return kOverflow;
      this->off[c.first] = c.second;
      for (size_type j = 0; j < n; ++j)
        this->content[c.second + j] = v[j];
      return c.first;
    }

    AV2P_FN_HOST_ACC AV2P_FN_INLINE void bulkFinalize(AtomicPairCounter const &apc) {
      this->off[apc.get().first] = apc.get().second;
    }

    template <aV2P::concepts::Acc TAcc>
    AV2P_FN_HOST_ACC AV2P_FN_INLINE void bulkFinalizeFill(const TAcc &acc, AtomicPairCounter const &apc) {
      size_type f = apc.get().first;
      auto s = apc.get().second;
      if (f >= this->nOnes()) {  // overflow!
        this->off[this->nOnes()] = this->off[this->nOnes() - 1];
        return;
      }
      auto first = f + aV2P::getIdx<aV2P::Grid, aV2P::Threads>(acc)[0];
      for (size_type i = first; i < this->totOnes(); i += aV2P::getWorkDiv<aV2P::Grid, aV2P::Threads>(acc)[0]) {
        this->off[i] = s;
      }
    }

    struct finalizeBulk {
      template <aV2P::concepts::Acc TAcc>
      AV2P_FN_ACC void operator()(const TAcc &acc,
                                    AtomicPairCounter const *apc,
                                    OneToManyAssocSequential *__restrict__ assoc) const {
        assoc->bulkFinalizeFill(acc, *apc);
      }
    };
  };

  template <
      // type stored in the container (usually an index in a vector of the input values)
      typename I,
      // number of "Ones"  +1. If cms::alpakatools::kDynamicSize is initialized at runtime using external storage
      FlexiStorageBase::size_type ONES,
      // max number of element. If cms::alpakatools::kDynamicSize is initialized at runtime using external storage
      FlexiStorageBase::size_type SIZE>
  class OneToManyAssocRandomAccess : public OneToManyAssocBase<I, ONES, SIZE> {
  public:
    using Counter = typename OneToManyAssocBase<I, ONES, SIZE>::Counter;
    using View = typename OneToManyAssocBase<I, ONES, SIZE>::View;
    using size_type = typename OneToManyAssocBase<I, ONES, SIZE>::size_type;
    using value_type = typename OneToManyAssocBase<I, ONES, SIZE>::value_type;

    template <aV2P::concepts::Acc TAcc>
    AV2P_FN_HOST_ACC AV2P_FN_INLINE void finalize(const TAcc &acc, Counter *ws = nullptr) {
      AV2P_ASSERT_ACC(this->off[this->totOnes() - 1] == 0);
      blockPrefixScan(acc, this->off.data(), this->totOnes(), ws);
      AV2P_ASSERT_ACC(this->off[this->totOnes() - 1] == this->off[this->totOnes() - 2]);
    }

    AV2P_FN_HOST_ACC AV2P_FN_INLINE void finalize() {
      // Single thread finalize.
      for (size_type i = 1; i < this->totOnes(); ++i)
        this->off[i] += this->off[i - 1];
    }

    template <aV2P::concepts::Acc TAcc, typename TQueue>
    AV2P_FN_INLINE static void launchFinalize(OneToManyAssocRandomAccess *h, TQueue &queue) {
      View view = {h, nullptr, nullptr, kDynamicSize, kDynamicSize};
      launchFinalize<TAcc>(view, queue);
    }

    template <aV2P::concepts::Acc TAcc, typename TQueue>
    AV2P_FN_INLINE static void launchFinalize(View view, TQueue &queue) {
      // View stores a base pointer, we need to upcast back...
      auto h = static_cast<OneToManyAssocRandomAccess *>(view.assoc);
      AV2P_ASSERT_ACC(h);
      if constexpr (!requires_single_thread_per_block_v<TAcc>) {
        Counter *poff = (Counter *)((char *)(h) + offsetof(OneToManyAssocRandomAccess, off));
        auto nOnes = OneToManyAssocRandomAccess::ctNOnes();
        if constexpr (OneToManyAssocRandomAccess::ctNOnes() == kDynamicSize) {
          AV2P_ASSERT_ACC(view.offStorage);
          AV2P_ASSERT_ACC(view.offSize > 0);
          nOnes = view.offSize;
          poff = view.offStorage;
        }
        AV2P_ASSERT_ACC(nOnes > 0);
        int32_t *ppsws = (int32_t *)((char *)(h) + offsetof(OneToManyAssocRandomAccess, psws));
        auto nthreads = 1024;
        auto nblocks = (nOnes + nthreads - 1) / nthreads;
        auto workDiv = cms::alpakatools::make_workdiv<TAcc>(nblocks, nthreads);
        cms::alpakatools::checkSharedMemoryPrefixScan<TAcc>(nOnes, nblocks, aV2P::getDev(queue));
        aV2P::exec<TAcc>(queue,
                           workDiv,
                           multiBlockPrefixScan<Counter>(),
                           poff,
                           poff,
                           nOnes,
                           nblocks,
                           ppsws,
                           aV2P::getPreferredWarpSize(aV2P::getDev(queue)));
      } else {
        h->finalize();
      }
    }
  };

}  // namespace cms::alpakatools

#endif  //HeterogeneousCore_AlpakaInterface_interface_OneToManyAssoc_h
