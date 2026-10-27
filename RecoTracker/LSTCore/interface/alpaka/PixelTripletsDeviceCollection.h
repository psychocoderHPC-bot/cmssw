#ifndef RecoTracker_LSTCore_interface_PixelTripletsDeviceCollection_h
#define RecoTracker_LSTCore_interface_PixelTripletsDeviceCollection_h

#include "DataFormats/Portable/interface/alpaka/PortableCollection.h"

#include "RecoTracker/LSTCore/interface/alpaka/Common.h"
#include "RecoTracker/LSTCore/interface/PixelTripletsSoA.h"

namespace AV2P_ACCELERATOR_NAMESPACE::lst {
  using PixelTripletsDeviceCollection = PortableCollection<PixelTripletsSoA>;
}  // namespace AV2P_ACCELERATOR_NAMESPACE::lst
#endif
