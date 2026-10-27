#!/usr/bin/env bash
# Check the CMSSW tree for leftover alpaka 2.x spellings that collide with
# native alpaka 3. The aV2P compatibility layer owns the AV2P_ macro prefix and
# the aV2P:: namespace, so the native spellings (ALPAKA_, alpaka::, <alpaka/...)
# must not survive in C/C++ or in generated/C++-emitting notebooks.
#
# The check is context aware:
#   C/C++            ALPAKA_, alpaka::, <alpaka/, namespace alpaka,
#                    using alpaka::, using namespace alpaka
#   .ipynb           ALPAKA_, alpaka::, <alpaka/, alpaka/alpaka.hpp
#   .py / .cfg       ALPAKA_ macros, except the kept CMSSW build/env names
#   shell / Makefile -DALPAKA_ / -UALPAKA_ compile flags
#   BuildFile.xml    <use name="aV2P"> and the renamed AV2P_BACKENDS flag
#
# Untracked files are scanned too when the tree is a git work tree and the git
# binary supports "git grep --untracked" (Git >= 2.18); otherwise a recursive
# grep fallback already covers untracked files.
#
# The kept CMSSW build-contract names are matched as exact tokens, and only
# outside C/C++ files: ALPAKA_BACKENDS, ALPAKA_BASE, ALPAKA_BACKEND,
# _ALPAKA_SUFFIX, SCRAM_ALPAKA_BACKEND.
#
# The 0-D bridge may name native alpaka 3 types on comment lines and in
# "#include <aV2P/..." lines; a real alpaka:: expression there still fails.
#
# Usage: check-av2p-migration.sh [--verbose] [--self-test] [ROOT]
# Exit status: 0 clean, 1 findings, 2 usage error.

set -euo pipefail

PROG=$(basename "$0")
VERBOSE=0
SELF_TEST=0
ROOT=""

usage() {
  cat <<EOF
Usage: $PROG [--verbose|-v] [--self-test] [--help|-h] [ROOT]

ROOT defaults to the repository root that contains this script.
Checks the CMSSW tree for alpaka 2.x spellings that collide with native
alpaka 3. Prints "file:line:text" grouped by rule and exits 0 only when clean.

  --self-test   build temporary repositories and verify that tracked and
                untracked hits are both caught and that a clean tree passes.
EOF
}

while [ $# -gt 0 ]; do
  case "$1" in
    -v|--verbose) VERBOSE=1 ;;
    --self-test) SELF_TEST=1 ;;
    -h|--help) usage; exit 0 ;;
    --) shift; break ;;
    -*) echo "$PROG: unknown option '$1'" >&2; usage >&2; exit 2 ;;
    *) ROOT="$1" ;;
  esac
  shift
done
[ $# -gt 0 ] && ROOT="$1"

SCRIPT_PATH=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)/$(basename -- "${BASH_SOURCE[0]}")

