#!/usr/bin/env bash
# Check the CMSSW tree for leftover alpaka 2.x spellings that collide with
# native alpaka 3. The aV2P compatibility layer owns the AV2P_ macro prefix and
# the aV2P:: namespace, so the native spellings (ALPAKA_, alpaka::, <alpaka/...)
# must not survive in C/C++.
#
# The check is context aware:
#   C/C++            ALPAKA_, alpaka::, <alpaka/, namespace alpaka,
#                    using alpaka::, using namespace alpaka
#   .py / .cfg       ALPAKA_ macros, except the kept CMSSW build/env names
#   shell / Makefile -DALPAKA_ / -UALPAKA_ compile flags
#   BuildFile.xml    <use name="aV2P"> and the renamed AV2P_BACKENDS flag
#
# The 0-D bridge may name native alpaka 3 types; those lines are printed for
# review instead of failing the check.
#
# Usage: check-av2p-migration.sh [--verbose] [ROOT]
# Exit status: 0 clean, 1 findings, 2 usage error.

set -euo pipefail

PROG=$(basename "$0")
VERBOSE=0
ROOT=""

usage() {
  cat <<EOF
Usage: $PROG [--verbose|-v] [--help|-h] [ROOT]

ROOT defaults to the repository root that contains this script.
Checks the CMSSW tree for alpaka 2.x spellings that collide with native
alpaka 3. Prints "file:line:text" grouped by rule and exits 0 only when clean.
EOF
}

while [ $# -gt 0 ]; do
  case "$1" in
    -v|--verbose) VERBOSE=1 ;;
    -h|--help) usage; exit 0 ;;
    --) shift; break ;;
    -*) echo "$PROG: unknown option '$1'" >&2; usage >&2; exit 2 ;;
    *) ROOT="$1" ;;
  esac
  shift
done
[ $# -gt 0 ] && ROOT="$1"

SCRIPT_PATH=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)/$(basename -- "${BASH_SOURCE[0]}")
if [ -z "$ROOT" ]; then
  ROOT=$(cd -- "$(dirname -- "$SCRIPT_PATH")/../../.." && pwd)
fi
if [ ! -d "$ROOT" ]; then
  echo "$PROG: no such directory: $ROOT" >&2
  exit 2
fi
ROOT=$(cd -- "$ROOT" && pwd)

SELF_REL=""
case "$SCRIPT_PATH" in
  "$ROOT"/*) SELF_REL=${SCRIPT_PATH#"$ROOT"/} ;;
esac

USE_GIT=0
if git -C "$ROOT" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  USE_GIT=1
fi

CXX_GLOBS=( '*.h' '*.hh' '*.hpp' '*.hxx' '*.ipp' '*.inl' '*.tpp'
            '*.c' '*.cc' '*.dev.cc' '*.cpp' '*.cxx' '*.cu' '*.cuh' '*.hip' )
PY_GLOBS=( '*.py' '*.cfg' )
SH_GLOBS=( '*.sh' '*.mk' 'Makefile' '*Makefile*' )
BUILD_GLOBS=( 'BuildFile.xml' '*/BuildFile.xml' )

FAIL=0
TOTAL_HITS=0
ALLOW_LINES=()
ALLOW_VERBOSE_LINES=()

declare -A HITS_BY_RULE=()
RULE_ORDER=()

note_allowed()      { ALLOW_LINES+=("$1"); }
note_allowed_verbose() { ALLOW_VERBOSE_LINES+=("$1"); }

add_hit() {
  local rule=$1 line=$2
  if [ -z "${HITS_BY_RULE[$rule]+x}" ]; then
    HITS_BY_RULE[$rule]=""
    RULE_ORDER+=("$rule")
  fi
  HITS_BY_RULE[$rule]+="$line"$'\n'
  TOTAL_HITS=$((TOTAL_HITS + 1))
}

# Emit "file:line:text" records for one pattern over a set of globs.
# $1 = pattern (extended regex), $2.. = globs
grep_all() {
  local pattern=$1
  shift
  if [ "$USE_GIT" -eq 1 ]; then
    git -C "$ROOT" grep -n -I -E -e "$pattern" -- "$@" 2>/dev/null || true
  else
    local inc=()
    local g
    for g in "$@"; do inc+=("--include=${g#\*/}"); done
    ( cd "$ROOT" && grep -R -n -I -E "${inc[@]}" -e "$pattern" . 2>/dev/null ) \
      | sed 's|^\./||' || true
  fi
}

# Count tracked/visible files for the verbose summary.
count_class() {
  if [ "$USE_GIT" -eq 1 ]; then
    git -C "$ROOT" ls-files -- "$@" 2>/dev/null | wc -l
  else
    local args=()
    local g sep=()
    for g in "$@"; do
      args+=("${sep[@]}" -name "${g#\*/}"); sep=( -o )
    done
    ( cd "$ROOT" && find . -type f \( "${args[@]}" \) 2>/dev/null ) | wc -l
  fi
}

