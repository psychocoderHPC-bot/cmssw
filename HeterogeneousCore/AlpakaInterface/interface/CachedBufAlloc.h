#ifndef HeterogeneousCore_AlpakaInterface_interface_CachedBufAlloc_h
#define HeterogeneousCore_AlpakaInterface_interface_CachedBufAlloc_h

#include <aV2P/alpaka.hpp>

#include "HeterogeneousCore/AlpakaInterface/interface/getDeviceCachingAllocator.h"
#include "HeterogeneousCore/AlpakaInterface/interface/getHostCachingAllocator.h"

namespace cms::alpakatools {

  namespace traits {

    //! The caching memory allocator trait.
    template <typename TElem, typename TDim, typename TIdx, typename TDev, typename TQueue>
      requires aV2P::concepts::Device<TDev> and aV2P::concepts::Queue<TQueue>
    struct CachedBufAlloc {
      static_assert(aV2P::meta::DependentFalseType<TDev>::value, "This device does not support a caching allocator");
    };

    //! The caching memory allocator implementation for the CPU device, with a blocking queue
    template <typename TElem, typename TDim, typename TIdx>
    struct CachedBufAlloc<TElem, TDim, TIdx, aV2P::DevCpu, aV2P::QueueCpuBlocking> {
      template <typename TExtent>
      AV2P_FN_HOST static auto allocCachedBuf(aV2P::DevCpu const& dev,
                                                aV2P::QueueCpuBlocking queue,
                                                TExtent const& extent) -> aV2P::BufCpu<TElem, TDim, TIdx> {
        // non-cached, synchronous host-only memory
        return aV2P::allocBuf<TElem, TIdx>(dev, extent);
      }
    };

    //! The caching memory allocator implementation for the CPU device, with a non-blocking queue
    template <typename TElem, typename TDim, typename TIdx>
    struct CachedBufAlloc<TElem, TDim, TIdx, aV2P::DevCpu, aV2P::QueueCpuNonBlocking> {
      template <typename TExtent>
      AV2P_FN_HOST static auto allocCachedBuf(aV2P::DevCpu const& dev,
                                                aV2P::QueueCpuNonBlocking queue,
                                                TExtent const& extent) -> aV2P::BufCpu<TElem, TDim, TIdx> {
        // non-cached, queue-ordered asynchronous host-only memory
        return aV2P::allocAsyncBuf<TElem, TIdx>(queue, extent);
      }
    };

#ifdef AV2P_ACC_GPU_CUDA_ENABLED

    //! The caching memory allocator implementation for the pinned host memory, with a blocking queue
    template <typename TElem, typename TDim, typename TIdx>
    struct CachedBufAlloc<TElem, TDim, TIdx, aV2P::DevCpu, aV2P::QueueCudaRtBlocking> {
      template <typename TExtent>
      AV2P_FN_HOST static auto allocCachedBuf(aV2P::DevCpu const& dev,
                                                aV2P::QueueCudaRtBlocking queue,
                                                TExtent const& extent) -> aV2P::BufCpu<TElem, TDim, TIdx> {
        AV2P_DEBUG_MINIMAL_LOG_SCOPE;

        auto& allocator = getHostCachingAllocator<aV2P::QueueCudaRtBlocking>();

        // FIXME the BufCpu does not support a pitch ?
        size_t size = aV2P::getExtentProduct(extent);
        size_t sizeBytes = size * sizeof(TElem);
        void* memPtr = allocator.allocate(sizeBytes, queue);

        // use a custom deleter to return the buffer to the CachingAllocator
        auto deleter = [alloc = &allocator](TElem* ptr) { alloc->free(ptr); };

        return aV2P::BufCpu<TElem, TDim, TIdx>(dev, reinterpret_cast<TElem*>(memPtr), std::move(deleter), extent);
      }
    };

