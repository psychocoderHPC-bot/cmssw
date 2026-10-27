#include <cstdint>

#include <aV2P/alpaka.hpp>

#include "HeterogeneousCore/AlpakaInterface/interface/config.h"
#include "HeterogeneousCore/AlpakaInterface/interface/workdivision.h"
#include "HeterogeneousTest/AlpakaKernel/interface/alpaka/DeviceAdditionKernel.h"

#include "AlpakaTestKernelAdditionAlgo.h"

namespace AV2P_ACCELERATOR_NAMESPACE::HeterogeneousTestAlpakaKernelPlugins {

  void wrapper_add_vectors_f(Queue& queue,
                             const float* __restrict__ in1,
                             const float* __restrict__ in2,
                             float* __restrict__ out,
                             uint32_t size) {
    aV2P::exec<Acc1D>(
        queue, cms::alpakatools::make_workdiv<Acc1D>(32, 32), test::KernelAddVectorsF{}, in1, in2, out, size);
  }

}  // namespace AV2P_ACCELERATOR_NAMESPACE::HeterogeneousTestAlpakaKernelPlugins
