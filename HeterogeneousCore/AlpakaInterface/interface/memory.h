#ifndef HeterogeneousCore_AlpakaInterface_interface_memory_h
#define HeterogeneousCore_AlpakaInterface_interface_memory_h

#include <type_traits>

#include <span>

#include <aV2P/alpaka.hpp>

#include "HeterogeneousCore/AlpakaInterface/interface/AllocatorPolicy.h"
#include "HeterogeneousCore/AlpakaInterface/interface/CachedBufAlloc.h"
#include "HeterogeneousCore/AlpakaInterface/interface/config.h"
#include "HeterogeneousCore/AlpakaInterface/interface/devices.h"
#include "HeterogeneousCore/AlpakaInterface/interface/host.h"
#include "HeterogeneousCore/AlpakaInterface/interface/traits.h"

namespace cms::alpakatools {

  // for Extent, Dim1D, Idx
  using namespace alpaka_common;

  // type deduction helpers
  namespace detail {

    template <typename TDev, typename T, typename = std::enable_if_t<aV2P::concepts::Device<TDev>>>
    struct buffer_type {
      using type = aV2P::Buf<TDev, T, Dim0D, Idx>;
    };

    template <typename TDev, typename T>
    struct buffer_type<TDev, T[]> {
      using type = aV2P::Buf<TDev, T, Dim1D, Idx>;
    };

    template <typename TDev, typename T, int N>
    struct buffer_type<TDev, T[N]> {
      using type = aV2P::Buf<TDev, T, Dim1D, Idx>;
    };

    template <typename TDev, typename T, typename = std::enable_if_t<aV2P::concepts::Device<TDev>>>
    struct view_type {
      using type = aV2P::ViewPlainPtr<TDev, T, Dim0D, Idx>;
    };

    template <typename TDev, typename T>
    struct view_type<TDev, T[]> {
      using type = aV2P::ViewPlainPtr<TDev, T, Dim1D, Idx>;
    };

    template <typename TDev, typename T, int N>
    struct view_type<TDev, T[N]> {
      using type = aV2P::ViewPlainPtr<TDev, T, Dim1D, Idx>;
    };

  }  // namespace detail

  // scalar and 1-dimensional host buffers

  template <typename T>
  using host_buffer = typename detail::buffer_type<DevHost, T>::type;

  template <typename T>
  using const_host_buffer = aV2P::ViewConst<host_buffer<T>>;

  // non-cached, non-pinned, scalar and 1-dimensional host buffers

  template <typename T>
  std::enable_if_t<not std::is_array_v<T>, host_buffer<T>> make_host_buffer() {
    return aV2P::allocBuf<T, Idx>(host(), Scalar{});
  }

  template <typename T>
  std::enable_if_t<cms::is_unbounded_array_v<T> and not std::is_array_v<std::remove_extent_t<T>>, host_buffer<T>>
  make_host_buffer(Extent extent) {
    return aV2P::allocBuf<std::remove_extent_t<T>, Idx>(host(), Vec1D{extent});
  }

  template <typename T>
  std::enable_if_t<cms::is_bounded_array_v<T> and not std::is_array_v<std::remove_extent_t<T>>, host_buffer<T>>
  make_host_buffer() {
    return aV2P::allocBuf<std::remove_extent_t<T>, Idx>(host(), Vec1D{std::extent_v<T>});
  }

  // non-cached, pinned, scalar and 1-dimensional host buffers
  // the memory is pinned according to the device associated to the platform

  template <typename T, typename TPlatform>
  std::enable_if_t<not std::is_array_v<T>, host_buffer<T>> make_host_buffer() {
    using Platform = TPlatform;
    return aV2P::allocMappedBuf<T, Idx>(host(), platform<Platform>(), Scalar{});
  }

