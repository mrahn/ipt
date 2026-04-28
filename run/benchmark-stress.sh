#!/usr/bin/env bash

set -euo pipefail

cxx="${CXX:-g++}"

. "$(dirname "$0")/common/result_dir"

mkdir -p results
mkdir -p "$result_dir"

stress_seed_count="${IPT_BENCHMARK_STRESS_SEED_COUNT:-1}"
all_query_cap="${IPT_BENCHMARK_ALL_QUERY_CAP:-100000}"
stress_scenarios="${IPT_BENCHMARK_STRESS_SCENARIOS:-multiple-survey-9-5d,multiple-survey-12-fortynine}"
cache_dependent_algorithms="${IPT_BENCHMARK_CACHE_DEPENDENT_ALGORITHMS:-greedy-plus-merge,greedy-plus-combine,regular-ipt}"

IFS=, read -r -a stress_configs <<< "${IPT_BENCHMARK_STRESS_CONFIGS:-${IPT_BENCHMARK_COMBINATIONS:-c0r1e1l0}}"

workdir="$(mktemp -d)"
cleanup()
{
  rm -rf "$workdir"
}
trap cleanup EXIT

decode_combination()
{
  local label="$1"

  if [[ ! "$label" =~ ^c([01])r([01])e([01])l([01])$ ]]; then
    printf 'invalid cache combination: %s\n' "$label" >&2
    exit 1
  fi

  printf '%s %s %s %s\n' \
    "${BASH_REMATCH[1]}" \
    "${BASH_REMATCH[2]}" \
    "${BASH_REMATCH[3]}" \
    "${BASH_REMATCH[4]}"
}

compile_combination_binary()
{
  local label="$1"
  local cuboid="$2"
  local ruler="$3"
  local end="$4"
  local lub="$5"
  local binary="$6"

  local compile_command=(
    "$cxx"
    -std=c++23
    -O3
    -DNDEBUG
    -I.
    "-DIPT_BENCHMARK_CACHE_CUBOID_SIZE=${cuboid}"
    "-DIPT_BENCHMARK_CACHE_RULER_SIZE=${ruler}"
    "-DIPT_BENCHMARK_CACHE_ENTRY_END=${end}"
    "-DIPT_BENCHMARK_CACHE_ENTRY_LUB=${lub}"
    benchmark.cpp
    third_party/CRoaring/roaring.c
    -o
    "$binary"
  )

  printf 'stress combination: %s\n' "$label"
  printf 'compiler command:'
  printf ' %q' "${compile_command[@]}"
  printf '\n'

  "${compile_command[@]}"
}

run_filtered_benchmark()
{
  local binary="$1"
  local output="$2"

  IPT_BENCHMARK_ONLY_ALGORITHMS="$cache_dependent_algorithms" \
  IPT_BENCHMARK_ONLY_SCENARIOS="$stress_scenarios" \
  IPT_BENCHMARK_SEED_COUNT="$stress_seed_count" \
  IPT_BENCHMARK_ALL_QUERY_CAP="$all_query_cap" \
  "$binary" > "$output"
}

for label in "${stress_configs[@]}"; do
  output="$result_dir/${label}-stress.txt"

  if [[ -e "$output" ]]; then
    printf 'skipping %s stress run: %s already exists\n' "$label" "$output"
    continue
  fi

  read -r c r e l <<< "$(decode_combination "$label")"

  binary="$workdir/benchmark-${label}-stress"
  compile_combination_binary "$label" "$c" "$r" "$e" "$l" "$binary"

  printf 'stress run: %s (%s seeds)\n' "$label" "$stress_seed_count"
  run_filtered_benchmark "$binary" "$output"
done

printf 'saved stress results to %s\n' "$result_dir"