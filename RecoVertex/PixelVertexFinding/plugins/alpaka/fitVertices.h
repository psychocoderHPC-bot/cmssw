#ifndef RecoVertex_PixelVertexFinding_plugins_alpaka_fitVertices_h
#define RecoVertex_PixelVertexFinding_plugins_alpaka_fitVertices_h

#include <algorithm>
#include <cmath>
#include <cstdint>

#include <aV2P/alpaka.hpp>

#include "HeterogeneousCore/AlpakaInterface/interface/HistoContainer.h"
#include "HeterogeneousCore/AlpakaInterface/interface/config.h"
#include "HeterogeneousCore/AlpakaInterface/interface/workdivision.h"

#include "vertexFinder.h"

namespace AV2P_ACCELERATOR_NAMESPACE::vertexFinder {

  AV2P_FN_ACC AV2P_FN_INLINE __attribute__((always_inline)) void fitVertices(Acc1D const& acc,
                                                                                 VtxSoAView& pdata,
                                                                                 TrkSoAView& ptrkdata,
                                                                                 WsSoAView& pws,
                                                                                 float chi2Max  // for outlier rejection
  ) {
    constexpr bool verbose = false;  // in principle the compiler should optmize out if false

    auto& __restrict__ data = pdata;
    auto& __restrict__ trkdata = ptrkdata;
    auto& __restrict__ ws = pws;
    auto nt = ws.ntrks();
    float const* __restrict__ zt = ws.zt().data();
    float const* __restrict__ ezt2 = ws.ezt2().data();
    float* __restrict__ zv = data.zv().data();
    float* __restrict__ wv = data.wv().data();
    float* __restrict__ chi2 = data.chi2().data();
    uint32_t& nvFinal = data.nvFinal();
    uint32_t& nvIntermediate = ws.nvIntermediate();

    int32_t* __restrict__ nn = trkdata.ndof().data();
    int32_t* __restrict__ iv = ws.iv().data();

    AV2P_ASSERT_ACC(nvFinal <= nvIntermediate);
    nvFinal = nvIntermediate;
    auto foundClusters = nvFinal;

    // zero
    for (auto i : cms::alpakatools::uniform_elements(acc, foundClusters)) {
      zv[i] = 0;
      wv[i] = 0;
      chi2[i] = 0;
    }

    // only for test
    auto& noise = aV2P::declareSharedVar<int, __COUNTER__>(acc);

    if constexpr (verbose) {
      if (cms::alpakatools::once_per_block(acc))
        noise = 0;
    }
    aV2P::syncBlockThreads(acc);

    // compute cluster location
    for (auto i : cms::alpakatools::uniform_elements(acc, nt)) {
      if (iv[i] > 9990) {
        if constexpr (verbose)
          aV2P::atomicAdd(acc, &noise, 1, aV2P::hierarchy::Threads{});
        continue;
      }
      AV2P_ASSERT_ACC(iv[i] >= 0);
      AV2P_ASSERT_ACC(iv[i] < int(foundClusters));
      auto w = 1.f / ezt2[i];
      aV2P::atomicAdd(acc, &zv[iv[i]], zt[i] * w, aV2P::hierarchy::Threads{});
      aV2P::atomicAdd(acc, &wv[iv[i]], w, aV2P::hierarchy::Threads{});
    }

    aV2P::syncBlockThreads(acc);
    // reuse nn
    for (auto i : cms::alpakatools::uniform_elements(acc, foundClusters)) {
      if constexpr (verbose) {
        if (not(wv[i] > 0.f)) {
          printf("ERROR: wv[%d] (%f) > 0.f failed\n", i, wv[i]);
          // printing info on tracks associated to this vertex
          for (auto trk_i = 0u; trk_i < nt; ++trk_i) {
            if (iv[trk_i] != int(i)) {
              continue;
            }
            printf("   iv[%d]=%d zt[%d]=%f ezt2[%d]=%f\n", trk_i, iv[trk_i], trk_i, zt[trk_i], trk_i, ezt2[trk_i]);
          }
        }
      }
      AV2P_ASSERT_ACC(wv[i] > 0.f);
      zv[i] /= wv[i];
      nn[i] = -1;  // ndof
    }
    aV2P::syncBlockThreads(acc);

    // compute chi2
    for (auto i : cms::alpakatools::uniform_elements(acc, nt)) {
      if (iv[i] > 9990)
        continue;

      auto c2 = zv[iv[i]] - zt[i];
      c2 *= c2 / ezt2[i];
      if (c2 > chi2Max) {
        iv[i] = 9999;
        continue;
      }
      aV2P::atomicAdd(acc, &chi2[iv[i]], c2, aV2P::hierarchy::Blocks{});
      aV2P::atomicAdd(acc, &nn[iv[i]], 1, aV2P::hierarchy::Blocks{});
    }
    aV2P::syncBlockThreads(acc);

    for (auto i : cms::alpakatools::uniform_elements(acc, foundClusters)) {
      if (nn[i] > 0) {
        wv[i] *= float(nn[i]) / chi2[i];
      }
    }
    if constexpr (verbose) {
      if (cms::alpakatools::once_per_block(acc)) {
        printf("found %d proto clusters ", foundClusters);
        printf("and %d noise\n", noise);
      }
    }
  }

  class FitVerticesKernel {
  public:
    AV2P_FN_ACC void operator()(Acc1D const& acc,
                                  VtxSoAView pdata,
                                  TrkSoAView ptrkdata,
                                  WsSoAView pws,
                                  float chi2Max  // for outlier rejection
    ) const {
      fitVertices(acc, pdata, ptrkdata, pws, chi2Max);
    }
  };

}  // namespace AV2P_ACCELERATOR_NAMESPACE::vertexFinder

#endif  // RecoVertex_PixelVertexFinding_plugins_alpaka_fitVertices_h
