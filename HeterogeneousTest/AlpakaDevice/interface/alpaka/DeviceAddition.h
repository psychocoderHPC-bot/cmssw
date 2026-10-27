#ifndef HeterogeneousTest_AlpakaDevice_interface_alpaka_DeviceAddition_h
#define HeterogeneousTest_AlpakaDevice_interface_alpaka_DeviceAddition_h

#include <cstdint>

#include <aV2P/alpaka.hpp>

#include "HeterogeneousCore/AlpakaInterface/interface/config.h"
#include "HeterogeneousCore/AlpakaInterface/interface/workdivision.h"

namespace AV2P_ACCELERATOR_NAMESPACE::test {

  inline AV2P_FN_ACC void add_vectors_f(Acc1D const& acc,
                                          float const* __restrict__ in1,
                                          float const* __restrict__ in2,
                                          float* __restrict__ out,
                                          uint32_t size) {
    for (auto i : cms::alpakatools::uniform_elements(acc, size)) {
      out[i] = in1[i] + in2[i];
    }
  }

  inline AV2P_FN_ACC void add_vectors_d(Acc1D const& acc,
                                          double const* __restrict__ in1,
                                          double const* __restrict__ in2,
                                          double* __restrict__ out,
                                          uint32_t size) {
    for (auto i : cms::alpakatools::uniform_elements(acc, size)) {
      out[i] = in1[i] + in2[i];
    }
  }

}  // namespace AV2P_ACCELERATOR_NAMESPACE::test

#endif  // HeterogeneousTest_AlpakaDevice_interface_alpaka_DeviceAddition_h