    //! The caching memory allocator implementation for the pinned host memory, with a non-blocking queue
    template <typename TElem, typename TDim, typename TIdx>
    struct CachedBufAlloc<TElem, TDim, TIdx, aV2P::DevCpu, aV2P::QueueCudaRtNonBlocking> {
      template <typename TExtent>
      AV2P_FN_HOST static auto allocCachedBuf(aV2P::DevCpu const& dev,
                                                aV2P::QueueCudaRtNonBlocking queue,
                                                TExtent const& extent) -> aV2P::BufCpu<TElem, TDim, TIdx> {
        AV2P_DEBUG_MINIMAL_LOG_SCOPE;

        auto& allocator = getHostCachingAllocator<aV2P::QueueCudaRtNonBlocking>();

        // FIXME the BufCpu does not support a pitch ?
        size_t size = aV2P::getExtentProduct(extent);
        size_t sizeBytes = size * sizeof(TElem);
        void* memPtr = allocator.allocate(sizeBytes, queue);

        // use a custom deleter to return the buffer to the CachingAllocator
        auto deleter = [alloc = &allocator](TElem* ptr) { alloc->free(ptr); };

        return aV2P::BufCpu<TElem, TDim, TIdx>(dev, reinterpret_cast<TElem*>(memPtr), std::move(deleter), extent);
      }
    };

    //! The caching memory allocator implementation for the CUDA device
    template <typename TElem, typename TDim, typename TIdx, typename TQueue>
      requires aV2P::concepts::Queue<TQueue>
    struct CachedBufAlloc<TElem, TDim, TIdx, aV2P::DevCudaRt, TQueue> {
      template <typename TExtent>
      AV2P_FN_HOST static auto allocCachedBuf(aV2P::DevCudaRt const& dev, TQueue queue, TExtent const& extent)
          -> aV2P::BufCudaRt<TElem, TDim, TIdx> {
        AV2P_DEBUG_MINIMAL_LOG_SCOPE;

        auto& allocator = getDeviceCachingAllocator<aV2P::DevCudaRt, TQueue>(dev);

        size_t width = aV2P::getWidth(extent);
        size_t widthBytes = width * static_cast<TIdx>(sizeof(TElem));
        // TODO implement pitch for TDim > 1
        size_t pitchBytes = widthBytes;
        size_t size = aV2P::getExtentProduct(extent);
        size_t sizeBytes = size * sizeof(TElem);
        void* memPtr = allocator.allocate(sizeBytes, queue);

        // use a custom deleter to return the buffer to the CachingAllocator
        auto deleter = [alloc = &allocator](TElem* ptr) { alloc->free(ptr); };

        return aV2P::BufCudaRt<TElem, TDim, TIdx>(
            dev, reinterpret_cast<TElem*>(memPtr), std::move(deleter), extent, pitchBytes);
      }
    };

#endif  // AV2P_ACC_GPU_CUDA_ENABLED

#ifdef AV2P_ACC_GPU_HIP_ENABLED

    //! The caching memory allocator implementation for the pinned host memory, with a blocking queue
    template <typename TElem, typename TDim, typename TIdx>
    struct CachedBufAlloc<TElem, TDim, TIdx, aV2P::DevCpu, aV2P::QueueHipRtBlocking> {
      template <typename TExtent>
      AV2P_FN_HOST static auto allocCachedBuf(aV2P::DevCpu const& dev,
                                                aV2P::QueueHipRtBlocking queue,
                                                TExtent const& extent) -> aV2P::BufCpu<TElem, TDim, TIdx> {
        AV2P_DEBUG_MINIMAL_LOG_SCOPE;

        auto& allocator = getHostCachingAllocator<aV2P::QueueHipRtBlocking>();

        // FIXME the BufCpu does not support a pitch ?
        size_t size = aV2P::getExtentProduct(extent);
        size_t sizeBytes = size * sizeof(TElem);
        void* memPtr = allocator.allocate(sizeBytes, queue);

        // use a custom deleter to return the buffer to the CachingAllocator
        auto deleter = [alloc = &allocator](TElem* ptr) { alloc->free(ptr); };

        return aV2P::BufCpu<TElem, TDim, TIdx>(dev, reinterpret_cast<TElem*>(memPtr), std::move(deleter), extent);
      }
    };

