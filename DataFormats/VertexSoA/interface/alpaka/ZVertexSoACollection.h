#ifndef DataFormats_VertexSoA_interface_ZVertexSoACollection_h
#define DataFormats_VertexSoA_interface_ZVertexSoACollection_h

#include <cstdint>

#include <aV2P/alpaka.hpp>

#include "DataFormats/Portable/interface/alpaka/PortableCollection.h"
#include "DataFormats/VertexSoA/interface/ZVertexDevice.h"
#include "DataFormats/VertexSoA/interface/ZVertexHost.h"
#include "DataFormats/VertexSoA/interface/ZVertexSoA.h"
#include "HeterogeneousCore/AlpakaInterface/interface/CopyToHost.h"
#include "HeterogeneousCore/AlpakaInterface/interface/config.h"

namespace AV2P_ACCELERATOR_NAMESPACE::reco {

  using ::reco::ZVertexDevice;
  using ::reco::ZVertexHost;
  using ZVertexSoACollection =
      std::conditional_t<std::is_same_v<Device, aV2P::DevCpu>, ZVertexHost, ZVertexDevice<Device>>;

}  // namespace AV2P_ACCELERATOR_NAMESPACE::reco

ASSERT_DEVICE_MATCHES_HOST_COLLECTION(reco::ZVertexSoACollection, reco::ZVertexHost);

#endif  // DataFormats_VertexSoA_interface_ZVertexSoACollection_h