# ---------------------------------------------------------------------------
# Self-test: build temp repositories and exercise the real scan paths.
self_test() {
  local tmp repo clean out rc fail=0
  tmp=$(mktemp -d "${TMPDIR:-/tmp}/av2p-audit-selftest.XXXXXX")
  repo="$tmp/repo"
  clean="$tmp/clean"
  mkdir -p "$repo" "$clean"

  # Forbidden hit that git tracks.
  git -C "$repo" init -q
  git -C "$repo" config user.email selftest@example.invalid
  git -C "$repo" config user.name selftest
  printf 'namespace x { void f() { alpaka::foo(); } }\n' > "$repo/tracked_bad.cc"
  printf 'int clean = 0;\n' > "$repo/clean.cc"
  git -C "$repo" add tracked_bad.cc clean.cc
  git -C "$repo" commit -q -m init

  # Forbidden hit that git does not track.
  printf 'namespace y { void g() { alpaka::bar(); } }\n' > "$repo/untracked_bad.cc"

  # Clean repository for the pass case.
  git -C "$clean" init -q
  git -C "$clean" config user.email selftest@example.invalid
  git -C "$clean" config user.name selftest
  printf 'int clean = 0;\n' > "$clean/clean.cc"
  git -C "$clean" add clean.cc
  git -C "$clean" commit -q -m init

  rc=0
  out=$(bash "$SCRIPT_PATH" "$repo" 2>&1) || rc=$?
  if [ "$rc" -eq 0 ]; then
    echo "self-test: FAIL tracked/untracked hits were not rejected" >&2
    fail=1
  fi
  if ! printf '%s' "$out" | grep -q 'tracked_bad\.cc:'; then
    echo "self-test: FAIL tracked forbidden hit not reported" >&2
    fail=1
  fi
  if ! printf '%s' "$out" | grep -q 'untracked_bad\.cc:'; then
    echo "self-test: FAIL untracked forbidden hit not reported" >&2
    fail=1
  fi

  rc=0
  out=$(bash "$SCRIPT_PATH" "$clean" 2>&1) || rc=$?
  if [ "$rc" -ne 0 ]; then
    echo "self-test: FAIL clean tree rejected" >&2
    printf '%s\n' "$out" >&2
    fail=1
  fi

  rm -rf "$tmp"
  if [ "$fail" -ne 0 ]; then
    echo "self-test: FAIL" >&2
    return 1
  fi
  echo "self-test: PASS (tracked hit caught, untracked hit caught, clean tree accepted)"
  return 0
}

if [ "$SELF_TEST" -eq 1 ]; then
  self_test
  exit $?
fi

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
GIT_GREP_UNTRACKED=0
if git -C "$ROOT" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  USE_GIT=1
  # "git grep --untracked" needs Git >= 2.18. A miss exits 1, an unknown option
  # exits 129/128, so probe the option and treat "no match" as supported.
  probe_rc=0
  git -C "$ROOT" grep --untracked -e 'aV2P-untracked-probe' -- . >/dev/null 2>&1 || probe_rc=$?
  if [ "$probe_rc" -eq 0 ] || [ "$probe_rc" -eq 1 ]; then
    GIT_GREP_UNTRACKED=1
  fi
fi

CXX_GLOBS=( '*.h' '*.hh' '*.hpp' '*.hxx' '*.ipp' '*.inl' '*.tpp'
            '*.c' '*.cc' '*.dev.cc' '*.cpp' '*.cxx' '*.cu' '*.cuh' '*.hip' )
IPY_GLOBS=( '*.ipynb' )
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
    local git_opts=( -n -I -E )
    if [ "$GIT_GREP_UNTRACKED" -eq 1 ]; then
      git_opts+=( --untracked )
    fi
    git -C "$ROOT" grep "${git_opts[@]}" -e "$pattern" -- "$@" 2>/dev/null || true
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
    if [ "$GIT_GREP_UNTRACKED" -eq 1 ]; then
      git -C "$ROOT" ls-files --cached --others --exclude-standard -- "$@" 2>/dev/null | wc -l
    else
      git -C "$ROOT" ls-files -- "$@" 2>/dev/null | wc -l
    fi
  else
    local args=()
    local g sep=()
    for g in "$@"; do
      args+=("${sep[@]}" -name "${g#\*/}"); sep=( -o )
    done
    ( cd "$ROOT" && find . -type f \( "${args[@]}" \) 2>/dev/null ) | wc -l
  fi
}

# The kept CMSSW build-contract names are exact tokens, never prefixes.
BUILD_CONTRACT_TOKENS=" ALPAKA_BACKENDS ALPAKA_BASE ALPAKA_BACKEND _ALPAKA_SUFFIX SCRAM_ALPAKA_BACKEND "

alpaka_tokens() {
  printf '%s' "$1" | grep -oE '[A-Za-z0-9_]*ALPAKA_[A-Za-z0-9_]*' || true
}

# True only when every ALPAKA_ token on the line is a known build-contract name.
only_build_contract() {
  local text=$1 tok found=0
  while IFS= read -r tok; do
    [ -n "$tok" ] || continue
    found=1
    case "$BUILD_CONTRACT_TOKENS" in
      *" $tok "*) ;;
      *) return 1 ;;
    esac
  done < <(alpaka_tokens "$text")
  [ "$found" -eq 1 ]
}