    //! The caching memory allocator implementation for the pinned host memory, with a non-blocking queue
    template <typename TElem, typename TDim, typename TIdx>
    struct CachedBufAlloc<TElem, TDim, TIdx, aV2P::DevCpu, aV2P::QueueHipRtNonBlocking> {
      template <typename TExtent>
      AV2P_FN_HOST static auto allocCachedBuf(aV2P::DevCpu const& dev,
                                                aV2P::QueueHipRtNonBlocking queue,
                                                TExtent const& extent) -> aV2P::BufCpu<TElem, TDim, TIdx> {
        AV2P_DEBUG_MINIMAL_LOG_SCOPE;

        auto& allocator = getHostCachingAllocator<aV2P::QueueHipRtNonBlocking>();

        // FIXME the BufCpu does not support a pitch ?
        size_t size = aV2P::getExtentProduct(extent);
        size_t sizeBytes = size * sizeof(TElem);
        void* memPtr = allocator.allocate(sizeBytes, queue);

        // use a custom deleter to return the buffer to the CachingAllocator
        auto deleter = [alloc = &allocator](TElem* ptr) { alloc->free(ptr); };

        return aV2P::BufCpu<TElem, TDim, TIdx>(dev, reinterpret_cast<TElem*>(memPtr), std::move(deleter), extent);
      }
    };

    //! The caching memory allocator implementation for the ROCm/HIP device
    template <typename TElem, typename TDim, typename TIdx, typename TQueue>
      requires aV2P::concepts::Queue<TQueue>
    struct CachedBufAlloc<TElem, TDim, TIdx, aV2P::DevHipRt, TQueue> {
      template <typename TExtent>
      AV2P_FN_HOST static auto allocCachedBuf(aV2P::DevHipRt const& dev, TQueue queue, TExtent const& extent)
          -> aV2P::BufHipRt<TElem, TDim, TIdx> {
        AV2P_DEBUG_MINIMAL_LOG_SCOPE;

        auto& allocator = getDeviceCachingAllocator<aV2P::DevHipRt, TQueue>(dev);

        size_t width = aV2P::getWidth(extent);
        size_t widthBytes = width * static_cast<TIdx>(sizeof(TElem));
        // TODO implement pitch for TDim > 1
        size_t pitchBytes = widthBytes;
        size_t size = aV2P::getExtentProduct(extent);
        size_t sizeBytes = size * sizeof(TElem);
        void* memPtr = allocator.allocate(sizeBytes, queue);

        // use a custom deleter to return the buffer to the CachingAllocator
        auto deleter = [alloc = &allocator](TElem* ptr) { alloc->free(ptr); };

        return aV2P::BufHipRt<TElem, TDim, TIdx>(
            dev, reinterpret_cast<TElem*>(memPtr), std::move(deleter), extent, pitchBytes);
      }
    };

#endif  // AV2P_ACC_GPU_HIP_ENABLED

  }  // namespace traits

  template <typename TElem, typename TIdx, typename TExtent, typename TQueue, typename TDev>
    requires aV2P::concepts::Device<TDev> and aV2P::concepts::Queue<TQueue>
  AV2P_FN_HOST auto allocCachedBuf(TDev const& dev, TQueue queue, TExtent const& extent = TExtent()) {
    return traits::CachedBufAlloc<TElem, aV2P::Dim<TExtent>, TIdx, TDev, TQueue>::allocCachedBuf(dev, queue, extent);
  }

}  // namespace cms::alpakatools

#endif  // HeterogeneousCore_AlpakaInterface_interface_CachedBufAlloc_h
