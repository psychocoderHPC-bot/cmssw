#ifndef DataFormats_Portable_interface_alpaka_PortableCollection_h
#define DataFormats_Portable_interface_alpaka_PortableCollection_h

#include <aV2P/alpaka.hpp>

#include "DataFormats/Portable/interface/PortableCollection.h"
#include "HeterogeneousCore/AlpakaInterface/interface/config.h"

// This header is not used by PortableCollection, but is included here to automatically
// provide its content to users of AV2P_ACCELERATOR_NAMESPACE::PortableCollection.
#include "HeterogeneousCore/AlpakaInterface/interface/AssertDeviceMatchesHostCollection.h"

namespace AV2P_ACCELERATOR_NAMESPACE {

  // generic SoA-based product in the device (that may be host) memory
  template <typename T>
  using PortableCollection = ::PortableCollection<Device, T>;

}  // namespace AV2P_ACCELERATOR_NAMESPACE

#endif  // DataFormats_Portable_interface_alpaka_PortableCollection_h
