# Hardware test evidence

CUDA target: `rosi_v100`, exclusive allocation, 4x Tesla V100-SXM2-32GB, `sm_70`.
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

## HIP validation (hal)

Target `hal`, one AMD Radeon RX 7900 XTX (`gfx1100`), 64-core AMD EPYC. The
layout is split: aV2P in `/home/agent/av2p-hip/install`, native alpaka 3 in
`/home/agent/av2p-hip/install-alpaka`. Both include roots are required and are
passed in the order aV2P, alpaka. `$HIP_INC` below expands to both:

```
-I/home/agent/av2p-hip/install/include -I/home/agent/av2p-hip/install-alpaka/include
```

| item | value |
|---|---|
| ROCm | 7.2.0 |
| HIP | 7.2.26015 |
| device compiler | AMD clang 22 (roc-7.2.0) |
| GPU | AMD Radeon RX 7900 XTX, `gfx1100` |
| host compiler | g++ 13.3.0 |
| CMSSW tree | `/home/agent/cmssw-hip` |

Results:

| check | result |
|---|---|
| T1 catch2 device kernel, default allocator, GPU 0 | `All tests passed (1048577 assertions in 1 test case)` |
| T2 plugin translation unit plus harness, GPU 0 | `PLUGIN_WRAPPER_OK 1024 elements on AMD Radeon RX 7900 XTX` |
| T3 g++ host-only objects `prefixScan.cc`, `Backend.cc` | both exit 0, 0 errors |
| T4 zero-dimensional bridge, GPU 0 | `ZERODIM_BRIDGE_OK on AMD Radeon RX 7900 XTX` |
| negative host-only launch | exit 1, exactly one `static_assert` |

### T1: catch2 device kernel

```
hipcc -x hip -std=c++20 --offload-arch=gfx1100 -O2 -DAV2P_ACC_GPU_HIP_ENABLED \
  $HIP_INC -I/home/agent/cmssw-hip \
  /home/agent/cmssw-hip/HeterogeneousTest/AlpakaKernel/test/alpaka/testDeviceAdditionKernel.dev.cc \
  -o test_addkernel -lCatch2Main -lCatch2 -lhwloc -lamdhip64 -lpthread
HIP_VISIBLE_DEVICES=0 ./test_addkernel
```

`COMPILE_EXIT=0` (0 errors), `RUN_EXIT=0`:

```
Randomness seeded to: 1726010235
===============================================================================
All tests passed (1048577 assertions in 1 test case)
```

The first attempt added `--expt-relaxed-constexpr` and failed before parsing
code:

```
clang++: error: unknown argument: '--expt-relaxed-constexpr'
```

AMD clang 22 removed that nvcc-only option. Without it the test compiles and
passes; the ROCm SCRAM tool file does not pass it.

### T2: plugin translation unit and harness

```
hipcc -x hip -std=c++20 --offload-arch=gfx1100 -O2 -DAV2P_ACC_GPU_HIP_ENABLED \
  $HIP_INC -I/home/agent/cmssw-hip \
  -I/home/agent/cmssw-hip/HeterogeneousTest/AlpakaKernel/plugins/alpaka \
  t2_harness.cpp AlpakaTestKernelAdditionAlgo.dev.cc \
  -o test_plugin -lhwloc -lamdhip64 -lpthread
HIP_VISIBLE_DEVICES=0 ./test_plugin
```

`COMPILE_EXIT=0`, `RUN_EXIT=0`:

```
PLUGIN_WRAPPER_OK 1024 elements on AMD Radeon RX 7900 XTX
```

### T3: g++ host-only translation units

```
g++ -std=c++20 -O2 -c -DAV2P_ACC_GPU_HIP_ENABLED -DAV2P_HOST_ONLY \
  $HIP_INC -I/home/agent/cmssw-hip -I/opt/rocm/include \
  HeterogeneousCore/AlpakaInterface/src/prefixScan.cc -o prefixScan.o
g++ -std=c++20 -O2 -c -DAV2P_ACC_GPU_HIP_ENABLED -DAV2P_HOST_ONLY \
  $HIP_INC -I/home/agent/cmssw-hip -I/opt/rocm/include \
  HeterogeneousCore/AlpakaInterface/src/Backend.cc -o Backend.o
```

Both `EXIT=0`, 0 errors. The shipped `HostRuntimeApi.hpp` has a HIP branch
(`AV2P_HOST_HIP_RT_API`) that defines `__HIP_PLATFORM_AMD__`, includes
`<hip/hip_runtime.h>`, sets `ALPAKA_LANG_HIP` and declares the HIP grid
builtins and intrinsics the host compiler needs for the alpaka 3 headers.

### T4: zero-dimensional bridge

```
ZERODIM_BRIDGE_OK on AMD Radeon RX 7900 XTX
```

`COMPILE_EXIT=0`, `RUN_EXIT=0`. The test exercises `make_device_buffer<int>(queue)`,
`make_host_view(int&)`, `getExtentProduct == 1`, 0-D `memcpy`
host->device->host, 0-D `memset`, and a one-thread kernel that increments the
scalar through `dBuf.data()`.

### Negative: host-only launch must not compile

`neg_hostonly_launch.cpp` is compiled with g++ and the host-only defines, then
calls `aV2P::exec<Acc1D>`:

```
g++ -std=c++20 -O2 -c -DAV2P_ACC_GPU_HIP_ENABLED -DAV2P_HOST_ONLY \
  $HIP_INC -I/home/agent/cmssw-hip -I/opt/rocm/include \
  neg_hostonly_launch.cpp -o /dev/null
```

`NEG_EXIT=1`, exactly one diagnostic:

```
/home/agent/av2p-hip/install/include/aV2P/adapter/Kernel.hpp:187:42:
  error: static assertion failed: aV2P: enqueuing a CUDA/HIP kernel requires a CUDA/HIP compiler.
  Move this code into a .dev.cc / .cu translation unit.
```

### Layout note

The aV2P install does not ship native alpaka 3 under `install/include/alpaka`;
that tree is a separate prefix at `install-alpaka/include/alpaka`. Both roots
are required for every compile above. This is a packaging difference, not a
code defect.

## Tree integrity

A sha256 comparison of the tested tree against a fresh `git archive` of the
final commit found no unexpected differences beyond the intended files, so the
tested content matches the published branch.
