#ifndef HeterogeneousCore_AlpakaInterface_interface_AllocatorPolicy_h
#define HeterogeneousCore_AlpakaInterface_interface_AllocatorPolicy_h

#include <aV2P/alpaka.hpp>

namespace cms::alpakatools {

  // Which memory allocator to use
  //   - Synchronous:   (device and host) aV2P::allocBuf, cudaMalloc/hipMalloc and cudaMallocHost/hipMallocHost
  //   - Asynchronous:  (device)          aV2P::allocAsyncBuf, cudaMallocAsync/hipMallocAsync (requires CUDA >= 11.2 or ROCm >= 5.6)
  //   - Caching:       (device and host) caching allocator
  enum class AllocatorPolicy { Synchronous = 0, Asynchronous = 1, Caching = 2 };

  template <typename TDev, typename = std::enable_if_t<aV2P::concepts::Device<TDev>>>
  constexpr inline AllocatorPolicy host_allocator_policy = AllocatorPolicy::Synchronous;

  template <typename TDev, typename = std::enable_if_t<aV2P::concepts::Device<TDev>>>
  constexpr inline AllocatorPolicy device_allocator_policy = AllocatorPolicy::Synchronous;

#if defined AV2P_ACC_CPU_B_SEQ_T_SEQ_ENABLED || defined AV2P_ACC_CPU_B_TBB_T_SEQ_ENABLED
  template <>
  constexpr inline AllocatorPolicy host_allocator_policy<aV2P::DevCpu> =
#if not defined AV2P_DISABLE_CACHING_ALLOCATOR and not defined AV2P_DISABLE_HOST_CACHING_ALLOCATOR
      AllocatorPolicy::Caching;
#else
      AllocatorPolicy::Synchronous;
#endif

  template <>
  constexpr inline AllocatorPolicy device_allocator_policy<aV2P::DevCpu> =
#if not defined AV2P_DISABLE_CACHING_ALLOCATOR and not defined AV2P_DISABLE_DEVICE_CACHING_ALLOCATOR
      AllocatorPolicy::Caching;
#else
      AllocatorPolicy::Synchronous;
#endif
#endif  // defined AV2P_ACC_CPU_B_SEQ_T_SEQ_ENABLED || defined AV2P_ACC_CPU_B_TBB_T_SEQ_ENABLED

#if defined AV2P_ACC_GPU_CUDA_ENABLED
  template <>
  constexpr inline AllocatorPolicy host_allocator_policy<aV2P::DevCudaRt> =
#if not defined AV2P_DISABLE_CACHING_ALLOCATOR and not defined AV2P_DISABLE_HOST_CACHING_ALLOCATOR
      AllocatorPolicy::Caching;
#else
      AllocatorPolicy::Synchronous;
#endif

  template <>
  constexpr inline AllocatorPolicy device_allocator_policy<aV2P::DevCudaRt> =
#if not defined AV2P_DISABLE_CACHING_ALLOCATOR and not defined AV2P_DISABLE_DEVICE_CACHING_ALLOCATOR
      AllocatorPolicy::Caching;
#elif not defined AV2P_DISABLE_ASYNC_ALLOCATOR and not defined AV2P_DISABLE_DEVICE_ASYNC_ALLOCATOR
      AllocatorPolicy::Asynchronous;
#else
                  AllocatorPolicy::Synchronous;
#endif
#endif  // AV2P_ACC_GPU_CUDA_ENABLED

#if defined AV2P_ACC_GPU_HIP_ENABLED
  template <>
  constexpr inline AllocatorPolicy host_allocator_policy<aV2P::DevHipRt> =
#if not defined AV2P_DISABLE_CACHING_ALLOCATOR and not defined AV2P_DISABLE_HOST_CACHING_ALLOCATOR
      AllocatorPolicy::Caching;
#else
      AllocatorPolicy::Synchronous;
#endif

  template <>
  constexpr inline AllocatorPolicy device_allocator_policy<aV2P::DevHipRt> =
#if not defined AV2P_DISABLE_CACHING_ALLOCATOR and not defined AV2P_DISABLE_DEVICE_CACHING_ALLOCATOR
      AllocatorPolicy::Caching;
#elif not defined AV2P_DISABLE_ASYNC_ALLOCATOR and not defined AV2P_DISABLE_DEVICE_ASYNC_ALLOCATOR
      AllocatorPolicy::Asynchronous;
#else
          AllocatorPolicy::Synchronous;
#endif
#endif  // AV2P_ACC_GPU_HIP_ENABLED

}  // namespace cms::alpakatools

#endif  // HeterogeneousCore_AlpakaInterface_interface_AllocatorPolicy_h
