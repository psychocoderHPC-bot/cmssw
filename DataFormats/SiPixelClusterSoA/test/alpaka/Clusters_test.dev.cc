#include <aV2P/alpaka.hpp>

#include "DataFormats/SiPixelClusterSoA/interface/SiPixelClustersDevice.h"
#include "DataFormats/SiPixelClusterSoA/interface/SiPixelClustersHost.h"
#include "DataFormats/SiPixelClusterSoA/interface/alpaka/SiPixelClustersSoACollection.h"
#include "HeterogeneousCore/AlpakaInterface/interface/config.h"
#include "HeterogeneousCore/AlpakaInterface/interface/workdivision.h"

#include "Clusters_test.h"

using namespace aV2P;

namespace AV2P_ACCELERATOR_NAMESPACE::testClusterSoA {

  class TestFillKernel {
  public:
    AV2P_FN_ACC void operator()(Acc1D const& acc, SiPixelClustersSoAView clust_view) const {
      for (int32_t j : cms::alpakatools::uniform_elements(acc, clust_view.metadata().size())) {
        clust_view[j].moduleStart() = j;
        clust_view[j].clusInModule() = j * 2;
        clust_view[j].moduleId() = j * 3;
        clust_view[j].clusModuleStart() = j * 4;
      }
    }
  };

  class TestVerifyKernel {
  public:
    AV2P_FN_ACC void operator()(Acc1D const& acc, SiPixelClustersSoAConstView clust_view) const {
      for (uint32_t j : cms::alpakatools::uniform_elements(acc, clust_view.metadata().size())) {
        AV2P_ASSERT_ACC(clust_view[j].moduleStart() == j);
        AV2P_ASSERT_ACC(clust_view[j].clusInModule() == j * 2);
        AV2P_ASSERT_ACC(clust_view[j].moduleId() == j * 3);
        AV2P_ASSERT_ACC(clust_view[j].clusModuleStart() == j * 4);
      }
    }
  };

  void runKernels(SiPixelClustersSoAView clust_view, Queue& queue) {
    uint32_t items = 64;
    uint32_t groups = cms::alpakatools::divide_up_by(clust_view.metadata().size(), items);
    auto workDiv = cms::alpakatools::make_workdiv<Acc1D>(groups, items);
    aV2P::exec<Acc1D>(queue, workDiv, TestFillKernel{}, clust_view);
    aV2P::exec<Acc1D>(queue, workDiv, TestVerifyKernel{}, clust_view);
  }

}  // namespace AV2P_ACCELERATOR_NAMESPACE::testClusterSoA
