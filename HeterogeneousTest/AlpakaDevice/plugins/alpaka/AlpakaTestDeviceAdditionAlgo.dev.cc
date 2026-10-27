#include <cstdint>

#include <aV2P/alpaka.hpp>

#include "HeterogeneousCore/AlpakaInterface/interface/config.h"
#include "HeterogeneousCore/AlpakaInterface/interface/workdivision.h"
#include "HeterogeneousTest/AlpakaDevice/interface/alpaka/DeviceAddition.h"

#include "AlpakaTestDeviceAdditionAlgo.h"

namespace AV2P_ACCELERATOR_NAMESPACE::HeterogeneousTestAlpakaDevicePlugins {

  struct KernelAddVectorsF {
    AV2P_FN_ACC void operator()(Acc1D const& acc,
                                  const float* __restrict__ in1,
                                  const float* __restrict__ in2,
                                  float* __restrict__ out,
                                  uint32_t size) const {
      test::add_vectors_f(acc, in1, in2, out, size);
    }
  };

  void wrapper_add_vectors_f(Queue& queue,
                             const float* __restrict__ in1,
                             const float* __restrict__ in2,
                             float* __restrict__ out,
                             uint32_t size) {
    aV2P::exec<Acc1D>(queue, cms::alpakatools::make_workdiv<Acc1D>(32, 32), KernelAddVectorsF{}, in1, in2, out, size);
  }

}  // namespace AV2P_ACCELERATOR_NAMESPACE::HeterogeneousTestAlpakaDevicePlugins
