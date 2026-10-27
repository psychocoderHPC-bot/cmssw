#ifndef RecoVertex_PixelVertexFinding_plugins_alpaka_clusterTracksByDensity_h
#define RecoVertex_PixelVertexFinding_plugins_alpaka_clusterTracksByDensity_h

#include <algorithm>
#include <cmath>
#include <cstdint>

#include <aV2P/alpaka.hpp>

#include "DataFormats/VertexSoA/interface/ZVertexSoA.h"
#include "HeterogeneousCore/AlpakaInterface/interface/HistoContainer.h"
#include "HeterogeneousCore/AlpakaInterface/interface/config.h"
#include "HeterogeneousCore/AlpakaInterface/interface/debug.h"
#include "HeterogeneousCore/AlpakaInterface/interface/warpsize.h"
#include "HeterogeneousCore/AlpakaInterface/interface/workdivision.h"
#include "RecoVertex/PixelVertexFinding/interface/PixelVertexWorkSpaceLayout.h"

#include "vertexFinder.h"

namespace AV2P_ACCELERATOR_NAMESPACE::vertexFinder {

  // This algo does not really scale as it works in a single block...
  // It should be good enough for <10K tracks we have.
  //
  // Based on Rodrighez&Laio algo.
  AV2P_FN_ACC AV2P_FN_INLINE void clusterTracksByDensity(Acc1D const& acc,
                                                             VtxSoAView& data,
                                                             TrkSoAView& trkdata,
                                                             WsSoAView& ws,
                                                             int minT,      // min number of neighbours to be "seed"
                                                             float eps,     // max absolute distance to cluster
                                                             float errmax,  // max error to be "seed"
                                                             float chi2max  // max normalized distance to cluster
  ) {
    constexpr bool verbose = false;

    if constexpr (verbose) {
      if (cms::alpakatools::once_per_block(acc))
        printf("params %d %f %f %f\n", minT, eps, errmax, chi2max);
    }

    auto nt = ws.ntrks();
    AV2P_ASSERT_ACC(static_cast<int>(nt) <= ws.metadata().size());
    AV2P_ASSERT_ACC(static_cast<int>(nt) <= trkdata.metadata().size());

    float const* __restrict__ zt = ws.zt().data();
    float const* __restrict__ ezt2 = ws.ezt2().data();
    uint8_t* __restrict__ izt = ws.izt().data();
    int32_t* __restrict__ iv = ws.iv().data();
    int32_t* __restrict__ nn = trkdata.ndof().data();
    AV2P_ASSERT_ACC(zt);
    AV2P_ASSERT_ACC(ezt2);
    AV2P_ASSERT_ACC(izt);
    AV2P_ASSERT_ACC(iv);
    AV2P_ASSERT_ACC(nn);

    using Hist = cms::alpakatools::HistoContainer<uint8_t, 256, 16000, 8, uint16_t>;
    auto& hist = aV2P::declareSharedVar<Hist, __COUNTER__>(acc);
    constexpr int warpSize = cms::alpakatools::warpSize;
    auto& hws = aV2P::declareSharedVar<Hist::Counter[warpSize], __COUNTER__>(acc);

    for (auto j : cms::alpakatools::uniform_elements(acc, Hist::totbins())) {
      hist.off[j] = 0;
    }
    for (auto j : cms::alpakatools::uniform_elements(acc, warpSize)) {
      hws[j] = 0;  // used by prefix scan in hist.finalize()
    }
    aV2P::syncBlockThreads(acc);

    if constexpr (verbose) {
      if (cms::alpakatools::once_per_block(acc))
        printf("booked hist with %d bins, size %d for %d tracks\n", hist.totbins(), hist.capacity(), nt);
    }

    AV2P_ASSERT_ACC(nt <= std::numeric_limits<Hist::value_type>::max());
    AV2P_ASSERT_ACC(nt <= hist.capacity());
    AV2P_ASSERT_ACC(eps <= 0.1f);  // see below

    // fill hist (bin shall be wider than "eps")
    for (auto i : cms::alpakatools::uniform_elements(acc, nt)) {
      int iz = static_cast<int>(zt[i] * 10.f);  // valid if eps <= 0.1
      // Equivalent of iz = std::clamp(iz, INT8_MIN, INT8_MAX)
      // which doesn't compile with gcc14 due to reference to __glibcxx_assert
      // See https://github.com/llvm/llvm-project/issues/95183
      int tmp_max = aV2P::math::max(acc, iz, INT8_MIN);
      iz = aV2P::math::min(acc, tmp_max, INT8_MAX);
      AV2P_ASSERT_ACC(iz - INT8_MIN >= 0);
      AV2P_ASSERT_ACC(iz - INT8_MIN < 256);
      izt[i] = iz - INT8_MIN;
      hist.count(acc, izt[i]);
      iv[i] = i;
      nn[i] = 0;
    }
    aV2P::syncBlockThreads(acc);

    hist.finalize(acc, hws);
    aV2P::syncBlockThreads(acc);

    AV2P_ASSERT_ACC(hist.size() == nt);
    for (auto i : cms::alpakatools::uniform_elements(acc, nt)) {
      hist.fill(acc, izt[i], uint16_t(i));
    }
    aV2P::syncBlockThreads(acc);

    // count neighbours
    const auto errmax2 = errmax * errmax;
    for (auto i : cms::alpakatools::uniform_elements(acc, nt)) {
      if (ezt2[i] > errmax2)
        continue;
      cms::alpakatools::forEachInBins(acc, hist, izt[i], 1, [&](uint32_t j) {
        if (i == j)
          return;
        auto dist = aV2P::math::abs(acc, zt[i] - zt[j]);
        if (dist > eps)
          return;
        if (dist * dist > chi2max * (ezt2[i] + ezt2[j]))
          return;
        nn[i]++;
      });
    }
    aV2P::syncBlockThreads(acc);

    // find closest above me .... (we ignore the possibility of two j at same distance from i)
    for (auto i : cms::alpakatools::uniform_elements(acc, nt)) {
      float mdist = eps;
      cms::alpakatools::forEachInBins(acc, hist, izt[i], 1, [&](uint32_t j) {
        if (nn[j] < nn[i])
          return;
        if (nn[j] == nn[i] && zt[j] >= zt[i])
          return;  // if equal use natural order...
        auto dist = aV2P::math::abs(acc, zt[i] - zt[j]);
        if (dist > mdist)
          return;
        if (dist * dist > chi2max * (ezt2[i] + ezt2[j]))
          return;  // (break natural order???)
        mdist = dist;
        iv[i] = j;  // assign to cluster (better be unique??)
      });
    }
    aV2P::syncBlockThreads(acc);

#ifdef GPU_DEBUG
    //  mini verification
    for (auto i : cms::alpakatools::uniform_elements(acc, nt)) {
      if (iv[i] != int(i))
        AV2P_ASSERT_ACC(iv[iv[i]] != int(i));
    }
    aV2P::syncBlockThreads(acc);
#endif

    // consolidate graph (percolate index of seed)
    for (auto i : cms::alpakatools::uniform_elements(acc, nt)) {
      auto m = iv[i];
      while (m != iv[m])
        m = iv[m];
      iv[i] = m;
    }

#ifdef GPU_DEBUG
    aV2P::syncBlockThreads(acc);
    //  mini verification
    for (auto i : cms::alpakatools::uniform_elements(acc, nt)) {
      if (iv[i] != int(i))
        AV2P_ASSERT_ACC(iv[iv[i]] != int(i));
    }
#endif

#ifdef GPU_DEBUG
    // and verify that we did not split any cluster...
    for (auto i : cms::alpakatools::uniform_elements(acc, nt)) {
      auto minJ = i;
      auto mdist = eps;
      cms::alpakatools::forEachInBins(acc, hist, izt[i], 1, [&](uint32_t j) {
        if (nn[j] < nn[i])
          return;
        if (nn[j] == nn[i] && zt[j] >= zt[i])
          return;  // if equal use natural order...
        auto dist = aV2P::math::abs(acc, zt[i] - zt[j]);
        if (dist > mdist)
          return;
        if (dist * dist > chi2max * (ezt2[i] + ezt2[j]))
          return;
        mdist = dist;
        minJ = j;
      });

      // should belong to the same cluster...
      AV2P_ASSERT_ACC(iv[i] == iv[minJ]);
      AV2P_ASSERT_ACC(nn[i] <= nn[iv[i]]);
    }
    aV2P::syncBlockThreads(acc);
#endif

    auto& foundClusters = aV2P::declareSharedVar<unsigned int, __COUNTER__>(acc);
    foundClusters = 0;
    aV2P::syncBlockThreads(acc);

    // find the number of different clusters, identified by a tracks with clus[i] == i and density larger than threshold;
    // mark these tracks with a negative id.
    for (auto i : cms::alpakatools::uniform_elements(acc, nt)) {
      if (iv[i] == int(i)) {
        if (nn[i] >= minT) {
          auto old = aV2P::atomicInc(acc, &foundClusters, 0xffffffff, aV2P::hierarchy::Threads{});
          iv[i] = -(old + 1);
        } else {  // noise
          iv[i] = -9998;
        }
      }
    }
    aV2P::syncBlockThreads(acc);

    AV2P_ASSERT_ACC(static_cast<int>(foundClusters) < data.metadata().size());

    // propagate the negative id to all the tracks in the cluster.
    for (auto i : cms::alpakatools::uniform_elements(acc, nt)) {
      if (iv[i] >= 0) {
        // mark each track in a cluster with the same id as the first one
        iv[i] = iv[iv[i]];
      }
    }
    aV2P::syncBlockThreads(acc);

    // adjust the cluster id to be a positive value starting from 0
    for (auto i : cms::alpakatools::uniform_elements(acc, nt)) {
      iv[i] = -iv[i] - 1;
    }

    ws.nvIntermediate() = foundClusters;
    data.nvFinal() = foundClusters;

    if constexpr (verbose) {
      if (cms::alpakatools::once_per_block(acc))
        printf("found %d proto vertices\n", foundClusters);
    }
  }

  class ClusterTracksByDensityKernel {
  public:
    AV2P_FN_ACC void operator()(Acc1D const& acc,
                                  VtxSoAView data,
                                  TrkSoAView trkdata,
                                  WsSoAView ws,
                                  int minT,      // min number of neighbours to be "seed"
                                  float eps,     // max absolute distance to cluster
                                  float errmax,  // max error to be "seed"
                                  float chi2max  // max normalized distance to cluster
    ) const {
      clusterTracksByDensity(acc, data, trkdata, ws, minT, eps, errmax, chi2max);
    }
  };

}  // namespace AV2P_ACCELERATOR_NAMESPACE::vertexFinder

#endif  // RecoVertex_PixelVertexFinding_plugins_alpaka_clusterTracksByDensity_h