  template <typename T, typename TPlatform>
  std::enable_if_t<cms::is_unbounded_array_v<T> and not std::is_array_v<std::remove_extent_t<T>>, host_buffer<T>>
  make_host_buffer(Extent extent) {
    using Platform = TPlatform;
    return aV2P::allocMappedBuf<std::remove_extent_t<T>, Idx>(host(), platform<Platform>(), Vec1D{extent});
  }

  template <typename T, typename TPlatform>
  std::enable_if_t<cms::is_bounded_array_v<T> and not std::is_array_v<std::remove_extent_t<T>>, host_buffer<T>>
  make_host_buffer() {
    using Platform = TPlatform;
    return aV2P::allocMappedBuf<std::remove_extent_t<T>, Idx>(host(), platform<Platform>(), Vec1D{std::extent_v<T>});
  }

  // potentially cached, pinned, scalar and 1-dimensional host buffers, associated to a work queue
  // the memory is pinned according to the device associated to the queue

  template <typename T, typename TQueue>
  std::enable_if_t<aV2P::concepts::Queue<TQueue> and not std::is_array_v<T>, host_buffer<T>> make_host_buffer(
      TQueue const& queue) {
    if constexpr (host_allocator_policy<aV2P::Dev<TQueue>> == AllocatorPolicy::Caching) {
      return allocCachedBuf<T, Idx>(host(), queue, Scalar{});
    } else {
      using Platform = aV2P::Platform<aV2P::Dev<TQueue>>;
      return aV2P::allocMappedBuf<T, Idx>(host(), platform<Platform>(), Scalar{});
    }
  }

  template <typename T, typename TQueue>
  std::enable_if_t<aV2P::concepts::Queue<TQueue> and cms::is_unbounded_array_v<T> and
                       not std::is_array_v<std::remove_extent_t<T>>,
                   host_buffer<T>>
  make_host_buffer(TQueue const& queue, Extent extent) {
    if constexpr (host_allocator_policy<aV2P::Dev<TQueue>> == AllocatorPolicy::Caching) {
      return allocCachedBuf<std::remove_extent_t<T>, Idx>(host(), queue, Vec1D{extent});
    } else {
      using Platform = aV2P::Platform<aV2P::Dev<TQueue>>;
      return aV2P::allocMappedBuf<std::remove_extent_t<T>, Idx>(host(), platform<Platform>(), Vec1D{extent});
    }
  }

  template <typename T, typename TQueue>
  std::enable_if_t<aV2P::concepts::Queue<TQueue> and cms::is_bounded_array_v<T> and
                       not std::is_array_v<std::remove_extent_t<T>>,
                   host_buffer<T>>
  make_host_buffer(TQueue const& queue) {
    if constexpr (host_allocator_policy<aV2P::Dev<TQueue>> == AllocatorPolicy::Caching) {
      return allocCachedBuf<std::remove_extent_t<T>, Idx>(host(), queue, Vec1D{std::extent_v<T>});
    } else {
      using Platform = aV2P::Platform<aV2P::Dev<TQueue>>;
      return aV2P::allocMappedBuf<std::remove_extent_t<T>, Idx>(
          host(), platform<Platform>(), Vec1D{std::extent_v<T>});
    }
  }

  // scalar and 1-dimensional host views

  template <typename T>
  using host_view = typename detail::view_type<DevHost, T>::type;

  template <typename T>
  std::enable_if_t<not std::is_array_v<T>, host_view<T>> make_host_view(T& data) {
    return aV2P::ViewPlainPtr<DevHost, T, Dim0D, Idx>(&data, host(), Scalar{});
  }

  template <typename T>
  host_view<T[]> make_host_view(T* data, Extent extent) {
    return aV2P::ViewPlainPtr<DevHost, T, Dim1D, Idx>(data, host(), Vec1D{extent});
  }

  template <typename T>
  host_view<T[]> make_host_view(std::span<T> span) {
    return aV2P::ViewPlainPtr<DevHost, T, Dim1D, Idx>(span.data(), host(), Vec1D{span.size()});
  }

