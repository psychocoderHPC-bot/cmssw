#ifndef RecoVertex_PixelVertexFinding_plugins_alpaka_splitVertices_h
#define RecoVertex_PixelVertexFinding_plugins_alpaka_splitVertices_h

#include <algorithm>
#include <cmath>
#include <cstdint>

#include <aV2P/alpaka.hpp>

#include "HeterogeneousCore/AlpakaInterface/interface/HistoContainer.h"
#include "HeterogeneousCore/AlpakaInterface/interface/config.h"
#include "HeterogeneousCore/AlpakaInterface/interface/workdivision.h"

#include "vertexFinder.h"

namespace AV2P_ACCELERATOR_NAMESPACE::vertexFinder {

  using VtxSoAView = ::reco::ZVertexSoAView;
  using TrkSoAView = ::reco::ZVertexTracksSoAView;
  using WsSoAView = ::vertexFinder::PixelVertexWorkSpaceSoAView;

  AV2P_FN_ACC AV2P_FN_INLINE __attribute__((always_inline)) void splitVertices(
      Acc1D const& acc, VtxSoAView& data, TrkSoAView& trkdata, WsSoAView& ws, float maxChi2) {
    constexpr bool verbose = false;  // in principle the compiler should optmize out if false
    constexpr uint32_t MAXTK = 512;

    auto& it = aV2P::declareSharedVar<uint32_t[MAXTK], __COUNTER__>(acc);   // track index
    auto& zz = aV2P::declareSharedVar<float[MAXTK], __COUNTER__>(acc);      // z pos
    auto& newV = aV2P::declareSharedVar<uint8_t[MAXTK], __COUNTER__>(acc);  // 0 or 1
    auto& ww = aV2P::declareSharedVar<float[MAXTK], __COUNTER__>(acc);      // z weight
    auto& nq = aV2P::declareSharedVar<uint32_t, __COUNTER__>(acc);          // number of track for this vertex

    // one vertex per block
    for (auto kv : cms::alpakatools::independent_groups(acc, data.nvFinal())) {
      int32_t ndof = trkdata[kv].ndof();
      if (ndof < 4)
        continue;
      if (data[kv].chi2() < maxChi2 * float(ndof))
        continue;

      AV2P_ASSERT_ACC(ndof < int32_t(MAXTK));

      if ((uint32_t)ndof >= MAXTK)
        continue;  // too bad FIXME

      if (cms::alpakatools::once_per_block(acc)) {
        // reset the number of tracks for the current vertex
        nq = 0u;
      }
      aV2P::syncBlockThreads(acc);

      // cache the data of the tracks associated to the current vertex into shared memory
      for (auto k : cms::alpakatools::independent_group_elements(acc, ws.ntrks())) {
        if (ws[k].iv() == int(kv)) {
          auto index = aV2P::atomicInc(acc, &nq, MAXTK, aV2P::hierarchy::Threads{});
          it[index] = k;
          zz[index] = ws[k].zt() - data[kv].zv();
          newV[index] = zz[index] < 0 ? 0 : 1;
          ww[index] = 1.f / ws[k].ezt2();
        }
      }

      // the new vertices
      auto& znew = aV2P::declareSharedVar<float[2], __COUNTER__>(acc);
      auto& wnew = aV2P::declareSharedVar<float[2], __COUNTER__>(acc);
      aV2P::syncBlockThreads(acc);

      AV2P_ASSERT_ACC(int(nq) == ndof + 1);

      int maxiter = 20;
      // kt-min....
      bool more = true;
      while (aV2P::syncBlockThreadsPredicate<aV2P::BlockOr>(acc, more)) {
        more = false;
        if (cms::alpakatools::once_per_block(acc)) {
          znew[0] = 0;
          znew[1] = 0;
          wnew[0] = 0;
          wnew[1] = 0;
        }
        aV2P::syncBlockThreads(acc);

        for (auto k : cms::alpakatools::uniform_elements(acc, nq)) {
          auto i = newV[k];
          aV2P::atomicAdd(acc, &znew[i], zz[k] * ww[k], aV2P::hierarchy::Threads{});
          aV2P::atomicAdd(acc, &wnew[i], ww[k], aV2P::hierarchy::Threads{});
        }
        aV2P::syncBlockThreads(acc);

        if (cms::alpakatools::once_per_block(acc)) {
          znew[0] /= wnew[0];
          znew[1] /= wnew[1];
        }
        aV2P::syncBlockThreads(acc);

        for (auto k : cms::alpakatools::uniform_elements(acc, nq)) {
          auto d0 = fabs(zz[k] - znew[0]);
          auto d1 = fabs(zz[k] - znew[1]);
          auto newer = d0 < d1 ? 0 : 1;
          more |= newer != newV[k];
          newV[k] = newer;
        }
        --maxiter;
        if (maxiter <= 0)
          more = false;
      }

      // avoid empty vertices
      if (0 == wnew[0] || 0 == wnew[1])
        continue;

      // quality cut
      auto dist2 = (znew[0] - znew[1]) * (znew[0] - znew[1]);

      auto chi2Dist = dist2 / (1.f / wnew[0] + 1.f / wnew[1]);

      if constexpr (verbose) {
        if (cms::alpakatools::once_per_block(acc))
          printf("inter %d %f %f\n", 20 - maxiter, chi2Dist, dist2 * data[kv].wv());
      }

      if (chi2Dist < 4)
        continue;

      // get a new global vertex
      auto& igv = aV2P::declareSharedVar<uint32_t, __COUNTER__>(acc);
      if (cms::alpakatools::once_per_block(acc))
        igv = aV2P::atomicAdd(acc, &ws.nvIntermediate(), 1u, aV2P::hierarchy::Blocks{});
      aV2P::syncBlockThreads(acc);
      for (auto k : cms::alpakatools::uniform_elements(acc, nq)) {
        if (1 == newV[k])
          ws[it[k]].iv() = igv;
      }

      // synchronise the threads before starting the next iteration of the loop over the vertices and resetting the shared memory
      aV2P::syncBlockThreads(acc);
    }  // loop on vertices
  }

  class SplitVerticesKernel {
  public:
    AV2P_FN_ACC void operator()(
        Acc1D const& acc, VtxSoAView data, TrkSoAView trkdata, WsSoAView ws, float maxChi2) const {
      splitVertices(acc, data, trkdata, ws, maxChi2);
    }
  };

}  // namespace AV2P_ACCELERATOR_NAMESPACE::vertexFinder

#endif  // RecoVertex_PixelVertexFinding_plugins_alpaka_splitVertices_h
