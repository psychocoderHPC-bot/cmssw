#ifndef HeterogeneousCore_AlpakaInterface_interface_debug_h
#define HeterogeneousCore_AlpakaInterface_interface_debug_h

#include "HeterogeneousCore/AlpakaInterface/interface/config.h"

namespace AV2P_ACCELERATOR_NAMESPACE::debug {

  [[nodiscard]] AV2P_FN_ACC inline bool always_true() {
    volatile const bool flag = true;
    return flag;
  }

  [[nodiscard]] AV2P_FN_ACC inline bool always_false() {
    volatile const bool flag = false;
    return flag;
  }

  AV2P_FN_ACC inline void do_not_optimise(const auto& arg) {
    volatile const auto* ptr = &arg;
    (void)*ptr;
  }

}  // namespace AV2P_ACCELERATOR_NAMESPACE::debug

#endif  // HeterogeneousCore_AlpakaInterface_interface_debug_h
