# CMSSW on aV2P (native alpaka 3)

This branch ports CMSSW's Alpaka integration from alpaka 2.2 to aV2P, the
native alpaka 3 compatibility layer. It is based on upstream CMSSW commit
`663bd11374d286d32415adb260fb78ef186114b9` and is published as the branch
`av2p-final` in `psychocoderHPC-bot/cmssw`.

## What this port is

aV2P ships alpaka 3 with a 2.2-shaped adapter surface. CMSSW code keeps its
2.2 idioms (`Buf`, `Queue`, `Event`, `exec`, the `Acc` aliases) and links
against the aV2P headers instead of the old alpaka 2.2 tree. The external
dependency is pinned to aV2P commit `70bbdb18a` on top of `5c1ad3d`.

The adapter fixes this port needed (the 2.2 `Buf` deleter constructor, the
host-only CUDA/HIP runtime pass, the `AV2P_ACC_*_ONLY_MODE` function-attribute
fix and the native alpaka 3 host queue-launch guard) are part of aV2P from
`5c1ad3d` through `70bbdb18a`. The aV2P checkout applies its own bundled
native patch, so this branch carries no patch files and no hand patches.

The CMSSW side keeps two matching source changes: the host-only
`TaskKernelGpuUniformCudaHipRt` stubs are removed from `config.h`, and
`CachingAllocator.h` prints the native queue/event handles through
`aV2P::getNativeHandle`.

## Collision-only rename rule

The rename touches only spellings that collide with native alpaka 3:

| CMSSW 2.2 spelling | aV2P spelling |
|---|---|
| `ALPAKA_` macro prefix | `AV2P_` |
| `alpaka::` namespace | `aV2P::` |
| `<alpaka/alpaka.hpp>` | `<aV2P/alpaka.hpp>` |

CMSSW-owned names that do not collide stay unchanged:

- SCRAM/CMake build contract: `ALPAKA_BACKENDS`, `<use name="alpaka"/>`.
- CMSSW namespaces: `alpaka_common`, `alpaka_cuda_async`, `alpaka_rocm_async`,
  `alpaka_serial_sync`, `alpaka_tbb_async`, `cms::alpakatools`.
- Python and environment names: `SCRAM_ALPAKA_BACKEND`, `_ALPAKA_SUFFIX`,
  `ALPAKA_BACKEND`, `@alpaka`, `alpaka_cff.py`, `ProcessAcceleratorAlpaka`,
  `ModuleTypeResolverAlpaka`.

The audit script `HeterogeneousCore/AlpakaInterface/scripts/check-av2p-migration.sh`
enforces the rule. Run it from the top of the tree:

```
HeterogeneousCore/AlpakaInterface/scripts/check-av2p-migration.sh .
```

`audit-output.txt` in this directory is the output for this branch.

## Zero-dimensional support

aV2P `70bbdb18a` provides `aV2P/zerodim/ZeroDim.hpp` with `ScalarBuf`,
`ScalarView`, the 0-D `CreateTaskMemcpy`/`CreateTaskMemset` specialisations and
the aV2P view traits. `HeterogeneousCore/AlpakaInterface/interface/ZeroDimCompat.h`
is now a thin CMSSW layer: it includes that header, aliases the aV2P types into
`cms::alpakatools::zerodim`, and keeps the scalar factory functions that select
CMSSW's caching allocator policy. The downstream call sites in `memory.h` and
`CopyToDevice.h` are unchanged.

## SCRAM tool files

`scram/` holds five drop-in files: `alpaka.xml`, `alpaka-cuda.xml`,
`alpaka-rocm.xml`, `alpaka-serial.xml`, `alpaka-tbb.xml`.

The SCRAM tool identifier stays `alpaka` so `BuildFile.xml` files and the
`ALPAKA_BACKENDS` flag need no edits. Only the contents change: the backend
gates are `AV2P_ACC_*`, the common flags are `AV2P_*`, and the device passes
strip `AV2P_HOST_ONLY`. Rules for an operator:

1. This external is the only provider of the backend gate macros. Do not
   install a second external that injects `-DALPAKA_ACC_*`.
2. `<aV2P-prefix>/include` provides both `aV2P/` and native `alpaka/`. A split
   layout with the native tree in a second prefix still works if the tool file
   lists both include roots. Put the aV2P root first so that
   `<aV2P/alpaka.hpp>` resolves to aV2P.
3. `ALPAKA_BACKENDS` stays. It is a SCRAM/CMake build flag, not a C++ macro.

## Build layer: self-contained install

From `70bbdb18a` a standalone aV2P install is self-contained. One prefix holds

```
<prefix>/include/aV2P
<prefix>/include/alpaka
<prefix>/lib/cmake/aV2P
<prefix>/lib/cmake/alpaka
```

and `aV2PConfig.cmake` sets `alpaka_DIR` to `<prefix>/lib/cmake/alpaka` before
`find_dependency(alpaka)` when the consumer did not set it. A CMake consumer
that calls `find_package(aV2P CONFIG REQUIRED)` therefore needs only the aV2P
prefix on `CMAKE_PREFIX_PATH`. The installed target requires the usual native
finalize step: call `aV2P_finalize(<target>)` after `target_link_libraries`, or
create the target with `aV2P_add_executable()`.

