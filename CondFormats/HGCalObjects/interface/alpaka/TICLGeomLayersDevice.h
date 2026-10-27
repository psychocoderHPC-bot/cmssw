#ifndef CondFormats_HGCalObjects_interface_alpaka_TICLGeomLayersDevice_h
#define CondFormats_HGCalObjects_interface_alpaka_TICLGeomLayersDevice_h

#include "CondFormats/HGCalObjects/interface/TICLGeomLayersHost.h"
#include "CondFormats/HGCalObjects/interface/TICLGeomLayersSoA.h"
#include "DataFormats/Portable/interface/alpaka/PortableCollection.h"
#include "HeterogeneousCore/AlpakaInterface/interface/config.h"

namespace AV2P_ACCELERATOR_NAMESPACE {

  using ::TICLGeomLayersHost;
  using TICLGeomLayersDevice = PortableCollection<TICLGeomLayersSoA>;

}  // namespace AV2P_ACCELERATOR_NAMESPACE

#endif  // CondFormats_HGCalObjects_interface_alpaka_TICLGeomLayersDevice_h
