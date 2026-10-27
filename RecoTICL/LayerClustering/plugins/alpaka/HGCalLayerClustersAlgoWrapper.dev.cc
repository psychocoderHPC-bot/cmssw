// Check that AV2P_HOST_ONLY is not defined during device compilation:
#ifdef AV2P_HOST_ONLY
#error AV2P_HOST_ONLY defined in device compilation
#endif

#include "RecoTICL/LayerClustering/interface/HGCalTilesConstants.h"

#include "HGCalLayerClustersAlgoWrapper.h"
#include "ConstantsForClusters.h"

#include "CLUEAlgoAlpaka.h"

namespace AV2P_ACCELERATOR_NAMESPACE {

  using namespace cms::alpakatools;
  using namespace hgcal::constants;

  void HGCalLayerClustersAlgoWrapper::run(Queue& queue,
                                          const unsigned int size,
                                          const float dc,
                                          const float kappa,
                                          const float outlierDeltaFactor,
                                          const HGCalSoARecHitsDeviceCollection::ConstView inputs,
                                          HGCalSoARecHitsExtraDeviceCollection::View outputs) const {
    CLUEAlgoAlpaka<AV2P_ACCELERATOR_NAMESPACE::Acc1D, Queue, HGCalSiliconTilesConstants, kHGCalLayers> algoStandalone(
        queue, dc, kappa, outlierDeltaFactor, false);

    algoStandalone.makeClustersCMSSW(size,
                                     inputs.dim1().data(),
                                     inputs.dim2().data(),
                                     inputs.layer().data(),
                                     inputs.energy().data(),
                                     inputs.sigmaNoise().data(),
                                     inputs.detid().data(),
                                     outputs.rho().data(),
                                     outputs.delta().data(),
                                     outputs.nearestHigher().data(),
                                     outputs.clusterIndex().data(),
                                     outputs.isSeed().data(),
                                     &outputs.numberOfClustersScalar());
  }

}  // namespace AV2P_ACCELERATOR_NAMESPACE
