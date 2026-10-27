#!/usr/bin/env bash
# Verify that an aV2P tool prefix is self-consistent for a SCRAM-style build:
# it must provide both the aV2P headers and the native alpaka 3 headers, and a
# tiny translation unit must preprocess with the backend gate enabled.
#
# Usage: check-tool-prefix.sh <prefix> <cuda|hip>
# Exit status: 0 ok, 1 check failed, 2 usage error.
#
# Environment: CXX selects the compiler (default nvcc for cuda, hipcc for hip)
# and CPPFLAGS adds operator-specific include paths. The backend gate is only
# accepted by a compiler that supports the backend, so use the backend compiler.

set -euo pipefail

PROG=$(basename "$0")

usage() {
  cat <<EOF
Usage: $PROG <prefix> <cuda|hip>

Checks that <prefix>/include contains both aV2P/ and alpaka/, then preprocesses
a tiny translation unit with -DAV2P_ACC_GPU_<BACKEND>_ENABLED -I<prefix>/include.
EOF
}

if [ $# -ne 2 ]; then
  usage >&2
  exit 2
fi

PREFIX=${1%/}
BACKEND=$2

case "$BACKEND" in
  cuda) DEF=AV2P_ACC_GPU_CUDA_ENABLED; XLANG=cu; DEFAULT_CXX=nvcc ;;
  hip)  DEF=AV2P_ACC_GPU_HIP_ENABLED; XLANG=hip; DEFAULT_CXX=hipcc ;;
  *) echo "$PROG: unknown backend '$BACKEND' (expected cuda or hip)" >&2; exit 2 ;;
esac

if [ ! -d "$PREFIX/include" ]; then
  echo "$PROG: FAIL missing include root: $PREFIX/include" >&2
  exit 1
fi

fail=0
for root in aV2P alpaka; do
  if [ ! -d "$PREFIX/include/$root" ]; then
    echo "$PROG: FAIL missing include root: $PREFIX/include/$root" >&2
    fail=1
  fi
done
for header in aV2P/alpaka.hpp alpaka/alpaka.hpp; do
  if [ ! -f "$PREFIX/include/$header" ]; then
    echo "$PROG: FAIL missing header: $PREFIX/include/$header" >&2
    fail=1
  fi
done
if [ "$fail" -ne 0 ]; then
  exit 1
fi

tmp=$(mktemp -d "${TMPDIR:-/tmp}/av2p-tool-prefix.XXXXXX")
trap 'rm -rf "$tmp"' EXIT

cat > "$tmp/probe.cc" <<'EOF'
#include <alpaka/alpaka.hpp>
#include <aV2P/alpaka.hpp>
int main() { return 0; }
EOF

CXX=${CXX:-$DEFAULT_CXX}
# shellcheck disable=SC2086
if ! "$CXX" -std=c++20 -E -x "$XLANG" "-D$DEF" -I"$PREFIX/include" ${CPPFLAGS:-} \
    -o /dev/null "$tmp/probe.cc"; then
  echo "$PROG: FAIL preprocess with -D$DEF -I$PREFIX/include" >&2
  echo "$PROG: hint: use the backend compiler (CXX=nvcc or CXX=hipcc) and set CPPFLAGS for its runtime include path" >&2
  exit 1
fi

echo "$PROG: OK $PREFIX ($BACKEND): aV2P/ and alpaka/ present, preprocess succeeded with -D$DEF"

echo "$PROG: OK $PREFIX ($BACKEND): aV2P/ and alpaka/ present, preprocess succeeded with -D$DEF"