# ---------------------------------------------------------------------------
# C/C++ rules.
cxx_rule() {
  local rule=$1 pattern=$2 allow_file=$3
  local rec f rest line text
  while IFS= read -r rec; do
    [ -n "$rec" ] || continue
    f=${rec%%:*}
    rest=${rec#*:}
    line=${rest%%:*}
    text=${rest#*:}
    if [ -n "$allow_file" ] && [ "$f" = "$allow_file" ]; then
      note_allowed "ALLOW  $f:$line:$text  (native alpaka 3 reference, review)"
      continue
    fi
    add_hit "$rule" "$f:$line:$text"
  done < <(grep_all "$pattern" "${CXX_GLOBS[@]}")
}

# The 0-D bridge may name native alpaka 3 types through the alpaka:: spelling,
# but an ALPAKA_ macro or an <alpaka/ include there is still a real bug.
cxx_rule "C/C++: ALPAKA_ macro"          'ALPAKA_'                     ""
cxx_rule "C/C++: alpaka:: namespace"     '\balpaka::'                  "HeterogeneousCore/AlpakaInterface/interface/ZeroDimCompat.h"
cxx_rule "C/C++: <alpaka/ include"       '<alpaka/|alpaka/alpaka\.hpp' ""
cxx_rule "C/C++: namespace alpaka"       'namespace alpaka([^A-Za-z0-9_]|$)' ""
cxx_rule "C/C++: using alpaka::"         'using alpaka::'              ""
cxx_rule "C/C++: using namespace alpaka" 'using namespace alpaka([^A-Za-z0-9_]|$)' ""

# ---------------------------------------------------------------------------
# .py / .cfg rules. The kept CMSSW names are masked before re-testing, so an
# allowed token on the same line cannot hide a real ALPAKA_ macro.
py_rule() {
  local rule=$1 rec f rest line text masked
  while IFS= read -r rec; do
    [ -n "$rec" ] || continue
    f=${rec%%:*}; rest=${rec#*:}; line=${rest%%:*}; text=${rest#*:}
    masked=$(printf '%s' "$text" | sed -E \
      -e 's/SCRAM_ALPAKA_BACKEND//g' \
      -e 's/_ALPAKA_SUFFIX//g' \
      -e 's/ALPAKA_BACKENDS//g' \
      -e 's/ALPAKA_BACKEND//g' \
      -e 's/ALPAKA_BASE//g')
    if printf '%s' "$masked" | grep -q 'ALPAKA_'; then
      add_hit "$rule" "$f:$line:$text"
    else
      note_allowed_verbose "ALLOW  $f:$line:$text  (kept CMSSW build/env name)"
    fi
  done < <(grep_all 'ALPAKA_' "${PY_GLOBS[@]}")
}

py_rule "Python/cfg: ALPAKA_ macro"

# ---------------------------------------------------------------------------
# shell / Makefile rules.
sh_rule() {
  local rule=$1 rec f rest line text
  while IFS= read -r rec; do
    [ -n "$rec" ] || continue
    f=${rec%%:*}; rest=${rec#*:}; line=${rest%%:*}; text=${rest#*:}
    add_hit "$rule" "$f:$line:$text"
  done < <(grep_all '\-DALPAKA_|\-UALPAKA_' "${SH_GLOBS[@]}")
}

sh_rule "shell/Makefile: -DALPAKA_ / -UALPAKA_ flag"

# ---------------------------------------------------------------------------
# BuildFile.xml rules: SCRAM tool stays "alpaka", backend flag stays
# ALPAKA_BACKENDS.
build_rule() {
  local rule=$1 pattern=$2 rec f rest line text
  while IFS= read -r rec; do
    [ -n "$rec" ] || continue
    f=${rec%%:*}; rest=${rec#*:}; line=${rest%%:*}; text=${rest#*:}
    add_hit "$rule" "$f:$line:$text"
  done < <(grep_all "$pattern" "${BUILD_GLOBS[@]}")
}

build_rule 'BuildFile.xml: aV2P SCRAM tool use'          '<use[[:space:]]+name="aV2P"'
build_rule 'BuildFile.xml: renamed AV2P_BACKENDS flag'   'AV2P_BACKENDS'

# Self-matches (the patterns above live in this script) are never findings.
for rule in "${RULE_ORDER[@]}"; do
  [ -n "$SELF_REL" ] || break
  HITS_BY_RULE[$rule]=$(printf '%s' "${HITS_BY_RULE[$rule]}" | grep -v -F "$SELF_REL:" || true)
done
TOTAL_HITS=0
for rule in "${RULE_ORDER[@]}"; do
  n=$(printf '%s' "${HITS_BY_RULE[$rule]}" | grep -c . || true)
  TOTAL_HITS=$((TOTAL_HITS + n))
done

# ---------------------------------------------------------------------------
# Report.
for rule in "${RULE_ORDER[@]}"; do
  [ -n "$(printf '%s' "${HITS_BY_RULE[$rule]}")" ] || continue
  printf '\n[%s]\n' "$rule"
  printf '%s' "${HITS_BY_RULE[$rule]}"
  FAIL=1
done

for l in "${ALLOW_LINES[@]}"; do printf '%s\n' "$l"; done
if [ "$VERBOSE" -eq 1 ]; then
  for l in "${ALLOW_VERBOSE_LINES[@]}"; do printf '%s\n' "$l"; done
fi

if [ "$VERBOSE" -eq 1 ]; then
  printf '\nscanned: %d C/C++, %d .py/.cfg, %d shell/Makefile, %d BuildFile.xml\n' \
    "$(count_class "${CXX_GLOBS[@]}")" \
    "$(count_class "${PY_GLOBS[@]}")" \
    "$(count_class "${SH_GLOBS[@]}")" \
    "$(count_class "${BUILD_GLOBS[@]}")"
fi

if [ "$FAIL" -eq 0 ]; then
  printf '\ncheck-av2p-migration: clean (%d findings, %d allowlisted lines)\n' \
    "$TOTAL_HITS" "$(( ${#ALLOW_LINES[@]} + ${#ALLOW_VERBOSE_LINES[@]} ))"
  exit 0
fi

printf '\ncheck-av2p-migration: %d finding(s) in %d rule group(s)\n' \
  "$TOTAL_HITS" "${#RULE_ORDER[@]}" >&2
exit 1
