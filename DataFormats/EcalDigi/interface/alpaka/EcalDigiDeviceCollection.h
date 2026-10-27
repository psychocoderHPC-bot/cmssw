#ifndef DataFormats_EcalDigi_interface_alpaka_EcalDigiDeviceCollection_h
#define DataFormats_EcalDigi_interface_alpaka_EcalDigiDeviceCollection_h

#include "DataFormats/Portable/interface/alpaka/PortableCollection.h"
#include "DataFormats/EcalDigi/interface/EcalDigiSoA.h"
#include "HeterogeneousCore/AlpakaInterface/interface/config.h"

namespace AV2P_ACCELERATOR_NAMESPACE {

  // EcalDigiSoA in device global memory
  using EcalDigiDeviceCollection = PortableCollection<EcalDigiSoA>;

}  // namespace AV2P_ACCELERATOR_NAMESPACE

#endif
