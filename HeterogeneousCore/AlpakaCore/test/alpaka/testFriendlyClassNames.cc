#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>

#include <aV2P/alpaka.hpp>

#include "FWCore/Utilities/interface/FriendlyName.h"
#include "FWCore/Utilities/interface/TypeDemangler.h"

namespace {

  template <typename T>
  std::string getFriendlyName() {
    return edm::friendlyname::friendlyName(edm::typeDemangle(typeid(T).name()));
  }

}  // namespace

TEST_CASE("Test edm::friendlyname::friendlyName for alpaka types ", "edm::friendlyname::friendlyName") {
  SECTION("CPU") {
    REQUIRE(getFriendlyName<aV2P::DevCpu>() == "alpakaDevCpu");
    REQUIRE(getFriendlyName<aV2P::QueueCpuBlocking>() == "alpakaQueueCpuBlocking");
    REQUIRE(getFriendlyName<aV2P::QueueCpuNonBlocking>() == "alpakaQueueCpuNonBlocking");
  }

#ifdef AV2P_ACC_GPU_CUDA_ENABLED
  SECTION("CUDA") {
    REQUIRE(getFriendlyName<aV2P::DevCudaRt>() == "alpakaDevCudaRt");
    REQUIRE(getFriendlyName<aV2P::QueueCudaRtBlocking>() == "alpakaQueueCudaRtBlocking");
    REQUIRE(getFriendlyName<aV2P::QueueCudaRtNonBlocking>() == "alpakaQueueCudaRtNonBlocking");
  }
#endif

#ifdef AV2P_ACC_GPU_HIP_ENABLED
  SECTION("ROCm") {
    REQUIRE(getFriendlyName<aV2P::DevHipRt>() == "alpakaDevHipRt");
    REQUIRE(getFriendlyName<aV2P::QueueHipRtBlocking>() == "alpakaQueueHipRtBlocking");
    REQUIRE(getFriendlyName<aV2P::QueueHipRtNonBlocking>() == "alpakaQueueHipRtNonBlocking");
  }
#endif
}
