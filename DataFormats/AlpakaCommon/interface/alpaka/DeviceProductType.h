#ifndef HeterogeneousCore_AlpakaCore_interface_alpaka_DeviceProductType_h
#define HeterogeneousCore_AlpakaCore_interface_alpaka_DeviceProductType_h

#include "DataFormats/Common/interface/DeviceProduct.h"
#include "HeterogeneousCore/AlpakaInterface/interface/config.h"

#include <type_traits>

namespace AV2P_ACCELERATOR_NAMESPACE::detail {
  // host synchronous backends can use TProduct directly
  // all device and asynchronous backends need to be wrapped
  inline constexpr bool useProductDirectly =
      std::is_same_v<Platform, aV2P::PlatformCpu> and std::is_same_v<Queue, aV2P::QueueCpuBlocking>;

  /**
   * Type alias for the actual product type put in the
   * edm::Event.
   */
  template <typename TProduct>
  using DeviceProductType = std::conditional_t<useProductDirectly, TProduct, edm::DeviceProduct<TProduct>>;

}  // namespace AV2P_ACCELERATOR_NAMESPACE::detail

#endif
