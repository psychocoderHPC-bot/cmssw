#include <aV2P/alpaka.hpp>

#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>

#include "FWCore/Utilities/interface/stringize.h"
#include "HeterogeneousCore/AlpakaInterface/interface/config.h"
#include "HeterogeneousCore/AlpakaInterface/interface/memory.h"
#include "HeterogeneousCore/AlpakaInterface/interface/workdivision.h"

// each test binary is built for a single Alpaka backend
using namespace AV2P_ACCELERATOR_NAMESPACE;

namespace {
  constexpr size_t SIZE = 32;

// Disable this test for HIP/ROCm until ROCm or alpaka provide a non-fatal way to assert in device code.
#ifndef AV2P_ACC_GPU_HIP_ENABLED
  void testDeviceSideError(Device const& device) {
    auto queue = Queue(device);
    auto buf_h = cms::alpakatools::make_host_buffer<int[]>(queue, SIZE);
    auto buf_d = cms::alpakatools::make_device_buffer<int[]>(queue, SIZE);
    aV2P::memset(queue, buf_h, 0);
    aV2P::memcpy(queue, buf_d, buf_h);
    // On the host device I don't know how to fabricate a device-side
    // error for which the Alpaka API calls would then throw an
    // exception. Therefore I just throw the std::runtime_error to
    // keep the caller side the same for all backends. At least the
    // test ensures the buffer destructors won't throw exceptions
    // during the stack unwinding of the thrown runtime_error.
    if constexpr (std::is_same_v<Device, aV2P::DevCpu>) {
      throw std::runtime_error("assert");
    } else {
      auto div = cms::alpakatools::make_workdiv<Acc1D>(1, 1);
      aV2P::exec<Acc1D>(
          queue,
          div,
          [] AV2P_FN_ACC(Acc1D const& acc, int* data, size_t size) {
            for (auto index : cms::alpakatools::uniform_elements(acc, size)) {
              AV2P_ASSERT_ACC(data[index] != 0);
            }
          },
          buf_d.data(),
          SIZE);
      aV2P::wait(queue);
    }
  }
#endif  // AV2P_ACC_GPU_HIP_ENABLED
}  // namespace

TEST_CASE("Test alpaka buffers for the " EDM_STRINGIZE(AV2P_ACCELERATOR_NAMESPACE) " backend",
          "[" EDM_STRINGIZE(AV2P_ACCELERATOR_NAMESPACE) "]") {
  // get the list of devices on the current platform
  auto const& devices = cms::alpakatools::devices<Platform>();
  if (devices.empty()) {
    FAIL("No devices available for the " EDM_STRINGIZE(AV2P_ACCELERATOR_NAMESPACE) " backend, "
         "the test will be skipped.");
  }

  SECTION("Single device buffer") {
    for (auto const& device : devices) {
      auto queue = Queue(device);
      auto buf = cms::alpakatools::make_device_buffer<int[]>(queue, SIZE);
      aV2P::memset(queue, buf, 0);
      aV2P::wait(queue);
    }
  }

  SECTION("Single host buffer") {
    for (auto const& device : devices) {
      auto queue = Queue(device);
      auto buf = cms::alpakatools::make_host_buffer<int[]>(queue, SIZE);
      buf[0] = 0;
      aV2P::wait(queue);
    }
  }

  SECTION("Host and device buffers") {
    for (auto const& device : devices) {
      auto queue = Queue(device);
      auto buf_h = cms::alpakatools::make_host_buffer<int[]>(queue, SIZE);
      auto buf_d = cms::alpakatools::make_device_buffer<int[]>(queue, SIZE);
      aV2P::memset(queue, buf_h, 0);
      aV2P::memcpy(queue, buf_d, buf_h);
      aV2P::wait(queue);
    }
  }

// Disable this test for HIP/ROCm until ROCm or alpaka provide a non-fatal way to assert in device code.
#ifndef AV2P_ACC_GPU_HIP_ENABLED
  SECTION("Buffer destruction after a device-side error") {
    for (auto const& device : devices) {
      REQUIRE_THROWS_AS(testDeviceSideError(device), std::runtime_error);
    }
  }
#endif  // AV2P_ACC_GPU_HIP_ENABLED
}
