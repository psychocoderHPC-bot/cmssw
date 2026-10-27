#include <cassert>
#include <cstdlib>
#include <iostream>
#include <new>

#include <aV2P/alpaka.hpp>

#include "FWCore/Utilities/interface/stringize.h"
#include "HeterogeneousCore/AlpakaInterface/interface/SimpleVector.h"
#include "HeterogeneousCore/AlpakaInterface/interface/devices.h"
#include "HeterogeneousCore/AlpakaInterface/interface/config.h"
#include "HeterogeneousCore/AlpakaInterface/interface/memory.h"
#include "HeterogeneousCore/AlpakaInterface/interface/workdivision.h"

using namespace cms::alpakatools;
using namespace AV2P_ACCELERATOR_NAMESPACE;

struct vector_pushback {
  AV2P_FN_ACC void operator()(Acc1D const& acc, SimpleVector<int>* foo) const {
    for (auto index : uniform_elements(acc))
      foo->push_back(acc, index);
  }
};

struct vector_reset {
  AV2P_FN_ACC void operator()(Acc1D const& acc, SimpleVector<int>* foo) const { foo->reset(); }
};

struct vector_emplace_back {
  AV2P_FN_ACC void operator()(Acc1D const& acc, SimpleVector<int>* foo) const {
    for (auto index : uniform_elements(acc))
      foo->emplace_back(acc, index);
  }
};

int main() {
  // get the list of devices on the current platform
  auto const& devices = cms::alpakatools::devices<Platform>();
  if (devices.empty()) {
    std::cerr << "No devices available for the " EDM_STRINGIZE(AV2P_ACCELERATOR_NAMESPACE) " backend, "
      "the test will be skipped.\n";
    exit(EXIT_FAILURE);
  }

  // run the test on each device
  for (auto const& device : devices) {
    Queue queue(device);
    auto maxN = 10000;
    auto vec_h = make_host_buffer<cms::alpakatools::SimpleVector<int>>(queue);
    auto vec_d = make_device_buffer<cms::alpakatools::SimpleVector<int>>(queue);
    auto data_h = make_host_buffer<int[]>(queue, maxN);
    auto data_d = make_device_buffer<int[]>(queue, maxN);

    [[maybe_unused]] auto v = make_SimpleVector(maxN, data_d.data());

    // Prepare the vec object on the host
    auto tmp_vec_h = make_host_buffer<cms::alpakatools::SimpleVector<int>>(queue);
    make_SimpleVector(tmp_vec_h.data(), maxN, data_d.data());
    assert(tmp_vec_h->size() == 0);
    assert(tmp_vec_h->capacity() == static_cast<int>(maxN));

    // ... and copy the object to the device.
    aV2P::memcpy(queue, vec_d, tmp_vec_h);
    aV2P::wait(queue);

    int numBlocks = 5;
    int numThreadsPerBlock = 256;
    const auto workDiv = make_workdiv<Acc1D>(numBlocks, numThreadsPerBlock);
    aV2P::enqueue(queue, aV2P::createTaskKernel<Acc1D>(workDiv, vector_pushback(), vec_d.data()));
    aV2P::wait(queue);

    aV2P::memcpy(queue, vec_h, vec_d);
    aV2P::wait(queue);
    printf("vec_h->size()=%d, numBlocks * numThreadsPerBlock=%d, maxN=%d\n",
           vec_h->size(),
           numBlocks * numThreadsPerBlock,
           maxN);
    assert(vec_h->size() == (numBlocks * numThreadsPerBlock < maxN ? numBlocks * numThreadsPerBlock : maxN));
    aV2P::enqueue(queue, aV2P::createTaskKernel<Acc1D>(workDiv, vector_reset(), vec_d.data()));
    aV2P::wait(queue);

    aV2P::memcpy(queue, vec_h, vec_d);
    aV2P::wait(queue);

    assert(vec_h->size() == 0);

    aV2P::enqueue(queue, aV2P::createTaskKernel<Acc1D>(workDiv, vector_emplace_back(), vec_d.data()));
    aV2P::wait(queue);

    aV2P::memcpy(queue, vec_h, vec_d);
    aV2P::wait(queue);

    assert(vec_h->size() == (numBlocks * numThreadsPerBlock < maxN ? numBlocks * numThreadsPerBlock : maxN));

    aV2P::memcpy(queue, data_h, data_d);
  }
  std::cout << "TEST PASSED" << std::endl;
  return 0;
}
