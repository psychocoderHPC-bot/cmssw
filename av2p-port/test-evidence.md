# Hardware test evidence

Target: `rosi_v100`, exclusive allocation, 4x Tesla V100-SXM2-32GB, `sm_70`.
Toolchain: CUDA 12.6.85, g++ 13.3.0, aV2P install prefix with the two patches
from this directory applied. All commands ran on the target. CMSSW tree is
`<cmssw-tree>`; aV2P prefix is `$AV2P_PREFIX`.

## TEST 1: catch2 device kernel, default caching allocator, GPU 0

```
nvcc -x cu -std=c++20 -arch=sm_70 --expt-relaxed-constexpr --extended-lambda \
  -DAV2P_ACC_GPU_CUDA_ENABLED \
  -I$AV2P_PREFIX/include -I<cmssw-tree> \
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

## TEST 2: plugin translation unit and harness, GPU 0

```
nvcc -x cu -std=c++20 -arch=sm_70 --expt-relaxed-constexpr --extended-lambda \
  -DAV2P_ACC_GPU_CUDA_ENABLED \
  -I$AV2P_PREFIX/include -I<cmssw-tree> \
  main_plugin.cc <cmssw-tree>/plugins/alpaka/AlpakaTestKernelAdditionAlgo.dev.cc \
  -o test_plugin -L/usr/lib -lhwloc -ltbb -lcudart -lpthread
CUDA_VISIBLE_DEVICES=0 ./test_plugin
```

`COMPILE_EXIT=0`, `RUN_EXIT=0`:

```
PLUGIN_WRAPPER_OK 1024 elements on Tesla V100-SXM2-32GB
```

## TEST 3: g++ host-only translation units

```
g++ -std=c++20 -c -DAV2P_ACC_GPU_CUDA_ENABLED -DAV2P_HOST_ONLY \
  -I$AV2P_PREFIX/include -I<cmssw-tree> -I/usr/local/cuda-12.6/include \
  <cmssw-tree>/HeterogeneousCore/AlpakaInterface/src/prefixScan.cc -o prefixScan.o
g++ -std=c++20 -c -DAV2P_ACC_GPU_CUDA_ENABLED -DAV2P_HOST_ONLY \
  -I$AV2P_PREFIX/include -I<cmssw-tree> -I/usr/local/cuda-12.6/include \
  <cmssw-tree>/HeterogeneousCore/AlpakaInterface/src/Backend.cc -o Backend.o
```

`EXIT=0`, 0 errors each. Objects produced: `prefixScan.o` (183360 B) and
`Backend.o` (21856 B).

Host capability probe, g++ and GPU 0:

```
devcount=1
name=Tesla V100-SXM2-32GB native=0 mem=34072559616
queue native=0x... empty=1
event native=0x...
HOST_API_OK
```

Negative probe, kernel enqueue in a g++ translation unit, exactly one
diagnostic:

```
alpaka/api/unifiedCudaHip/Queue.hpp:384:22: error: static assertion failed:
alpaka3: enqueuing a CUDA/HIP kernel requires a CUDA/HIP compiler. Move this code into a
.dev.cc / .cu translation unit.
```

## TEST 4: zero-dimensional bridge, GPU 0

```
nvcc -x cu -std=c++20 -arch=sm_70 --expt-relaxed-constexpr --extended-lambda \
  -DAV2P_ACC_GPU_CUDA_ENABLED \
  -I$AV2P_PREFIX/include -I<cmssw-tree> \
  test0d.cc -o test0d -L/usr/lib -lhwloc -ltbb -lcudart -lpthread
CUDA_VISIBLE_DEVICES=0 ./test0d
```

`COMPILE_EXIT=0`, `RUN_EXIT=0`:

```
value=42 extentProduct=1
ZERODIM_BRIDGE_OK on Tesla V100-SXM2-32GB
```

## Tree integrity

A sha256 comparison of the tested tree against a fresh `git archive` of the
final commit found no unexpected differences beyond the intended files, so the
tested content matches the published branch.
