#ifndef HeterogeneousCore_AlpakaInterface_interface_host_h
#define HeterogeneousCore_AlpakaInterface_interface_host_h

#include <aV2P/alpaka.hpp>

#include "HeterogeneousCore/AlpakaInterface/interface/devices.h"

namespace cms::alpakatools {

  // returns the alpaka host platform
  inline aV2P::PlatformCpu const& host_platform() { return platform<aV2P::PlatformCpu>(); }

  // returns the alpaka host device
  inline aV2P::DevCpu const& host() { return devices<aV2P::PlatformCpu>()[0]; }

}  // namespace cms::alpakatools

#endif  // HeterogeneousCore_AlpakaInterface_interface_host_h
