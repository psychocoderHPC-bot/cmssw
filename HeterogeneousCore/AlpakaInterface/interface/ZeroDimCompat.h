#ifndef HeterogeneousCore_AlpakaInterface_interface_ZeroDimCompat_h
#define HeterogeneousCore_AlpakaInterface_interface_ZeroDimCompat_h

// SPDX-License-Identifier: LGPL-3.0-only
//
// CMSSW-only, header-only bridge: alpaka 2.2 zero-dimensional (scalar) memory
// on aV2P, built on top of aV2P 1-D extent-1 objects.
//
// aV2P keeps the 2.2 Vec<DimInt<0>, T> type, but the native alpaka 3 allocation
// and view construction reject 0-D. This bridge models every CMSSW scalar as a
// real aV2P 1-D, extent-1 allocation while advertising Dim == 0 to aV2P:
//
//   - the owning wrapper holds an aV2P::Buf<TDev, T, DimInt<1u>, Idx> of extent 1
//   - the non-owning wrapper holds a bare T*
//   - both advertise 0-D extents (product == 1), zero offsets and zero pitches
//
// aV2P's host operations (memcpy, memset, getExtentProduct, std::data and the
// view accessors) reach a view only through Dim/Elem/Dev/GetExtents/
// GetPtrNative, so they treat the wrappers like a 2.2 0-D buffer and copy one
// element. The 0-D CreateTaskMemcpy/CreateTaskMemset specialisations below
// translate the operation to the exactly equivalent 1-D extent-1 views because
// the generic aV2P path cannot build a 0-D alpaka 3 View.
//
// Downstream CMSSW code keeps the 2.2 call syntax:
//   std::optional<...>, make_device_buffer<T>(q), memcpy(q, dst, src),
//   memset(q, *view, 0), *buf.data(), view.data()
//
// This header is included by memory.h; it must not include memory.h or
// CopyToDevice.h. The scalar CopyToDevice specialisation lives in CopyToDevice.h.

#include <cstdint>
#include <iterator>  // std::data
#include <utility>   // std::move

#include <aV2P/alpaka.hpp>

#include "HeterogeneousCore/AlpakaInterface/interface/AllocatorPolicy.h"
#include "HeterogeneousCore/AlpakaInterface/interface/CachedBufAlloc.h"
#include "HeterogeneousCore/AlpakaInterface/interface/config.h"
#include "HeterogeneousCore/AlpakaInterface/interface/devices.h"
#include "HeterogeneousCore/AlpakaInterface/interface/host.h"

namespace cms::alpakatools::zerodim {

  using namespace alpaka_common;  // DevHost, Vec, Dim0D, Idx

  namespace aV = aV2P;

  // Extent vector types the bridge advertises/consumes.
  using Ext1 = aV::Vec<aV::DimInt<1u>, Idx>;  // 1-D (element type = aV2P idx)
  using Ext0 = aV::Vec<aV::DimInt<0u>, Idx>;  // 0-D (product == 1), advertised to aV2P

  // --------------------------------------------------------------------------
  // Non-owning 0-D view over one T (wraps a bare T* + device).
  // 2.2: aV2P::ViewPlainPtr<TDev, T, Dim0D, Idx>(&x, dev, Scalar{})
  // --------------------------------------------------------------------------
  template <typename TDev, typename T>
  struct ScalarView {
    T* m_pMem;    // element pointer (aV2P free ops read it via GetPtrNative)
    TDev m_dev;   // aV2P device (host or GPU)

    AV2P_FN_HOST ScalarView(T* p, TDev const& dev) : m_pMem(p), m_dev(dev) {}

    // member accessors used directly at CMSSW sites (2.2 0-D forms)
    AV2P_FN_HOST auto data() -> T* { return m_pMem; }
    AV2P_FN_HOST auto data() const -> T const* { return m_pMem; }
    AV2P_FN_HOST auto operator*() -> T& { return *m_pMem; }
    AV2P_FN_HOST auto operator*() const -> T const& { return *m_pMem; }
    AV2P_FN_HOST auto operator->() -> T* { return m_pMem; }
    AV2P_FN_HOST auto operator->() const -> T const* { return m_pMem; }
  };

