#ifndef HeterogeneousCore_AlpakaInterface_interface_config_h
#define HeterogeneousCore_AlpakaInterface_interface_config_h

#include <type_traits>

#include <aV2P/alpaka.hpp>

#include "HeterogeneousCore/AlpakaInterface/interface/Backend.h"
#include "FWCore/Utilities/interface/stringize.h"

namespace alpaka_common {

  // common types and dimensions
  using Idx = uint32_t;
  using Extent = uint32_t;
  using Offsets = Extent;

  using Dim0D = aV2P::DimInt<0u>;
  using Dim1D = aV2P::DimInt<1u>;
  using Dim2D = aV2P::DimInt<2u>;
  using Dim3D = aV2P::DimInt<3u>;

  template <typename TDim>
  using Vec = aV2P::Vec<TDim, Idx>;
  using Vec1D = Vec<Dim1D>;
  using Vec2D = Vec<Dim2D>;
  using Vec3D = Vec<Dim3D>;
  using Scalar = Vec<Dim0D>;

  template <typename TDim>
  using WorkDiv = aV2P::WorkDivMembers<TDim, Idx>;
  using WorkDiv1D = WorkDiv<Dim1D>;
  using WorkDiv2D = WorkDiv<Dim2D>;
  using WorkDiv3D = WorkDiv<Dim3D>;

  // host types
  using DevHost = aV2P::DevCpu;
  using PlatformHost = aV2P::Platform<DevHost>;

}  // namespace alpaka_common

#ifdef AV2P_ACC_GPU_CUDA_ENABLED
namespace alpaka_cuda_async {
  using namespace alpaka_common;

  using Platform = aV2P::PlatformCudaRt;
  using Device = aV2P::DevCudaRt;
  using Queue = aV2P::QueueCudaRtNonBlocking;
  using Event = aV2P::EventCudaRt;

  template <typename TDim>
  using Acc = aV2P::AccGpuCudaRt<TDim, Idx>;
  using Acc1D = Acc<Dim1D>;
  using Acc2D = Acc<Dim2D>;
  using Acc3D = Acc<Dim3D>;

}  // namespace alpaka_cuda_async

#ifdef AV2P_ACCELERATOR_NAMESPACE
#define AV2P_DUPLICATE_NAMESPACE
#else
#define AV2P_ACCELERATOR_NAMESPACE alpaka_cuda_async
#define AV2P_TYPE_SUFFIX CudaAsync
#endif

#endif  // AV2P_ACC_GPU_CUDA_ENABLED

#ifdef AV2P_ACC_GPU_HIP_ENABLED
namespace alpaka_rocm_async {
  using namespace alpaka_common;

  using Platform = aV2P::PlatformHipRt;
  using Device = aV2P::DevHipRt;
  using Queue = aV2P::QueueHipRtNonBlocking;
  using Event = aV2P::EventHipRt;

  template <typename TDim>
  using Acc = aV2P::AccGpuHipRt<TDim, Idx>;
  using Acc1D = Acc<Dim1D>;
  using Acc2D = Acc<Dim2D>;
  using Acc3D = Acc<Dim3D>;

}  // namespace alpaka_rocm_async

#ifdef AV2P_ACCELERATOR_NAMESPACE
#define AV2P_DUPLICATE_NAMESPACE
#else
#define AV2P_ACCELERATOR_NAMESPACE alpaka_rocm_async
#define AV2P_TYPE_SUFFIX ROCmAsync
#endif

#endif  // AV2P_ACC_GPU_HIP_ENABLED

#ifdef AV2P_ACC_CPU_B_SEQ_T_SEQ_ENABLED
namespace alpaka_serial_sync {
  using namespace alpaka_common;

  using Platform = aV2P::PlatformCpu;
  using Device = aV2P::DevCpu;
  using Queue = aV2P::QueueCpuBlocking;
  using Event = aV2P::EventCpu;

  template <typename TDim>
  using Acc = aV2P::AccCpuSerial<TDim, Idx>;
  using Acc1D = Acc<Dim1D>;
  using Acc2D = Acc<Dim2D>;
  using Acc3D = Acc<Dim3D>;

}  // namespace alpaka_serial_sync

#ifdef AV2P_ACCELERATOR_NAMESPACE
#define AV2P_DUPLICATE_NAMESPACE
#else
#define AV2P_ACCELERATOR_NAMESPACE alpaka_serial_sync
#define AV2P_TYPE_SUFFIX SerialSync
#endif

#endif  // AV2P_ACC_CPU_B_SEQ_T_SEQ_ENABLED

#ifdef AV2P_ACC_CPU_B_TBB_T_SEQ_ENABLED
namespace alpaka_tbb_async {
  using namespace alpaka_common;

  using Platform = aV2P::PlatformCpu;
  using Device = aV2P::DevCpu;
  using Queue = aV2P::QueueCpuNonBlocking;
  using Event = aV2P::EventCpu;

  template <typename TDim>
  using Acc = aV2P::AccCpuTbbBlocks<TDim, Idx>;
  using Acc1D = Acc<Dim1D>;
  using Acc2D = Acc<Dim2D>;
  using Acc3D = Acc<Dim3D>;

}  // namespace alpaka_tbb_async

#ifdef AV2P_ACCELERATOR_NAMESPACE
#define AV2P_DUPLICATE_NAMESPACE
#else
#define AV2P_ACCELERATOR_NAMESPACE alpaka_tbb_async
#define AV2P_TYPE_SUFFIX TbbAsync
#endif

#endif  // AV2P_ACC_CPU_B_TBB_T_SEQ_ENABLED

#if defined AV2P_DUPLICATE_NAMESPACE
#error Only one alpaka backend symbol can be defined at the same time: AV2P_ACC_GPU_CUDA_ENABLED, AV2P_ACC_GPU_HIP_ENABLED, AV2P_ACC_CPU_B_SEQ_T_SEQ_ENABLED, AV2P_ACC_CPU_B_TBB_T_SEQ_ENABLED.
#endif

#if defined AV2P_ACCELERATOR_NAMESPACE

// create a new backend-specific identifier based on the original type name and a backend-specific suffix
#define AV2P_TYPE_ALIAS__(TYPE, SUFFIX) TYPE##SUFFIX
#define AV2P_TYPE_ALIAS_(TYPE, SUFFIX) AV2P_TYPE_ALIAS__(TYPE, SUFFIX)
#define AV2P_TYPE_ALIAS(TYPE) AV2P_TYPE_ALIAS_(TYPE, AV2P_TYPE_SUFFIX)

// declare the backend-specific identifier as an alias for the namespace-based type name
#define DECLARE_AV2P_TYPE_ALIAS(TYPE) using AV2P_TYPE_ALIAS(TYPE) = AV2P_ACCELERATOR_NAMESPACE::TYPE

// define a null-terminated string containing the backend-specific identifier
#define AV2P_TYPE_ALIAS_NAME(TYPE) EDM_STRINGIZE(AV2P_TYPE_ALIAS(TYPE))

// Ensure the enumeration names are consistent with type suffix
namespace AV2P_ACCELERATOR_NAMESPACE {
  inline constexpr const cms::alpakatools::Backend kBackend = cms::alpakatools::Backend::AV2P_TYPE_SUFFIX;
}

#endif  // AV2P_ACCELERATOR_NAMESPACE

#endif  // HeterogeneousCore_AlpakaInterface_interface_config_h
