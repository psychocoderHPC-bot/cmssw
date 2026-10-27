#include <cstdlib>

#include <aV2P/alpaka.hpp>

#include "DataFormats/SiPixelClusterSoA/interface/SiPixelClustersDevice.h"
#include "DataFormats/SiPixelClusterSoA/interface/SiPixelClustersHost.h"
#include "DataFormats/SiPixelClusterSoA/interface/SiPixelClustersSoA.h"
#include "DataFormats/SiPixelClusterSoA/interface/alpaka/SiPixelClustersSoACollection.h"
#include "FWCore/Utilities/interface/stringize.h"
#include "HeterogeneousCore/AlpakaInterface/interface/config.h"
#include "HeterogeneousCore/AlpakaInterface/interface/devices.h"
#include "HeterogeneousCore/AlpakaInterface/interface/memory.h"
#include "HeterogeneousCore/AlpakaInterface/interface/workdivision.h"

#include "Clusters_test.h"

using namespace AV2P_ACCELERATOR_NAMESPACE;

int main() {
  // Get the list of devices on the current platform
  auto const& devices = cms::alpakatools::devices<Platform>();
  if (devices.empty()) {
    std::cerr << "No devices available for the " EDM_STRINGIZE(AV2P_ACCELERATOR_NAMESPACE) " backend, "
      "the test will be skipped.\n";
    exit(EXIT_FAILURE);
  }

  // Run the test on each device
  for (const auto& device : devices) {
    Queue queue(device);

    // Inner scope to deallocate memory before destroying the stream
    {
      // Instantiate tracks on device. PortableDeviceCollection allocates
      // SoA on device automatically.
      SiPixelClustersSoACollection clusters_d(queue, 100);
      testClusterSoA::runKernels(clusters_d.view(), queue);

      // Instantate tracks on host. This is where the data will be
      // copied to from device.
      SiPixelClustersHost clusters_h(queue, clusters_d.view().metadata().size());

      std::cout << clusters_h.view().metadata().size() << std::endl;
      aV2P::memcpy(queue, clusters_h.buffer(), clusters_d.const_buffer());
      aV2P::wait(queue);
    }
  }

  return EXIT_SUCCESS;
}
