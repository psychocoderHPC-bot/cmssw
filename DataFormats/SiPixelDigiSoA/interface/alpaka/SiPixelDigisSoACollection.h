#ifndef DataFormats_SiPixelDigiSoA_interface_alpaka_SiPixelDigisSoACollection_h
#define DataFormats_SiPixelDigiSoA_interface_alpaka_SiPixelDigisSoACollection_h

#include <cstdint>

#include <aV2P/alpaka.hpp>

#include "DataFormats/Portable/interface/alpaka/PortableCollection.h"
#include "DataFormats/SiPixelDigiSoA/interface/SiPixelDigisDevice.h"
#include "DataFormats/SiPixelDigiSoA/interface/SiPixelDigisHost.h"
#include "HeterogeneousCore/AlpakaInterface/interface/CopyToHost.h"
#include "HeterogeneousCore/AlpakaInterface/interface/config.h"

namespace AV2P_ACCELERATOR_NAMESPACE {

  using SiPixelDigisSoACollection =
      std::conditional_t<std::is_same_v<Device, aV2P::DevCpu>, SiPixelDigisHost, SiPixelDigisDevice<Device>>;

}  // namespace AV2P_ACCELERATOR_NAMESPACE

namespace cms::alpakatools {
  template <typename TDevice>
  struct CopyToHost<SiPixelDigisDevice<TDevice>> {
    template <typename TQueue>
    static auto copyAsync(TQueue &queue, SiPixelDigisDevice<TDevice> const &srcData) {
      SiPixelDigisHost dstData(queue, srcData.view().metadata().size() - 1);
      aV2P::memcpy(queue, dstData.buffer(), srcData.buffer());
      dstData.setNModules(srcData.nModules());
      return dstData;
    }
  };
}  // namespace cms::alpakatools

ASSERT_DEVICE_MATCHES_HOST_COLLECTION(SiPixelDigisSoACollection, SiPixelDigisHost);

#endif  // DataFormats_SiPixelDigiSoA_interface_alpaka_SiPixelDigisSoACollection_h
