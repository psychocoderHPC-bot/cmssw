#include "HeterogeneousCore/AlpakaInterface/interface/config.h"
#include "HeterogeneousCore/AlpakaTest/interface/HostOnlyType.h"
#include "HeterogeneousCore/AlpakaTest/interface/alpaka/printAnswer.h"

namespace AV2P_ACCELERATOR_NAMESPACE::alpakatest {
  using namespace ::alpakatest;

  // A simple function to demonstarte the dependency on host-only types from alpaka libraries
  void printAnswer() {
    HostOnlyType answer(42);
    answer.print();
  }

}  // namespace AV2P_ACCELERATOR_NAMESPACE::alpakatest