  // --------------------------------------------------------------------------
  // Owning 0-D buffer: stores a real aV2P 1-D extent-1 Buf (RAII device memory)
  // and advertises 0-D.
  // 2.2: aV2P::Buf<TDev, T, Dim0D, Idx> from allocBuf / allocMappedBuf /
  //      allocAsyncBuf / allocCachedBuf (..., Scalar{})
  // --------------------------------------------------------------------------
  template <typename TDev, typename T>
  struct ScalarBuf {
    using Storage = aV::Buf<TDev, T, aV::DimInt<1u>, Idx>;
    Storage m_storage;  // OWNED 1-D extent-1 aV2P allocation
    T* m_pMem;          // cached element pointer
    TDev m_dev;

    AV2P_FN_HOST explicit ScalarBuf(Storage&& buf, TDev const& dev)
        : m_storage(std::move(buf)), m_pMem(std::data(m_storage)), m_dev(dev) {}

    AV2P_FN_HOST auto data() -> T* { return m_pMem; }
    AV2P_FN_HOST auto data() const -> T const* { return m_pMem; }
    AV2P_FN_HOST auto operator*() -> T& { return *m_pMem; }
    AV2P_FN_HOST auto operator*() const -> T const& { return *m_pMem; }
    AV2P_FN_HOST auto operator->() -> T* { return m_pMem; }
    AV2P_FN_HOST auto operator->() const -> T const* { return m_pMem; }
  };

  // PUBLIC type aliases -- the scalar (non-array) halves of memory.h.
  // detail::buffer_type<TDev,T>::type 2.2 aV2P::Buf<TDev,T,Dim0D,Idx>          -> ScalarBuf<TDev,T>
  // detail::view_type<TDev,T>::type   2.2 aV2P::ViewPlainPtr<TDev,T,Dim0D,Idx> -> ScalarView<TDev,T>
  // The T[]/T[N] 1-D specialisations are unchanged in memory.h.
  template <typename TDev, typename T>
  using host_buffer = ScalarBuf<TDev, T>;
  template <typename TDev, typename T>
  using device_buffer = ScalarBuf<TDev, T>;
  template <typename TDev, typename T>
  using host_view = ScalarView<TDev, T>;
  template <typename TDev, typename T>
  using device_view = ScalarView<TDev, T>;

}  // namespace cms::alpakatools::zerodim

// ----------------------------------------------------------------------------
// aV2P view-trait specialisations, in the aV2P::trait namespace. These are the
// only aV2P contracts the bridge depends on; every one mirrors the
// ViewPlainPtr.hpp / ViewStdVector.hpp view-trait pattern.
// ----------------------------------------------------------------------------
template <typename TDev, typename T>
struct aV2P::trait::DimType<cms::alpakatools::zerodim::ScalarView<TDev, T>> {
  using type = aV2P::DimInt<0u>;
};
template <typename TDev, typename T>
struct aV2P::trait::ElemType<cms::alpakatools::zerodim::ScalarView<TDev, T>> {
  using type = T;
};
template <typename TDev, typename T>
struct aV2P::trait::IdxType<cms::alpakatools::zerodim::ScalarView<TDev, T>> {
  using type = cms::alpakatools::zerodim::Idx;
};
template <typename TDev, typename T>
struct aV2P::trait::DevType<cms::alpakatools::zerodim::ScalarView<TDev, T>> {
  using type = aV2P::Dev<TDev>;
};
template <typename TDev, typename T>
struct aV2P::trait::GetDev<cms::alpakatools::zerodim::ScalarView<TDev, T>> {
  AV2P_FN_HOST static auto getDev(cms::alpakatools::zerodim::ScalarView<TDev, T> const& view) -> aV2P::Dev<TDev> {
    return view.m_dev;
  }
};
template <typename TDev, typename T>
struct aV2P::trait::GetExtents<cms::alpakatools::zerodim::ScalarView<TDev, T>> {
  AV2P_FN_HOST auto operator()(cms::alpakatools::zerodim::ScalarView<TDev, T> const&) const
      -> cms::alpakatools::zerodim::Ext0  // 0 extents; product == 1
  {
    return {};
  }
};
template <typename TDev, typename T>
struct aV2P::trait::GetOffsets<cms::alpakatools::zerodim::ScalarView<TDev, T>> {
  AV2P_FN_HOST auto operator()(cms::alpakatools::zerodim::ScalarView<TDev, T> const&) const
      -> cms::alpakatools::zerodim::Ext0 {
    return {};
  }
};
template <typename TDev, typename T>
struct aV2P::trait::GetPtrNative<cms::alpakatools::zerodim::ScalarView<TDev, T>> {
  AV2P_FN_HOST static auto getPtrNative(cms::alpakatools::zerodim::ScalarView<TDev, T> const& v) -> T const* {
    return v.m_pMem;
  }
  AV2P_FN_HOST static auto getPtrNative(cms::alpakatools::zerodim::ScalarView<TDev, T>& v) -> T* { return v.m_pMem; }
};
// aV2P's adapter layer asks every view for its pitches; 0-D has no pitch vector.
template <typename TDev, typename T>
struct aV2P::trait::GetPitchesInBytes<cms::alpakatools::zerodim::ScalarView<TDev, T>> {
  AV2P_FN_HOST auto operator()(cms::alpakatools::zerodim::ScalarView<TDev, T> const&) const
      -> cms::alpakatools::zerodim::Ext0 {
    return {};
  }
};

