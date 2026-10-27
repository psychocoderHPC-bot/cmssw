#ifndef RecoLocalTracker_SiPixelRecHits_alpaka_PixelRecHits_h
#define RecoLocalTracker_SiPixelRecHits_alpaka_PixelRecHits_h

// C++ headers
#include <cassert>
#include <cstdint>
#include <limits>
#include <type_traits>

// Alpaka headers
#include <aV2P/alpaka.hpp>

// CMSSW headers
#include "DataFormats/BeamSpot/interface/BeamSpotPOD.h"
#include "DataFormats/Math/interface/approx_atan2.h"
#include "DataFormats/SiPixelClusterSoA/interface/ClusteringConstants.h"
#include "DataFormats/SiPixelClusterSoA/interface/SiPixelClustersSoA.h"
#include "DataFormats/SiPixelDigiSoA/interface/SiPixelDigisSoA.h"
#include "DataFormats/TrackingRecHitSoA/interface/TrackingRecHitsSoA.h"
#include "HeterogeneousCore/AlpakaInterface/interface/config.h"
#include "HeterogeneousCore/AlpakaInterface/interface/workdivision.h"
#include "RecoLocalTracker/SiPixelRecHits/interface/pixelCPEforDevice.h"

//#define GPU_DEBUG
//#define ONLY_TRIPLETS_IN_HOLE
namespace AV2P_ACCELERATOR_NAMESPACE {
  namespace pixelRecHits {

