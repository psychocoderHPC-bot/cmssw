#ifndef RecoTracker_LSTCore_interface_PixelQuintupletsDeviceCollection_h
#define RecoTracker_LSTCore_interface_PixelQuintupletsDeviceCollection_h

#include "DataFormats/Portable/interface/alpaka/PortableCollection.h"

#include "RecoTracker/LSTCore/interface/alpaka/Common.h"
#include "RecoTracker/LSTCore/interface/PixelQuintupletsSoA.h"

namespace AV2P_ACCELERATOR_NAMESPACE::lst {
  using PixelQuintupletsDeviceCollection = PortableCollection<PixelQuintupletsSoA>;
}  // namespace AV2P_ACCELERATOR_NAMESPACE::lst
#endif