// ---------------------------------------------------------------------------
// Identical specialisation set for the owning buffer.
// ---------------------------------------------------------------------------
template <typename TDev, typename T>
struct aV2P::trait::DimType<cms::alpakatools::zerodim::ScalarBuf<TDev, T>> {
  using type = aV2P::DimInt<0u>;
};
template <typename TDev, typename T>
struct aV2P::trait::ElemType<cms::alpakatools::zerodim::ScalarBuf<TDev, T>> {
  using type = T;
};
template <typename TDev, typename T>
struct aV2P::trait::IdxType<cms::alpakatools::zerodim::ScalarBuf<TDev, T>> {
  using type = cms::alpakatools::zerodim::Idx;
};
template <typename TDev, typename T>
struct aV2P::trait::DevType<cms::alpakatools::zerodim::ScalarBuf<TDev, T>> {
  using type = aV2P::Dev<TDev>;
};
template <typename TDev, typename T>
struct aV2P::trait::GetDev<cms::alpakatools::zerodim::ScalarBuf<TDev, T>> {
  AV2P_FN_HOST static auto getDev(cms::alpakatools::zerodim::ScalarBuf<TDev, T> const& view) -> aV2P::Dev<TDev> {
    return view.m_dev;
  }
};
template <typename TDev, typename T>
struct aV2P::trait::GetExtents<cms::alpakatools::zerodim::ScalarBuf<TDev, T>> {
  AV2P_FN_HOST auto operator()(cms::alpakatools::zerodim::ScalarBuf<TDev, T> const&) const
      -> cms::alpakatools::zerodim::Ext0 {
    return {};
  }
};
template <typename TDev, typename T>
struct aV2P::trait::GetOffsets<cms::alpakatools::zerodim::ScalarBuf<TDev, T>> {
  AV2P_FN_HOST auto operator()(cms::alpakatools::zerodim::ScalarBuf<TDev, T> const&) const
      -> cms::alpakatools::zerodim::Ext0 {
    return {};
  }
};
template <typename TDev, typename T>
struct aV2P::trait::GetPtrNative<cms::alpakatools::zerodim::ScalarBuf<TDev, T>> {
  AV2P_FN_HOST static auto getPtrNative(cms::alpakatools::zerodim::ScalarBuf<TDev, T> const& view) -> T const* {
    return view.m_pMem;
  }
  AV2P_FN_HOST static auto getPtrNative(cms::alpakatools::zerodim::ScalarBuf<TDev, T>& view) -> T* { return view.m_pMem; }
};
template <typename TDev, typename T>
struct aV2P::trait::GetPitchesInBytes<cms::alpakatools::zerodim::ScalarBuf<TDev, T>> {
  AV2P_FN_HOST auto operator()(cms::alpakatools::zerodim::ScalarBuf<TDev, T> const&) const
      -> cms::alpakatools::zerodim::Ext0 {
    return {};
  }
};

