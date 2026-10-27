#ifndef HeterogeneousCore_AlpakaCore_interface_alpaka_typelookup_h
#define HeterogeneousCore_AlpakaCore_interface_alpaka_typelookup_h

#include "FWCore/Utilities/interface/typelookup.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/ESDeviceProduct.h"
#include "HeterogeneousCore/AlpakaInterface/interface/config.h"

// TODO: this place is suboptimal given that the framework's
// typelookup.h is in FWCore/Utilities (i.e. independent of the
// framework itself). The ESDeviceProduct should be in the same
// package.

// force expanding AV2P_ACCELERATOR_NAMESPACE before stringification inside TYPELOOKUP_DATA_REG
#define TYPELOOKUP_AV2P_DATA_REG2(name) TYPELOOKUP_DATA_REG(name)

// Model 1 for ESProducts with Alpaka: ESProduct class for host
// is defined in global namespace, and for devices in
// AV2P_ACCELERATOR_NAMESPACE
#ifdef AV2P_ACC_CPU_B_SEQ_T_SEQ_ENABLED
// all host backends
#define TYPELOOKUP_AV2P_DATA_REG(name)
#else
#define TYPELOOKUP_AV2P_DATA_REG(name) \
  TYPELOOKUP_AV2P_DATA_REG2(AV2P_ACCELERATOR_NAMESPACE::ESDeviceProduct<AV2P_ACCELERATOR_NAMESPACE::name>)
#endif

// Model 2 for ESProducts with Alpaka: ESProduct class templated over device type
// TODO: should really instantiated and registered per "memory space" rather than per backend
#ifdef AV2P_ACC_CPU_B_SEQ_T_SEQ_ENABLED
// all host backends
#define TYPELOOKUP_AV2P_TEMPLATED_DATA_REG(name)
#else
#define TYPELOOKUP_AV2P_TEMPLATED_DATA_REG(name) \
  TYPELOOKUP_AV2P_DATA_REG2(AV2P_ACCELERATOR_NAMESPACE::ESDeviceProduct<name<AV2P_ACCELERATOR_NAMESPACE::Device>>)
#endif

#endif
