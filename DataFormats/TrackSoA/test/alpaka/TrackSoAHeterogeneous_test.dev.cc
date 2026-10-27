#include <type_traits>

#include <aV2P/alpaka.hpp>

#include "DataFormats/TrackSoA/interface/TrackDefinitions.h"
#include "DataFormats/TrackSoA/interface/TracksDevice.h"
#include "DataFormats/TrackSoA/interface/TracksHost.h"
#include "DataFormats/TrackSoA/interface/alpaka/TracksSoACollection.h"
#include "Geometry/CommonTopologies/interface/SimplePixelTopology.h"
#include "HeterogeneousCore/AlpakaInterface/interface/config.h"

#include "TrackSoAHeterogeneous_test.h"

using namespace reco;

using Quality = pixelTrack::Quality;
namespace AV2P_ACCELERATOR_NAMESPACE {
  using namespace cms::alpakatools;
  namespace testTrackSoA {

    // Kernel which fills the TrackSoAView with data
    // to test writing to it
    class TestFillKernel {
    public:
      AV2P_FN_ACC void operator()(Acc1D const& acc, TrackSoAView tracks_view, int32_t nTracks) const {
        if (cms::alpakatools::once_per_grid(acc)) {
          tracks_view.nTracks() = nTracks;
        }

        for (int32_t j : uniform_elements(acc, nTracks)) {
          tracks_view[j].pt() = (float)j;
          tracks_view[j].eta() = (float)j;
          tracks_view[j].chi2() = (float)j;
          tracks_view[j].quality() = (Quality)(j % 256);
          tracks_view[j].nLayers() = j % 128;
          tracks_view[j].hitOffsets() = j;
        }
      }
    };

    // Kernel which reads from the TrackSoAView to verify
    // that it was written correctly from the fill kernel
    class TestVerifyKernel {
    public:
      AV2P_FN_ACC void operator()(Acc1D const& acc, TrackSoAConstView tracks_view, int32_t nTracks) const {
        if (cms::alpakatools::once_per_grid(acc)) {
          AV2P_ASSERT(tracks_view.nTracks() == nTracks);
        }
        for (int32_t j : uniform_elements(acc, tracks_view.nTracks())) {
          AV2P_ASSERT(abs(tracks_view[j].pt() - (float)j) < .0001);
          AV2P_ASSERT(abs(tracks_view[j].eta() - (float)j) < .0001);
          AV2P_ASSERT(abs(tracks_view[j].chi2() - (float)j) < .0001);
          AV2P_ASSERT(tracks_view[j].quality() == (Quality)(j % 256));
          AV2P_ASSERT(tracks_view[j].nLayers() == j % 128);
          AV2P_ASSERT(tracks_view[j].hitOffsets() == uint32_t(j));
        }
      }
    };

    // Host function which invokes the two kernels above
    void runKernels(TrackSoAView tracks_view, Queue& queue) {
      int32_t tracks = 420;
      uint32_t items = 64;
      uint32_t groups = divide_up_by(tracks, items);
      auto workDiv = make_workdiv<Acc1D>(groups, items);
      aV2P::exec<Acc1D>(queue, workDiv, TestFillKernel{}, tracks_view, tracks);
      aV2P::exec<Acc1D>(queue, workDiv, TestVerifyKernel{}, tracks_view, tracks);
    }

  }  // namespace testTrackSoA

}  // namespace AV2P_ACCELERATOR_NAMESPACE