// ----------------------------------------------------------------------------
// 0-D memory-operation specialisations.
//
// aV2P's generic CreateTaskMemcpy/CreateTaskMemset unconditionally convert the
// views to alpaka 3 Views. That conversion builds alpaka::View<..., Extents>
// whose base is MdSpan<T, Extents::UniVec, ...>; the alpaka 3 Vec<T,0>
// specialisation has no UniVec, so a 0-D view cannot reach the native copy path
// even though the allocation is a valid 1-D extent-1 buffer. These two
// specialisations translate the 0-D operation to the equivalent 1-D extent-1
// views (same pointer, same device, one element) and reuse aV2P's native
// implementation.
// ----------------------------------------------------------------------------
template <typename TDevSpecDst, typename TDevSpecSrc>
struct aV2P::trait::CreateTaskMemcpy<aV2P::DimInt<0u>, aV2P::DevAlpaka3<TDevSpecDst>, aV2P::DevAlpaka3<TDevSpecSrc>> {
  template <typename TExtent, typename TViewSrc, typename TViewDstFwd>
  AV2P_FN_HOST static auto createTaskMemcpy(TViewDstFwd&& viewDst, TViewSrc const& viewSrc, TExtent const&) {
    using Dst = std::remove_reference_t<TViewDstFwd>;
    using Src = TViewSrc;
    using DstDev = aV2P::Dev<Dst>;
    using SrcDev = aV2P::Dev<Src>;
    using DstElem = std::remove_const_t<aV2P::Elem<Dst>>;
    using SrcElem = std::remove_const_t<aV2P::Elem<Src>>;
    using Ext1 = aV2P::Vec<aV2P::DimInt<1u>, cms::alpakatools::zerodim::Idx>;

    Ext1 const extent1{1u};
    // getPtrNative() returns a const pointer for const sources; the copy never
    // writes through the source view.
    auto* pDst = const_cast<DstElem*>(aV2P::getPtrNative(viewDst));
    auto* pSrc = const_cast<SrcElem*>(aV2P::getPtrNative(viewSrc));
    aV2P::ViewPlainPtr<DstDev, DstElem, aV2P::DimInt<1u>, cms::alpakatools::zerodim::Idx> dst1{
        pDst, aV2P::getDev(viewDst), extent1};
    aV2P::ViewPlainPtr<SrcDev, SrcElem, aV2P::DimInt<1u>, cms::alpakatools::zerodim::Idx> src1{
        pSrc, aV2P::getDev(viewSrc), extent1};
    return aV2P::trait::CreateTaskMemcpy<aV2P::DimInt<1u>, DstDev, SrcDev>::createTaskMemcpy(dst1, src1, extent1);
  }
};

template <typename TDevSpec>
struct aV2P::trait::CreateTaskMemset<aV2P::DimInt<0u>, aV2P::DevAlpaka3<TDevSpec>> {
  template <typename TExtent, typename TView>
  AV2P_FN_HOST static auto createTaskMemset(TView&& view, std::uint8_t const& byte, TExtent const&) {
    using View = std::remove_reference_t<TView>;
    using Dev = aV2P::Dev<View>;
    using Elem = std::remove_const_t<aV2P::Elem<View>>;
    using Ext1 = aV2P::Vec<aV2P::DimInt<1u>, cms::alpakatools::zerodim::Idx>;

    Ext1 const extent1{1u};
    aV2P::ViewPlainPtr<Dev, Elem, aV2P::DimInt<1u>, cms::alpakatools::zerodim::Idx> view1{
        const_cast<Elem*>(aV2P::getPtrNative(view)), aV2P::getDev(view), extent1};
    return aV2P::trait::CreateTaskMemset<aV2P::DimInt<1u>, Dev>::createTaskMemset(view1, byte, extent1);
  }
};

// ----------------------------------------------------------------------------
// FACTORY FUNCTIONS -- the scalar (non-array) overloads of memory.h's
// make_host_buffer / make_device_buffer / make_device_view / make_host_view.
// Array overloads with (..., Extent) stay in memory.h, untouched.
// Every allocation is aV2P with an extent-1 (1-D) extent vector; the wrapper
// advertises Dim0D/0-extents so aV2P::memcpy copies exactly one element.
// ----------------------------------------------------------------------------
namespace cms::alpakatools::zerodim {