# ---------------------------------------------------------------------------
# C/C++ rules.
ZD_ALLOW="HeterogeneousCore/AlpakaInterface/interface/ZeroDimCompat.h"

# Comment-only lines ("//", "*", "/*" after optional whitespace) and
# "#include <aV2P/..." may name native alpaka; real code may not.
is_zd_allow_line() {
  printf '%s' "$1" | grep -qE '^[[:space:]]*(//|\*|/\*|#include[[:space:]]*[<"]aV2P/)'
}

cxx_rule() {
  local rule=$1 pattern=$2 allow_file=$3
  local rec f rest line text
  while IFS= read -r rec; do
    [ -n "$rec" ] || continue
    f=${rec%%:*}
    rest=${rec#*:}
    line=${rest%%:*}
    text=${rest#*:}
    if [ -n "$allow_file" ] && [ "$f" = "$allow_file" ] && is_zd_allow_line "$text"; then
      note_allowed "ALLOW  $f:$line:$text  (native alpaka 3 comment/include, review)"
      continue
    fi
    add_hit "$rule" "$f:$line:$text"
  done < <(grep_all "$pattern" "${CXX_GLOBS[@]}")
}

cxx_rule "C/C++: ALPAKA_ macro"          'ALPAKA_'                     "$ZD_ALLOW"
cxx_rule "C/C++: alpaka:: namespace"     '\balpaka::'                  "$ZD_ALLOW"
cxx_rule "C/C++: <alpaka/ include"       '<alpaka/|alpaka/alpaka\.hpp' "$ZD_ALLOW"
cxx_rule "C/C++: namespace alpaka"       'namespace alpaka([^A-Za-z0-9_]|$)' "$ZD_ALLOW"
cxx_rule "C/C++: using alpaka::"         'using alpaka::'              "$ZD_ALLOW"
cxx_rule "C/C++: using namespace alpaka" 'using namespace alpaka([^A-Za-z0-9_]|$)' "$ZD_ALLOW"

# ---------------------------------------------------------------------------
# .ipynb rules: the DNN notebooks emit C++ snippets, so they scan like C++.
ipynb_rule() {
  local rule=$1 rec f rest line text
  while IFS= read -r rec; do
    [ -n "$rec" ] || continue
    f=${rec%%:*}; rest=${rec#*:}; line=${rest%%:*}; text=${rest#*:}
    if printf '%s' "$text" | grep -qE 'alpaka::|<alpaka/|alpaka/alpaka\.hpp'; then
      add_hit "$rule" "$f:$line:$text"
    elif only_build_contract "$text"; then
      note_allowed_verbose "ALLOW  $f:$line:$text  (kept CMSSW build/env name)"
    else
      add_hit "$rule" "$f:$line:$text"
    fi
  done < <(grep_all 'ALPAKA_|alpaka::|<alpaka/|alpaka/alpaka\.hpp' "${IPY_GLOBS[@]}")
}

ipynb_rule "Notebook: alpaka 2.x spelling"

# ---------------------------------------------------------------------------
# .py / .cfg rules. Only exact build-contract tokens are allowed, so a prefix
# such as ALPAKA_BACKENDS_EXTRA cannot hide behind the allowlist.
py_rule() {
  local rule=$1 rec f rest line text
  while IFS= read -r rec; do
    [ -n "$rec" ] || continue
    f=${rec%%:*}; rest=${rec#*:}; line=${rest%%:*}; text=${rest#*:}
    if only_build_contract "$text"; then
      note_allowed_verbose "ALLOW  $f:$line:$text  (kept CMSSW build/env name)"
    else
      add_hit "$rule" "$f:$line:$text"
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
  printf '\nscanned: %d C/C++, %d notebook, %d .py/.cfg, %d shell/Makefile, %d BuildFile.xml\n' \
    "$(count_class "${CXX_GLOBS[@]}")" \
    "$(count_class "${IPY_GLOBS[@]}")" \
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