  template <typename T>
  host_view<T[]> make_host_view(std::span<T> span, Extent extent) {
    if (extent > span.size()) {
      throw std::runtime_error("make_host_view: span size is smaller than the specified extent");
    }
    return aV2P::ViewPlainPtr<DevHost, T, Dim1D, Idx>(span.data(), host(), Vec1D{extent});
  }

  template <typename T>
  std::enable_if_t<cms::is_unbounded_array_v<T> and not std::is_array_v<std::remove_extent_t<T>>, host_view<T>>
  make_host_view(T& data, Extent extent) {
    return aV2P::ViewPlainPtr<DevHost, std::remove_extent_t<T>, Dim1D, Idx>(data, host(), Vec1D{extent});
  }

  template <typename T>
  std::enable_if_t<cms::is_bounded_array_v<T> and not std::is_array_v<std::remove_extent_t<T>>, host_view<T>>
  make_host_view(T& data) {
    return aV2P::ViewPlainPtr<DevHost, std::remove_extent_t<T>, Dim1D, Idx>(data, host(), Vec1D{std::extent_v<T>});
  }

  // scalar and 1-dimensional device buffers

  template <typename TDev, typename T, typename = std::enable_if_t<aV2P::concepts::Device<TDev>>>
  using device_buffer = typename detail::buffer_type<TDev, T>::type;

  template <typename TDev, typename T, typename = std::enable_if_t<aV2P::concepts::Device<TDev>>>
  using const_device_buffer = aV2P::ViewConst<device_buffer<TDev, T>>;

  // non-cached, scalar and 1-dimensional device buffers

  template <typename T, typename TDev>
  std::enable_if_t<aV2P::concepts::Device<TDev> and not std::is_array_v<T>, device_buffer<TDev, T>>
  make_device_buffer(TDev const& device) {
    return aV2P::allocBuf<T, Idx>(device, Scalar{});
  }

  template <typename T, typename TDev>
  std::enable_if_t<aV2P::concepts::Device<TDev> and cms::is_unbounded_array_v<T> and
                       not std::is_array_v<std::remove_extent_t<T>>,
                   device_buffer<TDev, T>>
  make_device_buffer(TDev const& device, Extent extent) {
    return aV2P::allocBuf<std::remove_extent_t<T>, Idx>(device, Vec1D{extent});
  }

  template <typename T, typename TDev>
  std::enable_if_t<aV2P::concepts::Device<TDev> and cms::is_bounded_array_v<T> and
                       not std::is_array_v<std::remove_extent_t<T>>,
                   device_buffer<TDev, T>>
  make_device_buffer(TDev const& device) {
    return aV2P::allocBuf<std::remove_extent_t<T>, Idx>(device, Vec1D{std::extent_v<T>});
  }

  // potentially-cached, scalar and 1-dimensional device buffers with queue-ordered semantic

  template <typename T, typename TQueue>
  std::enable_if_t<aV2P::concepts::Queue<TQueue> and not std::is_array_v<T>, device_buffer<aV2P::Dev<TQueue>, T>>
  make_device_buffer(TQueue const& queue) {
    if constexpr (device_allocator_policy<aV2P::Dev<TQueue>> == AllocatorPolicy::Caching) {
      return allocCachedBuf<T, Idx>(aV2P::getDev(queue), queue, Scalar{});
    }
    if constexpr (device_allocator_policy<aV2P::Dev<TQueue>> == AllocatorPolicy::Asynchronous) {
      return aV2P::allocAsyncBuf<T, Idx>(queue, Scalar{});
    }
    if constexpr (device_allocator_policy<aV2P::Dev<TQueue>> == AllocatorPolicy::Synchronous) {
      return aV2P::allocBuf<T, Idx>(aV2P::getDev(queue), Scalar{});
    }
  }

