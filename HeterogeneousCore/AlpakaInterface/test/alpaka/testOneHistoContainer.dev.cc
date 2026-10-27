#include <algorithm>
#include <iostream>
#include <limits>
#include <random>

#include <aV2P/alpaka.hpp>

#include "FWCore/Utilities/interface/stringize.h"
#include "HeterogeneousCore/AlpakaInterface/interface/config.h"
#include "HeterogeneousCore/AlpakaInterface/interface/memory.h"
#include "HeterogeneousCore/AlpakaInterface/interface/workdivision.h"
#include "HeterogeneousCore/AlpakaInterface/interface/HistoContainer.h"

using namespace cms::alpakatools;
using namespace AV2P_ACCELERATOR_NAMESPACE;

template <int NBINS, int S, int DELTA>
struct mykernel {
  template <typename T>
  AV2P_FN_ACC void operator()(Acc1D const& acc, T const* __restrict__ v, uint32_t N) const {
    AV2P_ASSERT_ACC(v);
    AV2P_ASSERT_ACC(N == 12000);

    if (once_per_block(acc)) {
      printf("start kernel for %d data\n", N);
    }

    using Hist = HistoContainer<T, NBINS, 12000, S, uint16_t>;

    auto& hist = aV2P::declareSharedVar<Hist, __COUNTER__>(acc);
    auto& ws = aV2P::declareSharedVar<typename Hist::Counter[32], __COUNTER__>(acc);

    // set off zero
    for (auto j : uniform_elements(acc, Hist::totbins())) {
      hist.off[j] = 0;
    }
    aV2P::syncBlockThreads(acc);

    // set bins zero
    for (auto j : uniform_elements(acc, Hist::totbins())) {
      hist.content[j] = 0;
    }
    aV2P::syncBlockThreads(acc);

    // count
    for (auto j : uniform_elements(acc, N)) {
      hist.count(acc, v[j]);
    }
    aV2P::syncBlockThreads(acc);

    AV2P_ASSERT_ACC(0 == hist.size());
    aV2P::syncBlockThreads(acc);

    // finalize
    hist.finalize(acc, ws);
    aV2P::syncBlockThreads(acc);

    AV2P_ASSERT_ACC(N == hist.size());

    // verify
    for ([[maybe_unused]] auto j : uniform_elements(acc, Hist::nbins())) {
      AV2P_ASSERT_ACC(hist.off[j] <= hist.off[j + 1]);
    }
    aV2P::syncBlockThreads(acc);

    for (auto j : uniform_elements(acc, 32)) {
      ws[j] = 0;  // used by prefix scan...
    }
    aV2P::syncBlockThreads(acc);

    // fill
    for (auto j : uniform_elements(acc, N)) {
      hist.fill(acc, v[j], j);
    }
    aV2P::syncBlockThreads(acc);

    AV2P_ASSERT_ACC(0 == hist.off[0]);
    AV2P_ASSERT_ACC(N == hist.size());

    // bin
#ifndef NDEBUG
    for (auto j : uniform_elements(acc, hist.size() - 1)) {
      auto p = hist.begin() + j;
      AV2P_ASSERT_ACC((*p) < N);
      [[maybe_unused]] auto k1 = Hist::bin(v[*p]);
      [[maybe_unused]] auto k2 = Hist::bin(v[*(p + 1)]);
      AV2P_ASSERT_ACC(k2 >= k1);
    }
#endif

    // forEachInWindow
    for (auto i : uniform_elements(acc, hist.size())) {
      auto p = hist.begin() + i;
      auto j = *p;
#ifndef NDEBUG
      auto b0 = Hist::bin(v[j]);
#endif
      [[maybe_unused]] int tot = 0;
      auto ftest = [&](unsigned int k) {
        AV2P_ASSERT_ACC(k < N);
        ++tot;
      };
      forEachInWindow(hist, v[j], v[j], ftest);
#ifndef NDEBUG
      [[maybe_unused]] int rtot = hist.size(b0);
      AV2P_ASSERT_ACC(tot == rtot);
#endif
      tot = 0;
      auto vm = int(v[j]) - DELTA;
      auto vp = int(v[j]) + DELTA;
      constexpr int vmax = NBINS != 128 ? NBINS * 2 - 1 : std::numeric_limits<T>::max();
      vm = aV2P::math::max(acc, vm, 0);
      vm = aV2P::math::min(acc, vm, vmax);
      vp = aV2P::math::min(acc, vp, vmax);
      vp = aV2P::math::max(acc, vp, 0);
      AV2P_ASSERT_ACC(vp >= vm);
      forEachInWindow(hist, vm, vp, ftest);
#ifndef NDEBUG
      int bp = Hist::bin(vp);
      int bm = Hist::bin(vm);
      rtot = hist.end(bp) - hist.begin(bm);
      AV2P_ASSERT_ACC(tot == rtot);
#endif
    }
  }
};

template <typename T, int NBINS = 128, int S = 8 * sizeof(T), int DELTA = 1000>
void go(const DevHost& host, const Device& device, Queue& queue) {
  std::mt19937 eng;

  int rmin = std::numeric_limits<T>::min();
  int rmax = std::numeric_limits<T>::max();
  if (NBINS != 128) {
    rmin = 0;
    rmax = NBINS * 2 - 1;
  }

  std::uniform_int_distribution<T> rgen(rmin, rmax);
  constexpr unsigned int N = 12000;

  using Hist = HistoContainer<T, NBINS, N, S>;
  std::cout << "HistoContainer " << Hist::nbits() << ' ' << Hist::nbins() << ' ' << Hist{}.capacity() << ' '
            << (rmax - rmin) / Hist::nbins() << std::endl;
  std::cout << "bins " << int(Hist::bin(0)) << ' ' << int(Hist::bin(rmin)) << ' ' << int(Hist::bin(rmax)) << std::endl;

  auto v = make_host_buffer<T[]>(queue, N);
  auto v_d = make_device_buffer<T[]>(queue, N);

  for (int it = 0; it < 5; ++it) {
    for (long long j = 0; j < N; j++)
      v[j] = rgen(eng);
    if (it == 2)
      for (long long j = N / 2; j < N / 2 + N / 4; j++)
        v[j] = 4;

    aV2P::memcpy(queue, v_d, v);

    const auto threadsPerBlockOrElementsPerThread = 256u;
    const auto blocksPerGrid = 1u;
    const auto workDiv = make_workdiv<Acc1D>(blocksPerGrid, threadsPerBlockOrElementsPerThread);
    aV2P::enqueue(queue, aV2P::createTaskKernel<Acc1D>(workDiv, mykernel<NBINS, S, DELTA>(), v_d.data(), N));
  }
  aV2P::wait(queue);
}

int main() {
  auto const& host = cms::alpakatools::host();

  // get the list of devices on the current platform
  auto const& devices = cms::alpakatools::devices<Platform>();
  if (devices.empty()) {
    std::cerr << "No devices available for the " EDM_STRINGIZE(AV2P_ACCELERATOR_NAMESPACE) " backend, "
      "the test will be skipped.\n";
    exit(EXIT_FAILURE);
  }

  // run the test on each device
  for (auto const& device : devices) {
    std::cout << "Test One Histo Container on " << aV2P::getName(device) << '\n';

    auto queue = Queue(device);

    go<int16_t>(host, device, queue);
    go<uint8_t, 128, 8, 4>(host, device, queue);
    go<uint16_t, 313 / 2, 9, 4>(host, device, queue);
  }

  return 0;
}
