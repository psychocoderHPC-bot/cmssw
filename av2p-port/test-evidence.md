# Hardware test evidence

aV2P pin: `psychocoderHPC-bot/alpaka` branch `av2p-final` at
`70bbdb18ad5480e9564d9f7122e4922800d974d2`. The checkout applies its bundled
native alpaka 3 queue-launch patch itself; this branch carries no patch files.

The installs used below are self-contained prefixes: `include/aV2P`,
`include/alpaka`, `lib/cmake/aV2P` and `lib/cmake/alpaka` in one root.
`aV2PConfig.cmake` exports `alpaka_DIR` for the bundled alpaka.

## CUDA: `rosi_v100`

Exclusive allocation, 4x Tesla V100-SXM2-32GB, `sm_70`. Toolchain: CUDA
12.6.85, g++ 13.3.0. aV2P prefix `/home/ubuntu/av2p/install-70b`, configured
with `-DaV2P_ACC_GPU_CUDA_ENABLE=ON -DCMAKE_CUDA_ARCHITECTURES=70
-DBUILD_TESTING=OFF`. CMSSW tree `<cmssw-tree>`, HEAD of `av2p-final` with the
`aV2P/zerodim` migration applied.

Audit (`check-av2p-migration.sh .`): exit 0,
`clean (0 findings, 11 allowlisted lines)`.

`check-tool-prefix.sh /home/ubuntu/av2p/install-70b cuda`: exit 0,
`OK ... aV2P/ and alpaka/ present, preprocess succeeded with
-DAV2P_ACC_GPU_CUDA_ENABLED`.

### T1: catch2 device kernel, default caching allocator, GPU 0

```
nvcc -x cu -std=c++20 -arch=sm_70 --expt-relaxed-constexpr --extended-lambda \
  -DAV2P_ACC_GPU_CUDA_ENABLED -I$AV2P_PREFIX/include -I<cmssw-tree> \
  <cmssw-tree>/HeterogeneousTest/AlpakaKernel/test/alpaka/testDeviceAdditionKernel.dev.cc \
  -o test_addkernel -L/usr/lib -lCatch2Main -lCatch2 -lhwloc -ltbb -lcudart -lpthread
CUDA_VISIBLE_DEVICES=0 ./test_addkernel
```

`COMPILE_EXIT=0` (0 errors), `RUN_EXIT=0`:

```
All tests passed (1048577 assertions in 1 test case)
```

No `-DAV2P_DISABLE_CACHING_ALLOCATOR` was set, so the caching allocator path
runs.

### T3: g++ host translation units

```
g++ -std=c++20 -c -DAV2P_ACC_GPU_CUDA_ENABLED -DAV2P_HOST_ONLY \
  -I$AV2P_PREFIX/include -I<cmssw-tree> -I/usr/local/cuda-12.6/include \
  <cmssw-tree>/HeterogeneousCore/AlpakaInterface/src/prefixScan.cc -o prefixScan.o
g++ -std=c++20 -c -DAV2P_ACC_GPU_CUDA_ENABLED -DAV2P_HOST_ONLY \
  -I$AV2P_PREFIX/include -I<cmssw-tree> -I/usr/local/cuda-12.6/include \
  <cmssw-tree>/HeterogeneousCore/AlpakaInterface/src/Backend.cc -o Backend.o
```

`EXIT=0`, 0 errors each. Objects: `prefixScan.o` (183360 B) and `Backend.o`
(21856 B), the same sizes as the previous validated run.

### T4: zero-dimensional bridge, GPU 0

```
nvcc -x cu -std=c++20 -arch=sm_70 --expt-relaxed-constexpr --extended-lambda \
  -DAV2P_ACC_GPU_CUDA_ENABLED -I$AV2P_PREFIX/include -I<cmssw-tree> \
  test0d.cc -o test0d -L/usr/lib -lhwloc -ltbb -lcudart -lpthread
CUDA_VISIBLE_DEVICES=0 ./test0d
```

`COMPILE_EXIT=0`, `RUN_EXIT=0`:

```
value=42 extentProduct=1
ZERODIM_BRIDGE_OK on Tesla V100-SXM2-32GB
```

The program exercises `make_device_buffer<int>(queue)`, `make_host_view(int&)`,
`getExtentProduct == 1`, 0-D `memcpy` host->device->host and a one-thread
kernel that increments the scalar.

## HIP: `hal`

One AMD Radeon RX 7900 XTX (`gfx1100`), 64-core AMD EPYC. ROCm 7.2.0
(HIP 7.2.26015, AMD clang 22), g++ 13.3.0. aV2P prefix
`/home/agent/av2p-hip/install-70b`, configured with
`-DaV2P_ACC_GPU_HIP_ENABLE=ON -DaV2P_ACC_GPU_HIP_ONLY_MODE=OFF
-DCMAKE_HIP_ARCHITECTURES=gfx1100 -DBUILD_TESTING=OFF`. CMSSW tree
`<cmssw-tree>` at the same HEAD as the CUDA run.

Audit: exit 0, `clean (0 findings, 11 allowlisted lines)`.

### T1: catch2 device kernel, default caching allocator, GPU 0

```
hipcc -x hip -std=c++20 --offload-arch=gfx1100 -O2 -DAV2P_ACC_GPU_HIP_ENABLED \
  -I$AV2P_PREFIX/include -I<cmssw-tree> \
  <cmssw-tree>/HeterogeneousTest/AlpakaKernel/test/alpaka/testDeviceAdditionKernel.dev.cc \
  -o test_addkernel -lCatch2Main -lCatch2 -lhwloc -lamdhip64 -lpthread
HIP_VISIBLE_DEVICES=0 ./test_addkernel
```

`COMPILE_EXIT=0` (0 errors), `RUN_EXIT=0`:

```
All tests passed (1048577 assertions in 1 test case)
```

`--expt-relaxed-constexpr` is not passed: AMD clang 22 rejects it.

### T4: zero-dimensional bridge, GPU 0

```
hipcc -x hip -std=c++20 --offload-arch=gfx1100 -O2 -DAV2P_ACC_GPU_HIP_ENABLED \
  -I$AV2P_PREFIX/include -I<cmssw-tree> test0d.cc -o test0d \
  -lhwloc -lamdhip64 -lpthread
HIP_VISIBLE_DEVICES=0 ./test0d
```

`COMPILE_EXIT=0`, `RUN_EXIT=0`:

```
value=42 extentProduct=1
ZERODIM_BRIDGE_OK on AMD Radeon RX 7900 XTX
```

## Package consumer, both targets

A fresh CMake project with a clean cache calls only
`find_package(aV2P CONFIG REQUIRED)` and puts the aV2P prefix on
`CMAKE_PREFIX_PATH`; `alpaka_DIR` is not set. The consumer includes
`aV2P/alpaka.hpp` and `aV2P/zerodim/ZeroDim.hpp`, allocates
`aV2P::zerodim::make_host_buffer<int>()`, and links `aV2P::aV2P` with
`aV2P_finalize()`.

CUDA target: configured with `CMAKE_CUDA_ARCHITECTURES=70`; the cache records
`alpaka_DIR=<prefix>/lib/cmake/alpaka`. HIP target: configured with the ROCm
prefix and `gfx1100`. Both build and run:

```
PKG_CONSUMER_OK product=1 value=42
```

## Tree integrity

The tests ran against the same CMSSW HEAD that is pushed. Only the
`aV2P/zerodim` migration differs from the previous published commit.
