#include "HeterogeneousCore/AlpakaInterface/interface/workdivision.h"

#include "testPtrAlgoAsync.h"

namespace AV2P_ACCELERATOR_NAMESPACE {
  portabletest::TestProductWithPtr<Device> testPtrAlgoAsync(Queue& queue, int size) {
    portabletest::TestProductWithPtr<Device> ret{queue, size};
    using View = portabletest::TestProductWithPtr<Device>::View;
    aV2P::exec<Acc1D>(
        queue,
        cms::alpakatools::make_workdiv<Acc1D>(1, 1),
        [] AV2P_FN_ACC(Acc1D const& acc, View view) {
          if (cms::alpakatools::once_per_grid(acc)) {
            portabletest::setPtrInTestProductWithPtr(view);
          }
        },
        ret.view());
    aV2P::exec<Acc1D>(
        queue,
        cms::alpakatools::make_workdiv<Acc1D>(1, size),
        [] AV2P_FN_ACC(Acc1D const& acc, View view) {
          for (auto i : cms::alpakatools::uniform_elements(acc)) {
            view.ptr()[i] = 2 * i + 1;
          }
        },
        ret.view());
    return ret;
  }
}  // namespace AV2P_ACCELERATOR_NAMESPACE
