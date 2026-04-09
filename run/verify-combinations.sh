#!/usr/bin/env bash
# Compile and run every cache-flag combination without NDEBUG so that
# assertions are active, the small verify data paths execute, and only
# one seed is used.

set -euo pipefail

cxx="${CXX:-g++}"
jobs="${JOBS:-$(nproc)}"

workdir="$(mktemp -d)"
cleanup()
{
  rm -rf "$workdir"
}
trap cleanup EXIT

# Generate all 16 combinations of the 4 caching flags.
combinations=()
for cuboid in 0 1; do
  for ruler in 0 1; do
    for end in 0 1; do
      for lub in 0 1; do
        combinations+=("$cuboid $ruler $end $lub")
      done
    done
  done
done

printf 'compiler: %s\n' "$("$cxx" --version | head -n 1)"
printf 'parallel jobs: %s\n' "$jobs"
printf 'combinations: %s\n\n' "${#combinations[@]}"

verify_one()
{
  local cuboid="$1" ruler="$2" end="$3" lub="$4"
  local label="cuboid=${cuboid} ruler=${ruler} end=${end} lub=${lub}"
  local tag="c${cuboid}r${ruler}e${end}l${lub}"
  local binary="$workdir/$tag"
  local log="$workdir/${tag}.log"

  if ! "$cxx" -std=c++23 -O3 -I. \
      "-DIPT_BENCHMARK_CACHE_CUBOID_SIZE=${cuboid}" \
      "-DIPT_BENCHMARK_CACHE_RULER_SIZE=${ruler}" \
      "-DIPT_BENCHMARK_CACHE_ENTRY_END=${end}" \
      "-DIPT_BENCHMARK_CACHE_ENTRY_LUB=${lub}" \
      benchmark.cpp -o "$binary" \
      >"$log" 2>&1
  then
    printf 'FAILURE  %s  compile error:\n' "$label"
    sed 's/^/  /' "$log"
    return 1
  fi

  if ! "$binary" >"$log" 2>&1; then
    printf 'FAILURE  %s  runtime error (exit %s):\n' "$label" "$?"
    tail -n 20 "$log" | sed 's/^/  /'
    return 1
  fi

  printf 'OK       %s\n' "$label"
}
export -f verify_one
export cxx workdir

failures="$workdir/failures"
export failures

printf '%s\n' "${combinations[@]}" \
  | xargs -P"$jobs" -I{} bash -c 'verify_one {} || touch "$failures"'

if [[ -e "$failures" ]]; then
  printf '\nFAILED: some combinations did not pass.\n'
  exit 1
fi

printf '\nAll %s combinations passed.\n' "${#combinations[@]}"
