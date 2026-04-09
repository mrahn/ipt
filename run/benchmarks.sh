#!/usr/bin/env bash

set -euo pipefail

cxx="${CXX:-g++}"

. "$(dirname "$0")/common/result_dir"

mkdir -p results
mkdir -p "$result_dir"

random_seed_count="${IPT_BENCHMARK_RANDOM_SEED_COUNT:-30}"
structured_seed_count="${IPT_BENCHMARK_STRUCTURED_SEED_COUNT:-5}"
all_query_cap="${IPT_BENCHMARK_ALL_QUERY_CAP:-100000}"
heavy_all_query_cap="${IPT_BENCHMARK_HEAVY_ALL_QUERY_CAP:-10000}"
reference_config="${IPT_BENCHMARK_REFERENCE_CONFIG:-c0r0e0l0}"

cache_dependent_algorithms="${IPT_BENCHMARK_CACHE_DEPENDENT_ALGORITHMS:-greedy-plus-merge,greedy-plus-combine,regular-ipt}"
light_cache_independent_algorithms="${IPT_BENCHMARK_LIGHT_CACHE_INDEPENDENT_ALGORITHMS:-sorted-points,lex-run,row-csr-k1,row-csr-k2,grid}"
heavy_cache_independent_algorithms="${IPT_BENCHMARK_HEAVY_CACHE_INDEPENDENT_ALGORITHMS:-dense-bitset,block-bitmap}"

grid_scenarios="${IPT_BENCHMARK_GRID_SCENARIOS:-regular,random-1pct,random-2pct,random-5pct,random-10pct,random-25pct,random-50pct,random-75pct,random-90pct,random-95pct,random-98pct,random-99pct,random-100pct}"
core_structured_scenarios="${IPT_BENCHMARK_CORE_STRUCTURED_SCENARIOS:-multiple-survey-2-l,multiple-survey-3-steps,multiple-survey-4-overlap,multiple-survey-5-mixed,multiple-survey-6-bands,multiple-survey-7-large,multiple-survey-8-threed,multiple-survey-10-sixteen,multiple-survey-11-thirtysix}"
heavy_structured_scenarios="${IPT_BENCHMARK_HEAVY_STRUCTURED_SCENARIOS:-multiple-survey-2-l,multiple-survey-3-steps,multiple-survey-4-overlap,multiple-survey-5-mixed,multiple-survey-6-bands,multiple-survey-7-large,multiple-survey-8-threed}"

selected_combinations=()
if [[ -n "${IPT_BENCHMARK_COMBINATIONS:-}" ]]; then
  IFS=, read -r -a selected_combinations <<< "${IPT_BENCHMARK_COMBINATIONS}"
fi

combinations=()
for cuboid in 0 1; do
  for ruler in 0 1; do
    for end in 0 1; do
      for lub in 0 1; do
        combinations+=("c${cuboid}r${ruler}e${end}l${lub} ${cuboid} ${ruler} ${end} ${lub}")
      done
    done
  done
done

workdir="$(mktemp -d)"
cleanup()
{
  rm -rf "$workdir"
}
trap cleanup EXIT

selected_combination()
{
  local label="$1"

  if [[ "${#selected_combinations[@]}" -eq 0 ]]; then
    return 0
  fi

  local candidate
  for candidate in "${selected_combinations[@]}"; do
    if [[ "$candidate" == "$label" ]]; then
      return 0
    fi
  done

  return 1
}

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
    -o
    "$binary"
  )

  printf 'combination: %s\n' "$label"
  printf 'compiler command:'
  printf ' %q' "${compile_command[@]}"
  printf '\n'

  "${compile_command[@]}"
}

run_filtered_benchmark()
{
  local binary="$1"
  local output="$2"
  local algorithms="$3"
  local scenarios="$4"
  local seed_count="$5"
  local query_cap="${6:-$all_query_cap}"
  local command=("$binary")

  if [[ -z "$algorithms" || -z "$scenarios" ]]; then
    return
  fi

  if command -v stdbuf > /dev/null 2>&1; then
    command=(stdbuf -oL -eL "$binary")
  fi

  IPT_BENCHMARK_ONLY_ALGORITHMS="$algorithms" \
  IPT_BENCHMARK_ONLY_SCENARIOS="$scenarios" \
  IPT_BENCHMARK_SEED_COUNT="$seed_count" \
  IPT_BENCHMARK_ALL_QUERY_CAP="$query_cap" \
  "${command[@]}" >> "$output"
}

reference_output="$result_dir/cache-independent-${reference_config}.txt"

if [[ ! -e "$reference_output" ]]; then
  read -r reference_c reference_r reference_e reference_l \
    <<< "$(decode_combination "$reference_config")"

  reference_binary="$workdir/benchmark-${reference_config}-reference"
  compile_combination_binary \
    "$reference_config" \
    "$reference_c" \
    "$reference_r" \
    "$reference_e" \
    "$reference_l" \
    "$reference_binary"

  : > "$reference_output"

  printf 'cache-independent run: grid/random light baselines (%s seeds)\n' "$random_seed_count"
  run_filtered_benchmark \
    "$reference_binary" \
    "$reference_output" \
    "$light_cache_independent_algorithms" \
    "$grid_scenarios" \
    "$random_seed_count"

  printf 'cache-independent run: structured light baselines (%s seeds)\n' "$structured_seed_count"
  run_filtered_benchmark \
    "$reference_binary" \
    "$reference_output" \
    "$light_cache_independent_algorithms" \
    "$core_structured_scenarios" \
    "$structured_seed_count"

  printf 'cache-independent run: structured heavy baselines (%s seeds)\n' "$structured_seed_count"
  run_filtered_benchmark \
    "$reference_binary" \
    "$reference_output" \
    "$heavy_cache_independent_algorithms" \
    "$heavy_structured_scenarios" \
    "$structured_seed_count" \
    "$heavy_all_query_cap"
else
  printf 'skipping cache-independent reference: %s already exists\n' "$reference_output"
fi

for entry in "${combinations[@]}"; do
  read -r label c r e l <<< "$entry"

  if ! selected_combination "$label"; then
    continue
  fi

  output="$result_dir/${label}.txt"

  if [[ -e "$output" ]]; then
    printf 'skipping %s: %s already exists\n' "$label" "$output"
    continue
  fi

  binary="$workdir/benchmark-${label}"
  compile_combination_binary "$label" "$c" "$r" "$e" "$l" "$binary"

  : > "$output"

  printf 'cache-dependent run: %s grid/random scenarios (%s seeds)\n' "$label" "$random_seed_count"
  run_filtered_benchmark \
    "$binary" \
    "$output" \
    "$cache_dependent_algorithms" \
    "$grid_scenarios" \
    "$random_seed_count"

  printf 'cache-dependent run: %s structured scenarios (%s seeds)\n' "$label" "$structured_seed_count"
  run_filtered_benchmark \
    "$binary" \
    "$output" \
    "$cache_dependent_algorithms" \
    "$core_structured_scenarios" \
    "$structured_seed_count"
done

printf 'saved results to %s\n' "$result_dir"
