#ifndef HeterogeneousCore_AlpakaInterface_interface_ZeroDimCompat_h
#define HeterogeneousCore_AlpakaInterface_interface_ZeroDimCompat_h

// SPDX-License-Identifier: LGPL-3.0-only
//
// CMSSW glue for aV2P's zero-dimensional (scalar) memory support.
//
// aV2P >= 70bbdb18a ships aV2P/zerodim/ZeroDim.hpp: ScalarBuf/ScalarView model
// a scalar as a real 1-D extent-1 allocation that advertises DimInt<0u>, plus
// the view traits and the 0-D memcpy/memset specialisations. This header does
// not redefine any of that; it aliases the aV2P types into the CMSSW namespace
// and keeps the CMSSW allocator-policy selection for the scalar factories.
//
// aV2P's own scalar factories use aV2P::zerodim::host_allocator_policy /
// device_allocator_policy and its allocCachedBuf hook. CMSSW keys its caching
// allocator on cms::alpakatools::{host,device}_allocator_policy and routes
// through cms::alpakatools::allocCachedBuf, so the scalar factories stay here.
// Downstream CMSSW code keeps the 2.2 call syntax unchanged.

#include <type_traits>
#include <utility>

#include <aV2P/alpaka.hpp>
#include <aV2P/zerodim/ZeroDim.hpp>

#include "HeterogeneousCore/AlpakaInterface/interface/AllocatorPolicy.h"
#include "HeterogeneousCore/AlpakaInterface/interface/CachedBufAlloc.h"
#include "HeterogeneousCore/AlpakaInterface/interface/config.h"
#include "HeterogeneousCore/AlpakaInterface/interface/devices.h"
#include "HeterogeneousCore/AlpakaInterface/interface/host.h"

namespace cms::alpakatools::zerodim {

  using namespace alpaka_common;  // DevHost, Idx

  namespace aV = aV2P;

  // The scalar wrappers and their aV2P view traits come from aV2P.
  template <typename TDev, typename T>
  using ScalarBuf = aV::zerodim::ScalarBuf<TDev, T>;
  template <typename TDev, typename T>
  using ScalarView = aV::zerodim::ScalarView<TDev, T>;

  // Scalar halves of memory.h. The owning wrapper stores a 1-D extent-1
  // aV2P allocation; aV2P::zerodim::Idx is the index type of that storage.
  template <typename TDev, typename T>
  using host_buffer = ScalarBuf<TDev, T>;
  template <typename TDev, typename T>
  using device_buffer = ScalarBuf<TDev, T>;
  template <typename TDev, typename T>
  using host_view = ScalarView<TDev, T>;
  template <typename TDev, typename T>
  using device_view = ScalarView<TDev, T>;

  using Ext0 = aV::zerodim::Ext0;
  using Ext1 = aV::zerodim::Ext1;
  using StorageIdx = aV::zerodim::Idx;

  template <typename T>
  auto make_host_buffer() -> host_buffer<DevHost, T> {
    auto buf = aV::allocBuf<T, StorageIdx>(host(), Ext1{1u});
    return host_buffer<DevHost, T>(std::move(buf), host());
  }

  template <typename T, typename TPlatform>
  auto make_host_buffer() -> host_buffer<DevHost, T> {
    auto buf = aV::allocMappedBuf<T, StorageIdx>(host(), platform<TPlatform>(), Ext1{1u});
    return host_buffer<DevHost, T>(std::move(buf), host());
  }

  template <typename T, typename TQueue>
  auto make_host_buffer(TQueue const& queue) -> host_buffer<DevHost, T> {
    // As in the 2.2 implementation, the host allocator policy is selected by
    // the device associated to the queue (pinned host memory), not by DevHost.
    if constexpr (host_allocator_policy<aV::Dev<TQueue>> == AllocatorPolicy::Caching) {
      auto buf = allocCachedBuf<T, StorageIdx>(host(), queue, Ext1{1u});
      return host_buffer<DevHost, T>(std::move(buf), host());
    } else {
      using Platform = aV::Platform<aV::Dev<TQueue>>;
      auto buf = aV::allocMappedBuf<T, StorageIdx>(host(), platform<Platform>(), Ext1{1u});
      return host_buffer<DevHost, T>(std::move(buf), host());
    }
  }

  template <typename T, typename TDev>
    requires aV::concepts::Device<TDev>
  auto make_device_buffer(TDev const& device) -> device_buffer<TDev, T> {
    auto buf = aV::allocBuf<T, StorageIdx>(device, Ext1{1u});
    return device_buffer<TDev, T>(std::move(buf), device);
  }

  template <typename T, typename TQueue>
    requires aV::concepts::Queue<TQueue>
  auto make_device_buffer(TQueue const& queue) -> device_buffer<aV::Dev<TQueue>, T> {
    using TDev = aV::Dev<TQueue>;
    auto const dev = aV::getDev(queue);
    if constexpr (device_allocator_policy<TDev> == AllocatorPolicy::Caching) {
      auto buf = allocCachedBuf<T, StorageIdx>(dev, queue, Ext1{1u});
      return device_buffer<TDev, T>(std::move(buf), dev);
    }
    if constexpr (device_allocator_policy<TDev> == AllocatorPolicy::Asynchronous) {
      auto buf = aV::allocAsyncBuf<T, StorageIdx>(queue, Ext1{1u});
      return device_buffer<TDev, T>(std::move(buf), dev);
    }
    if constexpr (device_allocator_policy<TDev> == AllocatorPolicy::Synchronous) {
      auto buf = aV::allocBuf<T, StorageIdx>(dev, Ext1{1u});
      return device_buffer<TDev, T>(std::move(buf), dev);
    }
    auto buf = aV::allocBuf<T, StorageIdx>(dev, Ext1{1u});  // default: synchronous
    return device_buffer<TDev, T>(std::move(buf), dev);
  }

  template <typename TDev, typename T>
    requires aV::concepts::Device<TDev>
  auto make_device_view(TDev const& device, T& data) -> device_view<TDev, T> {
    return device_view<TDev, T>(&data, device);
  }

  template <typename T, typename TQueue>
    requires aV::concepts::Queue<TQueue>
  auto make_device_view(TQueue const& queue, T& data) -> device_view<aV::Dev<TQueue>, T> {
    return device_view<aV::Dev<TQueue>, T>(&data, aV::getDev(queue));
  }

  template <typename T>
  auto make_host_view(T& data) -> host_view<DevHost, T> {
    return host_view<DevHost, T>(&data, host());
  }

}  // namespace cms::alpakatools::zerodim

#endif  // HeterogeneousCore_AlpakaInterface_interface_ZeroDimCompat_h
