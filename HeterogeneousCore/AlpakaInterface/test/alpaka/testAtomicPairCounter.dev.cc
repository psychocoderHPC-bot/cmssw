#include <cassert>
#include <iostream>

#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>

#include <aV2P/alpaka.hpp>

#include "FWCore/Utilities/interface/stringize.h"
#include "HeterogeneousCore/AlpakaInterface/interface/config.h"
#include "HeterogeneousCore/AlpakaInterface/interface/memory.h"
#include "HeterogeneousCore/AlpakaInterface/interface/workdivision.h"
#include "HeterogeneousCore/AlpakaInterface/interface/AtomicPairCounter.h"

using namespace cms::alpakatools;
using namespace AV2P_ACCELERATOR_NAMESPACE;

static constexpr auto s_tag = "[" AV2P_TYPE_ALIAS_NAME(alpakaTestAtomicPair) "]";

struct update {
  AV2P_FN_ACC void operator()(
      const Acc1D &acc, AtomicPairCounter *dc, uint32_t *ind, uint32_t *cont, uint32_t n) const {
    for (auto i : uniform_elements(acc, n)) {
      auto m = i % 11;
      m = m % 6 + 1;  // max 6, no 0
      auto c = dc->inc_add(acc, m);
      AV2P_ASSERT_ACC(c.first < n);
      ind[c.first] = c.second;
      for (uint32_t j = c.second; j < c.second + m; ++j)
        cont[j] = i;
    }
  }
};

struct finalize {
  AV2P_FN_ACC void operator()(
      const Acc1D &acc, AtomicPairCounter const *dc, uint32_t *ind, uint32_t *cont, uint32_t n) const {
    AV2P_ASSERT_ACC(dc->get().first == n);
    ind[n] = dc->get().second;
  }
};

TEST_CASE("Standard checks of " AV2P_TYPE_ALIAS_NAME(alpakaTestAtomicPair), s_tag) {
  SECTION("AtomicPairCounter") {
    auto const &devices = cms::alpakatools::devices<Platform>();
    if (devices.empty()) {
      FAIL("No devices available for the " EDM_STRINGIZE(AV2P_ACCELERATOR_NAMESPACE) "backend, "
           "the test will be skipped.");
    }

    // run the test on each device
    for (auto const &device : devices) {
      std::cout << "Test AtomicPairCounter on " << aV2P::getName(device) << '\n';
      auto queue = Queue(device);

      auto c_d = make_device_buffer<AtomicPairCounter>(queue);
      aV2P::memset(queue, c_d, 0);

      std::cout << "- size " << sizeof(AtomicPairCounter) << std::endl;

      constexpr uint32_t N = 20000;
      constexpr uint32_t M = N * 6;
      auto n_d = make_device_buffer<uint32_t[]>(queue, N);
      auto m_d = make_device_buffer<uint32_t[]>(queue, M);

      constexpr uint32_t NUM_VALUES = 10000;

      // Update
      const auto blocksPerGrid = 2000u;
      const auto threadsPerBlockOrElementsPerThread = 512u;
      const auto workDiv = make_workdiv<Acc1D>(blocksPerGrid, threadsPerBlockOrElementsPerThread);
      aV2P::enqueue(
          queue, aV2P::createTaskKernel<Acc1D>(workDiv, update(), c_d.data(), n_d.data(), m_d.data(), NUM_VALUES));

      // Finalize
      const auto blocksPerGridFinalize = 1u;
      const auto threadsPerBlockOrElementsPerThreadFinalize = 1u;
      const auto workDivFinalize =
          make_workdiv<Acc1D>(blocksPerGridFinalize, threadsPerBlockOrElementsPerThreadFinalize);
      aV2P::enqueue(
          queue,
          aV2P::createTaskKernel<Acc1D>(workDivFinalize, finalize(), c_d.data(), n_d.data(), m_d.data(), NUM_VALUES));

      auto c_h = make_host_buffer<AtomicPairCounter>(queue);
      auto n_h = make_host_buffer<uint32_t[]>(queue, N);
      auto m_h = make_host_buffer<uint32_t[]>(queue, M);

      // copy the results from the device to the host
      aV2P::memcpy(queue, c_h, c_d);
      aV2P::memcpy(queue, n_h, n_d);
      aV2P::memcpy(queue, m_h, m_d);

      // wait for all the operations to complete
      aV2P::wait(queue);

      REQUIRE(c_h.data()->get().first == NUM_VALUES);
      REQUIRE(n_h[NUM_VALUES] == c_h.data()->get().second);
      REQUIRE(n_h[0] == 0);

      for (size_t i = 0; i < NUM_VALUES; ++i) {
        auto ib = n_h.data()[i];
        auto ie = n_h.data()[i + 1];
        auto k = m_h.data()[ib++];
        REQUIRE(k < NUM_VALUES);

        for (; ib < ie; ++ib)
          REQUIRE(m_h.data()[ib] == k);
      }
    }
  }
}
