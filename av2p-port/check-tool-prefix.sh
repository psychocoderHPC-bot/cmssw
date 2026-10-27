#!/usr/bin/env bash
# Verify that an aV2P tool prefix is self-consistent for a SCRAM-style build:
# it must provide both the aV2P headers and the native alpaka 3 headers, and a
# tiny translation unit must preprocess with the backend gate enabled.
#
# Usage: check-tool-prefix.sh <prefix> <cuda|hip> [alpaka-prefix]
# Exit status: 0 ok, 1 check failed, 2 usage error.
#
# Without <alpaka-prefix> the aV2P prefix is a merged root and must contain
# both aV2P/ and alpaka/. With <alpaka-prefix> the layout is split: aV2P/ must
# be in <prefix>, alpaka/ in <alpaka-prefix>, and preprocessing uses both -I
# paths in the order aV2P, alpaka.
#
# Environment: CXX selects the compiler (default nvcc for cuda, hipcc for hip)
# and CPPFLAGS adds operator-specific include paths. The backend gate is only
# accepted by a compiler that supports the backend, so use the backend compiler.

set -euo pipefail

PROG=$(basename "$0")

usage() {
  cat <<EOF
Usage: $PROG <prefix> <cuda|hip> [alpaka-prefix]

Checks that <prefix>/include contains aV2P/ and, for the merged layout, alpaka/.
With [alpaka-prefix], checks aV2P/ in <prefix> and alpaka/ in <alpaka-prefix>
instead. Then preprocesses a tiny translation unit with
-DAV2P_ACC_GPU_<BACKEND>_ENABLED and the include roots in the order aV2P, alpaka.
EOF
}

if [ $# -ne 2 ] && [ $# -ne 3 ]; then
  usage >&2
  exit 2
fi

PREFIX=${1%/}
BACKEND=$2
if [ $# -eq 3 ]; then
  ALPAKA_PREFIX=${3%/}
  SPLIT=1
else
  ALPAKA_PREFIX=$PREFIX
  SPLIT=0
fi

case "$BACKEND" in
  cuda) DEF=AV2P_ACC_GPU_CUDA_ENABLED; XLANG=cu; DEFAULT_CXX=nvcc ;;
  hip)  DEF=AV2P_ACC_GPU_HIP_ENABLED; XLANG=hip; DEFAULT_CXX=hipcc ;;
  *) echo "$PROG: unknown backend '$BACKEND' (expected cuda or hip)" >&2; exit 2 ;;
esac

fail=0
for d in "$PREFIX/include" "$ALPAKA_PREFIX/include"; do
  if [ ! -d "$d" ]; then
    echo "$PROG: FAIL missing include root: $d" >&2
    fail=1
  fi
done
if [ "$fail" -ne 0 ]; then
  exit 1
fi

if [ "$SPLIT" -eq 1 ]; then
  # Split layout: each root provides only its own tree.
  if [ ! -d "$PREFIX/include/aV2P" ]; then
    echo "$PROG: FAIL missing include root: $PREFIX/include/aV2P" >&2
    fail=1
  fi
  if [ ! -d "$ALPAKA_PREFIX/include/alpaka" ]; then
    echo "$PROG: FAIL missing include root: $ALPAKA_PREFIX/include/alpaka" >&2
    fail=1
  fi
  if [ ! -f "$PREFIX/include/aV2P/alpaka.hpp" ]; then
    echo "$PROG: FAIL missing header: $PREFIX/include/aV2P/alpaka.hpp" >&2
    fail=1
  fi
  if [ ! -f "$ALPAKA_PREFIX/include/alpaka/alpaka.hpp" ]; then
    echo "$PROG: FAIL missing header: $ALPAKA_PREFIX/include/alpaka/alpaka.hpp" >&2
    fail=1
  fi
else
  # Merged layout: one root carries both trees.
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
fi
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
if ! "$CXX" -std=c++20 -E -x "$XLANG" "-D$DEF" \
    -I"$PREFIX/include" -I"$ALPAKA_PREFIX/include" ${CPPFLAGS:-} \
    -o /dev/null "$tmp/probe.cc"; then
  echo "$PROG: FAIL preprocess with -D$DEF -I$PREFIX/include -I$ALPAKA_PREFIX/include" >&2
  echo "$PROG: hint: use the backend compiler (CXX=nvcc or CXX=hipcc) and set CPPFLAGS for its runtime include path" >&2
  exit 1
fi

if [ "$SPLIT" -eq 1 ]; then
  echo "$PROG: OK $PREFIX + $ALPAKA_PREFIX ($BACKEND): aV2P/ in the first root, alpaka/ in the second, preprocess succeeded with -D$DEF (aV2P first)"
else
  echo "$PROG: OK $PREFIX ($BACKEND): aV2P/ and alpaka/ present, preprocess succeeded with -D$DEF"
fi