  template <typename T, typename TQueue>
  std::enable_if_t<aV2P::concepts::Queue<TQueue> and cms::is_unbounded_array_v<T> and
                       not std::is_array_v<std::remove_extent_t<T>>,
                   device_buffer<aV2P::Dev<TQueue>, T>>
  make_device_buffer(TQueue const& queue, Extent extent) {
    if constexpr (device_allocator_policy<aV2P::Dev<TQueue>> == AllocatorPolicy::Caching) {
      return allocCachedBuf<std::remove_extent_t<T>, Idx>(aV2P::getDev(queue), queue, Vec1D{extent});
    }
    if constexpr (device_allocator_policy<aV2P::Dev<TQueue>> == AllocatorPolicy::Asynchronous) {
      return aV2P::allocAsyncBuf<std::remove_extent_t<T>, Idx>(queue, Vec1D{extent});
    }
    if constexpr (device_allocator_policy<aV2P::Dev<TQueue>> == AllocatorPolicy::Synchronous) {
      return aV2P::allocBuf<std::remove_extent_t<T>, Idx>(aV2P::getDev(queue), Vec1D{extent});
    }
  }

  template <typename T, typename TQueue>
  std::enable_if_t<aV2P::concepts::Queue<TQueue> and cms::is_bounded_array_v<T> and
                       not std::is_array_v<std::remove_extent_t<T>>,
                   device_buffer<aV2P::Dev<TQueue>, T>>
  make_device_buffer(TQueue const& queue) {
    if constexpr (device_allocator_policy<aV2P::Dev<TQueue>> == AllocatorPolicy::Caching) {
      return allocCachedBuf<std::remove_extent_t<T>, Idx>(aV2P::getDev(queue), queue, Vec1D{std::extent_v<T>});
    }
    if constexpr (device_allocator_policy<aV2P::Dev<TQueue>> == AllocatorPolicy::Asynchronous) {
      return aV2P::allocAsyncBuf<std::remove_extent_t<T>, Idx>(queue, Vec1D{std::extent_v<T>});
    }
    if constexpr (device_allocator_policy<aV2P::Dev<TQueue>> == AllocatorPolicy::Synchronous) {
      return aV2P::allocBuf<std::remove_extent_t<T>, Idx>(aV2P::getDev(queue), Vec1D{std::extent_v<T>});
    }
  }

  // scalar and 1-dimensional device views

  template <typename TDev, typename T, typename = std::enable_if_t<aV2P::concepts::Device<TDev>>>
  using device_view = typename detail::view_type<TDev, T>::type;

  template <typename T, typename TDev>
  std::enable_if_t<aV2P::concepts::Device<TDev> and not std::is_array_v<T>, device_view<TDev, T>> make_device_view(
      TDev const& device, T& data) {
    return aV2P::ViewPlainPtr<TDev, T, Dim0D, Idx>(&data, device, Scalar{});
  }

  template <typename T, typename TDev>
  std::enable_if_t<aV2P::concepts::Device<TDev>, device_view<TDev, T[]>> make_device_view(TDev const& device,
                                                                                            T* data,
                                                                                            Extent extent) {
    return aV2P::ViewPlainPtr<TDev, T, Dim1D, Idx>(data, device, Vec1D{extent});
  }

  template <typename T, typename TDev>
  std::enable_if_t<aV2P::concepts::Device<TDev>, device_view<TDev, T[]>> make_device_view(TDev const& device,
                                                                                            std::span<T> span) {
    return aV2P::ViewPlainPtr<TDev, T, Dim1D, Idx>(span.data(), device, Vec1D{span.size()});
  }

  template <typename T, typename TDev>
  std::enable_if_t<aV2P::concepts::Device<TDev>, device_view<TDev, T[]>> make_device_view(TDev const& device,
                                                                                            std::span<T> span,
                                                                                            Extent extent) {
    if (extent > span.size()) {
      throw std::runtime_error("make_device_view: span size is smaller than the specified extent");
    }
    return aV2P::ViewPlainPtr<TDev, T, Dim1D, Idx>(span.data(), device, Vec1D{extent});
  }