  // host buffers (memory.h scalar overloads)
  template <typename T>
  auto make_host_buffer() -> host_buffer<DevHost, T> {
    auto buf = aV2P::allocBuf<T, Idx>(host(), Ext1{1u});
    return host_buffer<DevHost, T>(std::move(buf), host());
  }

  template <typename T, typename TPlatform>
  auto make_host_buffer() -> host_buffer<DevHost, T> {
    auto buf = aV2P::allocMappedBuf<T, Idx>(host(), platform<TPlatform>(), Ext1{1u});
    return host_buffer<DevHost, T>(std::move(buf), host());
  }

  template <typename T, typename TQueue>
  auto make_host_buffer(TQueue const& queue) -> host_buffer<DevHost, T> {
    // As in the 2.2 implementation, the host allocator policy is selected by
    // the device associated to the queue (pinned host memory), not by DevHost.
    if constexpr (host_allocator_policy<aV2P::Dev<TQueue>> == AllocatorPolicy::Caching) {
      auto buf = allocCachedBuf<T, Idx>(host(), queue, Ext1{1u});
      return host_buffer<DevHost, T>(std::move(buf), host());
    } else {
      using Platform = aV2P::Platform<aV2P::Dev<TQueue>>;
      auto buf = aV2P::allocMappedBuf<T, Idx>(host(), platform<Platform>(), Ext1{1u});
      return host_buffer<DevHost, T>(std::move(buf), host());
    }
  }

  // device buffers (memory.h scalar overloads)
  template <typename T, typename TDev>
    requires aV2P::concepts::Device<TDev>
  auto make_device_buffer(TDev const& device) -> device_buffer<TDev, T> {
    auto buf = aV2P::allocBuf<T, Idx>(device, Ext1{1u});
    return device_buffer<TDev, T>(std::move(buf), device);
  }

  template <typename T, typename TQueue>
    requires aV2P::concepts::Queue<TQueue>
  auto make_device_buffer(TQueue const& queue) -> device_buffer<aV2P::Dev<TQueue>, T> {
    using TDev = aV2P::Dev<TQueue>;
    auto const dev = aV2P::getDev(queue);
    if constexpr (device_allocator_policy<TDev> == AllocatorPolicy::Caching) {
      auto buf = allocCachedBuf<T, Idx>(dev, queue, Ext1{1u});
      return device_buffer<TDev, T>(std::move(buf), dev);
    }
    if constexpr (device_allocator_policy<TDev> == AllocatorPolicy::Asynchronous) {
      auto buf = aV2P::allocAsyncBuf<T, Idx>(queue, Ext1{1u});
      return device_buffer<TDev, T>(std::move(buf), dev);
    }
    if constexpr (device_allocator_policy<TDev> == AllocatorPolicy::Synchronous) {
      auto buf = aV2P::allocBuf<T, Idx>(dev, Ext1{1u});
      return device_buffer<TDev, T>(std::move(buf), dev);
    }
    auto buf = aV2P::allocBuf<T, Idx>(dev, Ext1{1u});  // default: synchronous
    return device_buffer<TDev, T>(std::move(buf), dev);
  }

  // views (memory.h scalar overloads) -- non-owning, wrap a single T&
  template <typename TDev, typename T>
    requires aV2P::concepts::Device<TDev>
  auto make_device_view(TDev const& device, T& data) -> device_view<TDev, T> {
    return device_view<TDev, T>(&data, device);
  }

  template <typename T, typename TQueue>
    requires aV2P::concepts::Queue<TQueue>
  auto make_device_view(TQueue const& queue, T& data) -> device_view<aV2P::Dev<TQueue>, T> {
    return device_view<aV2P::Dev<TQueue>, T>(&data, aV2P::getDev(queue));
  }

  template <typename T>
  auto make_host_view(T& data) -> host_view<DevHost, T> {
    return host_view<DevHost, T>(&data, host());
  }

}  // namespace cms::alpakatools::zerodim

#endif  // HeterogeneousCore_AlpakaInterface_interface_ZeroDimCompat_h