Check a prefix with:

```
av2p-port/check-tool-prefix.sh "$AV2P_PREFIX" cuda                   # self-contained
av2p-port/check-tool-prefix.sh "$AV2P_PREFIX" cuda "$ALPAKA_PREFIX"  # split
# or: hip
```

Without the optional third argument the script requires both include roots in
`$AV2P_PREFIX`. With it, the script requires `aV2P/` in the first prefix and
`alpaka/` in the second, and preprocesses with both `-I` paths in the order
aV2P, alpaka. It uses `nvcc` for `cuda` and `hipcc` for `hip` by default;
override the compiler with `CXX`.

`-lhwloc` is required by the validated install because its vendored alpaka 3
was built with hwloc. If a package's external builds alpaka with hwloc off,
that package must drop `-lhwloc`. This follows the alpaka build of each
package, so decide it from that external's alpaka configuration instead of
copying the command below.

`AV2P_ACC_*_ONLY_MODE` is fixed in `70bbdb18a`: `AV2P_FN_ACC` stays
`__device__ __host__` in only mode, so the native alpaka 3 kernel functor is
still host-invocable. The validated builds use `AV2P_ACC_GPU_HIP_ONLY_MODE=OFF`
and never set the CUDA only mode.

`--expt-relaxed-constexpr` is an nvcc option. AMD clang 22 (ROCm 7.2) rejects
it as an unknown argument, so the ROCm tool file does not pass it. The CUDA
tool file keeps it on `CUDA_FLAGS` for the device pass only.

## Build and run

`$AV2P_PREFIX` is the aV2P prefix from the self-contained install. For a split
layout also set `$ALPAKA_PREFIX` and add its `include` root after the aV2P one.

The CUDA validation used CUDA 12.6.85, g++ 13.3.0 and a Tesla V100-SXM2-32GB
(`sm_70`). Real device test with the default caching allocator:

```
nvcc -x cu -std=c++20 -arch=sm_70 --expt-relaxed-constexpr --extended-lambda \
  -DAV2P_ACC_GPU_CUDA_ENABLED \
  -I$AV2P_PREFIX/include -I<cmssw-tree> \
  HeterogeneousTest/AlpakaKernel/test/alpaka/testDeviceAdditionKernel.dev.cc \
  -o test_addkernel -L/usr/lib -lCatch2Main -lCatch2 -lhwloc -ltbb -lcudart -lpthread
CUDA_VISIBLE_DEVICES=0 ./test_addkernel
```

The HIP validation used ROCm 7.2.0 (HIP 7.2.26015, AMD clang 22) and a Radeon
RX 7900 XTX (`gfx1100`). Real device test with the default caching allocator:

```
hipcc -x hip -std=c++20 --offload-arch=gfx1100 -O2 -DAV2P_ACC_GPU_HIP_ENABLED \
  -I$AV2P_PREFIX/include -I<cmssw-tree> \
  HeterogeneousTest/AlpakaKernel/test/alpaka/testDeviceAdditionKernel.dev.cc \
  -o test_addkernel -lCatch2Main -lCatch2 -lhwloc -lamdhip64 -lpthread
HIP_VISIBLE_DEVICES=0 ./test_addkernel
```

Host-only translation unit with g++:

```
g++ -std=c++20 -c -DAV2P_ACC_GPU_CUDA_ENABLED -DAV2P_HOST_ONLY \
  -I$AV2P_PREFIX/include -I<cmssw-tree> -I/usr/local/cuda-12.6/include \
  HeterogeneousCore/AlpakaInterface/src/prefixScan.cc -o prefixScan.o
```

A full SCRAM release build needs the SCRAM externals and is out of scope here.

## Test results

All hardware checks pass on the final commit with the self-contained
`install-70b` prefixes from aV2P `70bbdb18a`. See `test-evidence.md` for
commands and exact output lines.

CUDA on `rosi_v100`:

| check | result |
|---|---|
| catch2 device kernel, default allocator, GPU 0 | `All tests passed (1048577 assertions in 1 test case)` |
| g++ host compile of `prefixScan.cc` and `Backend.cc` | exit 0, 0 errors each |
| zero-dimensional bridge, GPU 0 | `ZERODIM_BRIDGE_OK on Tesla V100-SXM2-32GB` |
| package consumer, `find_package(aV2P CONFIG)` | `PKG_CONSUMER_OK` |

HIP on `hal`:

| check | result |
|---|---|
| catch2 device kernel, default allocator, GPU 0 | `All tests passed (1048577 assertions in 1 test case)` |
| zero-dimensional bridge, GPU 0 | `ZERODIM_BRIDGE_OK on AMD Radeon RX 7900 XTX` |
| package consumer, `find_package(aV2P CONFIG)` | `PKG_CONSUMER_OK` |