    template <typename TrackerTraits>
    class GetHits {
    public:
      AV2P_FN_ACC void operator()(Acc1D const& acc,
                                    pixelCPEforDevice::ParamsOnDeviceT<TrackerTraits> const* __restrict__ cpeParams,
                                    BeamSpotPOD const* __restrict__ bs,
                                    SiPixelDigisSoAConstView digis,
                                    uint32_t numElements,
                                    uint32_t nonEmptyModules,
                                    SiPixelClustersSoAConstView clusters,
                                    ::reco::TrackingRecHitView hits) const {
        AV2P_ASSERT_ACC(cpeParams);

        // outer loop: one block per module
        for (uint32_t module : cms::alpakatools::independent_groups(acc, nonEmptyModules)) {
#ifdef ONLY_TRIPLETS_IN_HOLE
          // This is necessary only once - consider moving it somewhere else.
          // Copy the average geometry corrected by the beamspot.
          if (0 == module) {
            auto const& ag = cpeParams->averageGeometry();
            auto nLadders = TrackerTraits::numberOfLaddersInBarrel;

            for (uint32_t il : cms::alpakatools::independent_group_elements(acc, nLadders)) {
              agc.ladderZ[il] = ag.ladderZ[il] - bs->z;
              agc.ladderX[il] = ag.ladderX[il] - bs->x;
              agc.ladderY[il] = ag.ladderY[il] - bs->y;
              agc.ladderR[il] = sqrt(agc.ladderX[il] * agc.ladderX[il] + agc.ladderY[il] * agc.ladderY[il]);
              agc.ladderMinZ[il] = ag.ladderMinZ[il] - bs->z;
              agc.ladderMaxZ[il] = ag.ladderMaxZ[il] - bs->z;
            }

            if (cms::alpakatools::once_per_block(acc)) {
              agc.endCapZ[0] = ag.endCapZ[0] - bs->z;
              agc.endCapZ[1] = ag.endCapZ[1] - bs->z;
            }
          }
#endif  // ONLY_TRIPLETS_IN_HOLE

          // to be moved in common namespace...
          using pixelClustering::invalidModuleId;
          constexpr int32_t maxHitsInIter = pixelCPEforDevice::MaxHitsInIter;

          auto me = clusters[module].moduleId();
          int nclus = clusters[me].clusInModule();

          // skip empty modules
          if (0 == nclus)
            continue;

#ifdef GPU_DEBUG
          if (cms::alpakatools::once_per_block(acc)) {
            auto k = clusters[1 + module].moduleStart();
            while (digis[k].moduleId() == invalidModuleId)
              ++k;
            AV2P_ASSERT_ACC(digis[k].moduleId() == me);
          }

          if (me % 100 == 1)
            if (cms::alpakatools::once_per_block(acc))
              printf("hitbuilder: %d clusters in module %d. will write at %d\n",
                     nclus,
                     me,
                     clusters[me].clusModuleStart());
#endif

          auto& clusParams = aV2P::declareSharedVar<pixelCPEforDevice::ClusParams, __COUNTER__>(acc);

          for (int startClus = 0, endClus = nclus; startClus < endClus; startClus += maxHitsInIter) {
            auto first = clusters[1 + module].moduleStart();
            int nClusInIter = aV2P::math::min(acc, maxHitsInIter, endClus - startClus);
            int lastClus = startClus + nClusInIter;
            AV2P_ASSERT_ACC(nClusInIter <= nclus);
            AV2P_ASSERT_ACC(nClusInIter > 0);
            AV2P_ASSERT_ACC(lastClus <= nclus);
            AV2P_ASSERT_ACC(nclus > maxHitsInIter || (0 == startClus && nClusInIter == nclus && lastClus == nclus));

            // init
            for (uint32_t ic : cms::alpakatools::independent_group_elements(acc, nClusInIter)) {
              clusParams.minRow[ic] = std::numeric_limits<uint32_t>::max();
              clusParams.maxRow[ic] = 0;
              clusParams.minCol[ic] = std::numeric_limits<uint32_t>::max();
              clusParams.maxCol[ic] = 0;
              clusParams.charge[ic] = 0;
              clusParams.q_f_X[ic] = 0;
              clusParams.q_l_X[ic] = 0;
              clusParams.q_f_Y[ic] = 0;
              clusParams.q_l_Y[ic] = 0;
            }

            aV2P::syncBlockThreads(acc);

            // one thread or element per "digi"
            for (uint32_t i : cms::alpakatools::independent_group_elements(acc, first, numElements)) {
              auto id = digis[i].moduleId();
              if (id == invalidModuleId)
                continue;  // not valid
              if (id != me)
                break;  // end of module
              auto cl = digis[i].clus();
              if (cl < startClus || cl >= lastClus)
                continue;
              cl -= startClus;
              AV2P_ASSERT_ACC(cl >= 0);
              AV2P_ASSERT_ACC(cl < maxHitsInIter);
              auto x = digis[i].xx();
              auto y = digis[i].yy();
              aV2P::atomicMin(acc, &clusParams.minRow[cl], (uint32_t)x, aV2P::hierarchy::Threads{});
              aV2P::atomicMax(acc, &clusParams.maxRow[cl], (uint32_t)x, aV2P::hierarchy::Threads{});
              aV2P::atomicMin(acc, &clusParams.minCol[cl], (uint32_t)y, aV2P::hierarchy::Threads{});
              aV2P::atomicMax(acc, &clusParams.maxCol[cl], (uint32_t)y, aV2P::hierarchy::Threads{});
            }

            aV2P::syncBlockThreads(acc);

            auto pixmx = cpeParams->detParams(me).pixmx;
            for (uint32_t i : cms::alpakatools::independent_group_elements(acc, first, numElements)) {
              auto id = digis[i].moduleId();
              if (id == invalidModuleId)
                continue;  // not valid
              if (id != me)
                break;  // end of module
              auto cl = digis[i].clus();
              if (cl < startClus || cl >= lastClus)
                continue;
              cl -= startClus;
              AV2P_ASSERT_ACC(cl >= 0);
              AV2P_ASSERT_ACC(cl < maxHitsInIter);
              auto x = digis[i].xx();
              auto y = digis[i].yy();
              auto ch = digis[i].adc();
              aV2P::atomicAdd(acc, &clusParams.charge[cl], (int32_t)ch, aV2P::hierarchy::Threads{});
              ch = aV2P::math::min(acc, ch, pixmx);
              if (clusParams.minRow[cl] == x)
                aV2P::atomicAdd(acc, &clusParams.q_f_X[cl], (int32_t)ch, aV2P::hierarchy::Threads{});
              if (clusParams.maxRow[cl] == x)
                aV2P::atomicAdd(acc, &clusParams.q_l_X[cl], (int32_t)ch, aV2P::hierarchy::Threads{});
              if (clusParams.minCol[cl] == y)
                aV2P::atomicAdd(acc, &clusParams.q_f_Y[cl], (int32_t)ch, aV2P::hierarchy::Threads{});
              if (clusParams.maxCol[cl] == y)
                aV2P::atomicAdd(acc, &clusParams.q_l_Y[cl], (int32_t)ch, aV2P::hierarchy::Threads{});
            }

            aV2P::syncBlockThreads(acc);

            // next one cluster per thread...
            first = clusters[me].clusModuleStart() + startClus;
            for (uint32_t ic : cms::alpakatools::independent_group_elements(acc, nClusInIter)) {
              auto h = first + ic;  // output index in global memory

              assert(h < (uint32_t)hits.metadata().size());
              assert(h < clusters[me + 1].clusModuleStart());

              pixelCPEforDevice::position<TrackerTraits>(
                  cpeParams->commonParams(), cpeParams->detParams(me), clusParams, ic);

              pixelCPEforDevice::errorFromDB<TrackerTraits>(
                  cpeParams->commonParams(), cpeParams->detParams(me), clusParams, ic);

              // store it
              hits[h].chargeAndStatus().charge = clusParams.charge[ic];
              hits[h].chargeAndStatus().status = clusParams.status[ic];
              hits[h].detectorIndex() = me;

              // local coordinates for computations
              float xl, yl;
              hits[h].xLocal() = xl = clusParams.xpos[ic];
              hits[h].yLocal() = yl = clusParams.ypos[ic];

              hits[h].clusterSizeX() = clusParams.xsize[ic];
              hits[h].clusterSizeY() = clusParams.ysize[ic];

              hits[h].xerrLocal() = clusParams.xerr[ic] * clusParams.xerr[ic] + cpeParams->detParams(me).apeXX;
              hits[h].yerrLocal() = clusParams.yerr[ic] * clusParams.yerr[ic] + cpeParams->detParams(me).apeYY;

              // global coordinates and phi computation
              float xg, yg, zg;
              cpeParams->detParams(me).frame.toGlobal(xl, yl, xg, yg, zg);
              // correct for the beamspot position
              xg -= bs->x;
              yg -= bs->y;
              zg -= bs->z;

              hits[h].xGlobal() = xg;
              hits[h].yGlobal() = yg;
              hits[h].zGlobal() = zg;
              hits[h].rGlobal() = aV2P::math::sqrt(acc, xg * xg + yg * yg);
              hits[h].iphi() = unsafe_atan2s<7>(yg, xg);
            }
            aV2P::syncBlockThreads(acc);
          }  // end loop on batches
        }
      }
    };

  }  // namespace pixelRecHits
}  // namespace AV2P_ACCELERATOR_NAMESPACE

#endif  // RecoLocalTracker_SiPixelRecHits_plugins_alpaka_PixelRecHits_h