  template <typename T, typename TDev>
  std::enable_if_t<aV2P::concepts::Device<TDev> and cms::is_unbounded_array_v<T> and
                       not std::is_array_v<std::remove_extent_t<T>>,
                   device_view<TDev, T>>
  make_device_view(TDev const& device, T& data, Extent extent) {
    return aV2P::ViewPlainPtr<TDev, std::remove_extent_t<T>, Dim1D, Idx>(data, device, Vec1D{extent});
  }

  template <typename T, typename TDev>
  std::enable_if_t<aV2P::concepts::Device<TDev> and cms::is_bounded_array_v<T> and
                       not std::is_array_v<std::remove_extent_t<T>>,
                   device_view<TDev, T>>
  make_device_view(TDev const& device, T& data) {
    return aV2P::ViewPlainPtr<TDev, std::remove_extent_t<T>, Dim1D, Idx>(data, device, Vec1D{std::extent_v<T>});
  }

  template <typename T, typename TQueue>
  std::enable_if_t<aV2P::concepts::Queue<TQueue> and not std::is_array_v<T>, device_view<aV2P::Dev<TQueue>, T>>
  make_device_view(TQueue const& queue, T& data) {
    return aV2P::ViewPlainPtr<aV2P::Dev<TQueue>, T, Dim0D, Idx>(&data, aV2P::getDev(queue), Scalar{});
  }

  template <typename T, typename TQueue>
  std::enable_if_t<aV2P::concepts::Queue<TQueue>, device_view<aV2P::Dev<TQueue>, T[]>> make_device_view(
      TQueue const& queue, T* data, Extent extent) {
    return aV2P::ViewPlainPtr<aV2P::Dev<TQueue>, T, Dim1D, Idx>(data, aV2P::getDev(queue), Vec1D{extent});
  }

  template <typename T, typename TQueue>
  std::enable_if_t<aV2P::concepts::Queue<TQueue>, device_view<aV2P::Dev<TQueue>, T[]>> make_device_view(
      TQueue const& queue, std::span<T> span) {
    return aV2P::ViewPlainPtr<aV2P::Dev<TQueue>, T, Dim1D, Idx>(
        span.data(), aV2P::getDev(queue), Vec1D{span.size()});
  }

  template <typename T, typename TQueue>
  std::enable_if_t<aV2P::concepts::Queue<TQueue>, device_view<aV2P::Dev<TQueue>, T[]>> make_device_view(
      TQueue const& queue, std::span<T> span, Extent extent) {
    if (extent > span.size()) {
      throw std::runtime_error("make_device_view: span size is smaller than the specified extent");
    }
    return aV2P::ViewPlainPtr<aV2P::Dev<TQueue>, T, Dim1D, Idx>(span.data(), aV2P::getDev(queue), Vec1D{extent});
  }

  template <typename T, typename TQueue>
  std::enable_if_t<aV2P::concepts::Queue<TQueue> and cms::is_unbounded_array_v<T> and
                       not std::is_array_v<std::remove_extent_t<T>>,
                   device_view<aV2P::Dev<TQueue>, T>>
  make_device_view(TQueue const& queue, T& data, Extent extent) {
    return aV2P::ViewPlainPtr<aV2P::Dev<TQueue>, std::remove_extent_t<T>, Dim1D, Idx>(
        data, aV2P::getDev(queue), Vec1D{extent});
  }

  template <typename T, typename TQueue>
  std::enable_if_t<aV2P::concepts::Queue<TQueue> and cms::is_bounded_array_v<T> and
                       not std::is_array_v<std::remove_extent_t<T>>,
                   device_view<aV2P::Dev<TQueue>, T>>
  make_device_view(TQueue const& queue, T& data) {
    return aV2P::ViewPlainPtr<aV2P::Dev<TQueue>, std::remove_extent_t<T>, Dim1D, Idx>(
        data, aV2P::getDev(queue), Vec1D{std::extent_v<T>});
  }

}  // namespace cms::alpakatools

#endif  // HeterogeneousCore_AlpakaInterface_interface_memory_h
