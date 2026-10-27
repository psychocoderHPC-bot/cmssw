#ifndef RecoTracker_LSTCore_interface_alpaka_ModulesDeviceCollection_h
#define RecoTracker_LSTCore_interface_alpaka_ModulesDeviceCollection_h

#include "DataFormats/Portable/interface/alpaka/PortableCollection.h"

#include "RecoTracker/LSTCore/interface/alpaka/Common.h"
#include "RecoTracker/LSTCore/interface/ModulesSoA.h"

namespace AV2P_ACCELERATOR_NAMESPACE::lst {
  using ModulesDeviceCollection = PortableCollection<ModulesSoABlocks>;
}  // namespace AV2P_ACCELERATOR_NAMESPACE::lst

#endif
