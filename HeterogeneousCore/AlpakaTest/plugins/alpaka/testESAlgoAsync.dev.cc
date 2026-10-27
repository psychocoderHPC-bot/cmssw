#include "testESAlgoAsync.h"

namespace AV2P_ACCELERATOR_NAMESPACE {
  AlpakaESTestDataDDevice testESAlgoAsync(Queue& queue,
                                          AlpakaESTestDataADevice const& dataA,
                                          cms::alpakatest::AlpakaESTestDataB<Device> const& dataB) {
    auto const size = std::min(dataA->metadata().size(), static_cast<int>(dataB.size()));
    AlpakaESTestDataDDevice ret(queue, size);

    auto const& deviceProperties = aV2P::getAccDevProps<Acc1D>(aV2P::getDev(queue));
    uint32_t maxThreadsPerBlock = deviceProperties.m_blockThreadExtentMax[0];

    uint32_t threadsPerBlock = maxThreadsPerBlock;
    uint32_t blocksPerGrid = (size + threadsPerBlock - 1) / threadsPerBlock;
    uint32_t elementsPerThread = 1;
    auto workDiv = WorkDiv1D{blocksPerGrid, threadsPerBlock, elementsPerThread};

    aV2P::exec<Acc1D>(
        queue,
        workDiv,
        [] AV2P_FN_ACC(Acc1D const& acc,
                         AlpakaESTestDataADevice::ConstView a,
                         int const* b,
                         AlpakaESTestDataDDevice::View c,
                         int size) {
          const int32_t thread = aV2P::getIdx<aV2P::Grid, aV2P::Threads>(acc)[0u];
          const int32_t stride = aV2P::getWorkDiv<aV2P::Grid, aV2P::Threads>(acc)[0u];
          for (auto i = thread; i < size; i += stride) {
            c[i] = a.z()[i] + b[i];
          }
        },
        dataA.view(),
        dataB.data(),
        ret.view(),
        size);

    return ret;
  }
}  // namespace AV2P_ACCELERATOR_NAMESPACE
