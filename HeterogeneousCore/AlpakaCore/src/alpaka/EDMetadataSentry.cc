#include <aV2P/alpaka.hpp>

#include "HeterogeneousCore/AlpakaCore/interface/alpaka/EDMetadataSentry.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/chooseDevice.h"

namespace AV2P_ACCELERATOR_NAMESPACE::detail {

  EDMetadataSentry::EDMetadataSentry(edm::StreamID streamID, bool synchronize)
      : metadata_(std::make_shared<EDMetadata>(detail::chooseDevice(streamID))), synchronize_(synchronize) {}

  void EDMetadataSentry::finish(bool launchedAsyncWork) {
    if constexpr (not std::is_same_v<Queue, aV2P::Queue<Device, aV2P::Blocking>>) {
      if (launchedAsyncWork and synchronize_) {
        aV2P::wait(metadata_->queue());
      }
    }

    if (launchedAsyncWork and not synchronize_) {
      metadata_->recordEvent();
    }
  }

}  // namespace AV2P_ACCELERATOR_NAMESPACE::detail
