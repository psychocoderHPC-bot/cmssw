#ifndef PhysicsTools_PyTorchAlpaka_interface_alpaka_QueueGuard_h
#define PhysicsTools_PyTorchAlpaka_interface_alpaka_QueueGuard_h

#include <aV2P/alpaka.hpp>
#ifdef AV2P_ACC_GPU_CUDA_ENABLED
#include <c10/cuda/CUDAStream.h>
#elif defined(AV2P_ACC_GPU_HIP_ENABLED)
#include <c10/hip/HIPStream.h>
#endif

#include "HeterogeneousCore/AlpakaInterface/interface/config.h"
#include "PhysicsTools/PyTorchAlpaka/interface/GetDevice.h"

namespace AV2P_ACCELERATOR_NAMESPACE::torch {

#if defined(AV2P_ACC_GPU_CUDA_ENABLED) || defined(AV2P_ACC_GPU_HIP_ENABLED)
  class QueueGuard {
  public:
    explicit QueueGuard(const Queue &queue) noexcept : cached_stream_{c10::cuda::getCurrentCUDAStream()} {
      auto stream =
          c10::cuda::getStreamFromExternal(queue.getNativeHandle(), cms::torch::alpakatools::getDevice(queue).index());
      c10::cuda::setCurrentCUDAStream(stream);
    }

    ~QueueGuard() noexcept { c10::cuda::setCurrentCUDAStream(cached_stream_); }

  private:
    c10::cuda::CUDAStream cached_stream_;
  };
#else
  class QueueGuard {
  public:
    explicit QueueGuard(const Queue &) noexcept {}
    ~QueueGuard() = default;
  };

#endif

}  // namespace AV2P_ACCELERATOR_NAMESPACE::torch

#endif  // PhysicsTools_PyTorchAlpaka_interface_alpaka_QueueGuard_h
