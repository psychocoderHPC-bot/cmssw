# CMSSW on aV2P (native alpaka 3)

This branch ports CMSSW's Alpaka integration from alpaka 2.2 to aV2P, the
native alpaka 3 compatibility layer. It is based on upstream CMSSW commit
`663bd11374d286d32415adb260fb78ef186114b9` and is published as the branch
`av2p-final` in `psychocoderHPC-bot/cmssw`.

## What this port is

aV2P ships alpaka 3 with a 2.2-shaped adapter surface. CMSSW code keeps its
2.2 idioms (`Buf`, `Queue`, `Event`, `exec`, the `Acc` aliases) and links
against the aV2P headers instead of the old alpaka 2.2 tree. The external
dependency is pinned to aV2P commit `47148de` on top of `5c1ad3d`.

Two host-side defects in the 2.2 adapter had to be fixed for CMSSW's host
translation units and its caching allocator. Both fixes live in
`av2p-hostpass.patch` in this directory and are part of aV2P commit `47148de`:

- `Buf` overload resolution for the 2.2 `(device, ptr, deleter, extents,
  pitches)` constructor.
- A host-only CUDA/HIP pass: include the vendor runtime header early so that
  `PlatformCudaRt`, `DevCudaRt`, `QueueCudaRt*` and `EventCudaRt` construct and
  query on the host, while a kernel enqueue remains a compile-time error.

The CMSSW side keeps two matching changes: the host-only
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

## Required patches

1. `av2p-hostpass.patch`: applies to aV2P at `include/aV2P/` and fixes the
   `Buf` adapter and the host-only CUDA/HIP runtime path. It is already part of
   aV2P commit `47148de`.
2. `alpaka3-host-queue-launch.patch`: applies to native alpaka 3 at
   `include/alpaka/api/unifiedCudaHip/Queue.hpp`. It wraps the kernel launch in
   `__CUDACC__ || __CUDA__ || __HIP__` and turns a host-compiler enqueue into a
   single `static_assert`. The patched header must be installed in the aV2P
   prefix.

## SCRAM tool files

`scram/` holds five drop-in files: `alpaka.xml`, `alpaka-cuda.xml`,
`alpaka-rocm.xml`, `alpaka-serial.xml`, `alpaka-tbb.xml`.

The SCRAM tool identifier stays `alpaka` so `BuildFile.xml` files and the
`ALPAKA_BACKENDS` flag need no edits. Only the contents change: the backend
gates are `AV2P_ACC_*`, the common flags are `AV2P_*`, and the device passes
strip `AV2P_HOST_ONLY`. Rules for an operator:

1. This external is the only provider of the backend gate macros. Do not
   install a second external that injects `-DALPAKA_ACC_*`.
2. `$TOOL_BASE/include` must contain both `aV2P/` and native `alpaka/`, and
   must come first on the include path so that `<aV2P/alpaka.hpp>` resolves to
   aV2P.
3. `ALPAKA_BACKENDS` stays. It is a SCRAM/CMake build flag, not a C++ macro.

## Build layer: include roots, link flags and gates

The validated install is one include root. `$TOOL_BASE/include` must contain
both `aV2P/` and native `alpaka/`; the validated install ships alpaka 3.0.0 at
`eb677e8d`. A prefix with only `aV2P/` has no native alpaka, and a prefix with
only `alpaka/` has no adapter. Check a prefix with:

```
av2p-port/check-tool-prefix.sh "$AV2P_PREFIX" cuda   # or: hip
```

The script checks both include roots and both headers, then preprocesses a tiny
translation unit with `-DAV2P_ACC_GPU_<BACKEND>_ENABLED -I$AV2P_PREFIX/include`.
It uses `nvcc` for `cuda` and `hipcc` for `hip` by default; override the
compiler with `CXX`.

`-lhwloc` is required by the validated install because its vendored alpaka 3
was built with hwloc. If a package's external builds alpaka with hwloc off,
that package must drop `-lhwloc`. This follows the alpaka build of each
package, so decide it from that external's alpaka configuration instead of
copying the command below.

`AV2P_ACC_*_ONLY_MODE` must stay off. With `AV2P_ACC_GPU_HIP_ONLY_MODE`
defined, aV2P sets `AV2P_FN_ACC` to `__device__` only; alpaka 3's
`KernelBundle::operator()` guards the launch on `std::is_invocable_v` in the
HIP host pass, so the kernel launch fails to compile. This is an
aV2P/alpaka 3 incompatibility, reproduced on gfx1100. The SCRAM tool files in
`scram/` therefore do not pass `AV2P_ACC_*_ONLY_MODE`.

`aV2PConfig.cmake` does not export `alpaka_DIR`. A CMake consumer that calls
`find_package(aV2P CONFIG)` must also provide the native alpaka prefix, for
example on `CMAKE_PREFIX_PATH` or through `alpaka_DIR`. SCRAM consumers use the
tool file and are not affected.

## Build and run

The validation below used CUDA 12.6.85, g++ 13.3.0, a Tesla V100-SXM2-32GB
(`sm_70`) and an aV2P install prefix at `$AV2P_PREFIX`. The commands assume the
two include roots and the per-package `-lhwloc` decision from the previous
section.

Real device test with the default caching allocator:

```
nvcc -x cu -std=c++20 -arch=sm_70 --expt-relaxed-constexpr --extended-lambda \
  -DAV2P_ACC_GPU_CUDA_ENABLED \
  -I$AV2P_PREFIX/include -I<cmssw-tree> \
  HeterogeneousTest/AlpakaKernel/test/alpaka/testDeviceAdditionKernel.dev.cc \
  -o test_addkernel -L/usr/lib -lCatch2Main -lCatch2 -lhwloc -ltbb -lcudart -lpthread
CUDA_VISIBLE_DEVICES=0 ./test_addkernel
```

Host-only translation unit with g++:

```
g++ -std=c++20 -c -DAV2P_ACC_GPU_CUDA_ENABLED -DAV2P_HOST_ONLY \
  -I$AV2P_PREFIX/include -I<cmssw-tree> -I/usr/local/cuda-12.6/include \
  HeterogeneousCore/AlpakaInterface/src/prefixScan.cc -o prefixScan.o
```

A full SCRAM release build needs the SCRAM externals and is out of scope here.

## Test results

All four hardware checks pass on the final commit. See `test-evidence.md` for
commands and exact output lines.

| check | result |
|---|---|
| catch2 device kernel, default allocator, GPU 0 | `All tests passed (1048577 assertions in 1 test case)` |
| plugin translation unit plus harness, GPU 0 | `PLUGIN_WRAPPER_OK 1024 elements on Tesla V100-SXM2-32GB` |
| g++ host-only compile of `prefixScan.cc` and `Backend.cc` | exit 0, 0 errors each |
| zero-dimensional bridge, GPU 0 | `ZERODIM_BRIDGE_OK on Tesla V100-SXM2-32GB` |
