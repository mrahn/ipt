# IPT

The paper and benchmark code for the Adaptive Index-Point Translator
(IPT), a flat rank/unrank representation for irregular spatial data.

## Contents

- `IPT.tex` — LaTeX source of the paper.
- `IPT.bib` — BibTeX database.
- `texmf/` — ACM `acmart` class and bundled style files.
- `Makefile` — build targets for the paper, plots, and benchmarks.
- `benchmark.cpp` — C++ benchmark driver (single translation unit).
  Emits `pos`/`at` measurements as well as `select_d*` and
  `restrict_d*` measurements for four relative query-window volumes. In debug
  builds (no `-DNDEBUG`) it additionally runs the corner-steal and
  cross-implementation selection verifiers at startup, so `make verify`
  exercises both as part of the 16-combination sweep.
- `ipt/` — header-only IPT library included by `benchmark.cpp`.
- `run/` — shell scripts for verification and benchmarking.
- `plot/` — gnuplot scripts and small C++ extractors
  (`plot/*.cpp`, `plot/IPTPlot.hpp`) that turn raw result files into
  the TSV inputs of the figures included by the paper, plus
  `plot/select-restrict-summary.cpp` for a stand-alone select/restrict
  summary report. The Makefile compiles the extractors on the fly and
  runs them in parallel.

## Prerequisites

- `make`.
- A C++23 compiler with standard-library support for `<format>` and
  `<print>`. Known good compilers for this repository are GCC 14 and
  Clang 20.
- `gnuplot` (with the `cairolatex` terminal).
- A TeX Live distribution with `pdflatex` and `bibtex`. The ACM
  `acmart` class is included in `texmf/`.

## Compiling the benchmark

`benchmark.cpp` includes headers from the `ipt/` directory and uses
the C++23 standard-library headers `<format>` and `<print>`, so the
repository root must be on the include path and the compiler/library
combination must provide those headers.  The simplest invocation is:

```sh
g++ -std=c++23 -O3 -DNDEBUG -I. benchmark.cpp -o benchmark
```

### Compile-time switches

The cache switches default to `0` (disabled) and are activated by
defining them to `1` on the compiler command line
(`-DIPT_BENCHMARK_<NAME>=1`).

| Macro                                      | Effect                                                                                                                    |
|--------------------------------------------|---------------------------------------------------------------------------------------------------------------------------|
| `IPT_BENCHMARK_CACHE_CUBOID_SIZE`          | Cache the cuboid size inside each cuboid so that `size()` is O(1) instead of recomputed from rulers.                      |
| `IPT_BENCHMARK_CACHE_RULER_SIZE`           | Cache the ruler size (tick count) inside each `Ruler` so that `size()` is O(1) instead of computed from begin/stride/end. |
| `IPT_BENCHMARK_CACHE_ENTRY_END`            | Cache the entry end-index, avoiding its recomputation during queries.                                                     |
| `IPT_BENCHMARK_CACHE_ENTRY_LUB`            | Cache the entry least-upper-bound point.                                                                                  |

The four `CACHE_*` flags control which values are stored eagerly
versus recomputed on the fly. `make verify` exercises all 16
cache-flag combinations with assertions enabled on a reduced,
representative scenario subset, while `make benchmark` runs all 16
cache-flag combinations without assertions for the cache-dependent
algorithms and runs the cache-independent baselines only once.

The seed count is controlled only by `NDEBUG`: non-`NDEBUG` builds use
1 seed, and `-DNDEBUG` builds use 30 seeds. You can override the default
at runtime with `IPT_BENCHMARK_SEED_COUNT`.

The exhaustive `pos_all` and `at_all` benchmark phases are capped by
default at 100000 evenly spaced queries per scenario. Override that cap
with `IPT_BENCHMARK_ALL_QUERY_CAP`, or set it to `0` to disable the cap.

## Running the benchmarks

```sh
# Step 0 — verify all 16 cache-flag combinations compile and run on a reduced assertion-enabled scenario subset:
make verify

# Step 1 — core suite: all 16 cache combinations for IPT, cache-independent baselines once:
make benchmark

# Step 2 — optional stress scenarios (defaults to c0r1e1l0 only):
make benchmark-stress
```

For focused runs, the benchmark binary also accepts exact-match
comma-separated filters via environment variables:

- `IPT_BENCHMARK_ONLY_SCENARIOS` — limit output to the listed scenario names.
- `IPT_BENCHMARK_ONLY_ALGORITHMS` — limit output to the listed algorithm names.
- `IPT_BENCHMARK_SEED_COUNT` — override the default seed count.
- `IPT_BENCHMARK_ALL_QUERY_CAP` — cap the `pos_all` and `at_all` query count (`0` disables the cap).

These filters are applied before large scenario data sets are built, so
they are suitable for timing individual scenario/algorithm combinations.
For example:

```sh
IPT_BENCHMARK_ONLY_SCENARIOS=multiple-survey-5-mixed \
IPT_BENCHMARK_ONLY_ALGORITHMS=block-bitmap \
IPT_BENCHMARK_SEED_COUNT=1 \
IPT_BENCHMARK_ALL_QUERY_CAP=10000 \
./benchmark
```

`verify` compiles and runs every combination of the
four cache flags (2⁴ = 16) without `-DNDEBUG`, so assertions stay
enabled, the reduced verification paths run, and exactly 1 seed is
used. It intentionally runs only a small representative subset of the
scenario space so that it catches compile errors and obvious
assertion/logic failures quickly. Compilation and execution use up to
`nproc` parallel jobs (override with `JOBS=N`). Each combination is
reported as `OK` or `FAILURE` with the error inlined.

`make benchmark` auto-detects the compiler, OS, CPU, and kernel and
writes results into a directory named
`results/os_<os>__kernel_<kernel>__cpu_<cpu>__compiler_<compiler>`.
The directory is created automatically if needed. It compiles without
assertions and emits two result groups:

- `c*.txt` contain the cache-dependent algorithms (`greedy-plus-merge`,
  `greedy-plus-combine`, `regular-ipt`) for all 16 cache configurations.
  The default core sweep uses 30 seeds on `regular` and the random-density
  scenarios, and 5 seeds on the selected structured scenarios
  `multiple-survey-{2,3,4,5,6,7,8,10,11}`.
- `cache-independent-<config>.txt` contains the cache-independent
  baselines measured once with the reference cache configuration
  (`c0r0e0l0` by default). The heavy baselines (`dense-bitset`,
  `block-bitmap`, and the vendored `roaring` Roaring-bitmap baseline) are
  restricted to the smaller structured scenarios
  `multiple-survey-{2,3,4,5,6,7,8}` in the default suite and use a lower
  default `pos_all` / `at_all` cap of 10000 queries.

The `roaring` baseline is implemented on top of CRoaring v4.6.1, which
is vendored unmodified under `third_party/CRoaring/` (dual Apache-2.0 /
MIT licensed; see `third_party/CRoaring/LICENSE`). The `benchmark.cpp`
build commands compile `third_party/CRoaring/roaring.c` together with
`benchmark.cpp` in a single invocation; no system installation of
CRoaring is required.

`make benchmark-stress` is opt-in and runs only the large structured
stress scenarios `multiple-survey-9-5d` and
`multiple-survey-12-fortynine` for the selected cache configurations
(`c0r1e1l0` by default) using the cache-dependent algorithms only.

Both benchmark scripts are incremental. If the expected output file is
already present in the target `results/` directory, the script prints a
skip message and leaves the existing file untouched.

Both benchmark scripts accept a few useful environment overrides:

- `IPT_BENCHMARK_COMBINATIONS` — comma-separated cache combinations for `make benchmark`.
- `IPT_BENCHMARK_REFERENCE_CONFIG` — cache combination used for the one-off cache-independent baseline file.
- `IPT_BENCHMARK_RANDOM_SEED_COUNT` — seed count for `regular` and random-density scenarios in `make benchmark`.
- `IPT_BENCHMARK_STRUCTURED_SEED_COUNT` — seed count for the structured core scenarios in `make benchmark`.
- `IPT_BENCHMARK_HEAVY_ALL_QUERY_CAP` — `pos_all` / `at_all` cap used only for the heavy cache-independent structured phase in `make benchmark`.
- `IPT_BENCHMARK_CACHE_DEPENDENT_ALGORITHMS` — override the cache-dependent algorithm set for `make benchmark` or `make benchmark-stress`.
- `IPT_BENCHMARK_LIGHT_CACHE_INDEPENDENT_ALGORITHMS` — override the light cache-independent baseline set for `make benchmark`.
- `IPT_BENCHMARK_HEAVY_CACHE_INDEPENDENT_ALGORITHMS` — override the heavy cache-independent baseline set for `make benchmark`.
- `IPT_BENCHMARK_GRID_SCENARIOS` — override the grid/random scenario list for `make benchmark`.
- `IPT_BENCHMARK_CORE_STRUCTURED_SCENARIOS` — override the structured core scenario list for `make benchmark`.
- `IPT_BENCHMARK_HEAVY_STRUCTURED_SCENARIOS` — override the dense-bitmap structured scenario list for `make benchmark`.
- `IPT_BENCHMARK_STRESS_CONFIGS` — comma-separated cache combinations for `make benchmark-stress`.
- `IPT_BENCHMARK_STRESS_SEED_COUNT` — seed count for `make benchmark-stress`.
- `IPT_BENCHMARK_STRESS_SCENARIOS` — override the stress-scenario list for `make benchmark-stress`.

Set `CXX` to override the compiler:

```sh
make benchmark CXX=clang++
```

Run both steps on every target platform, then gather the
`results/` directories into the repository root for plot generation.

## Generating the paper

Once the repository contains the result directories used by the paper:

```sh
make -j$(nproc) paper
```

This does not rerun the benchmarks.  It only extracts data from the
files in `results/`, renders the gnuplot figures, builds the
bibliography, and compiles the paper.  The extraction scripts select
the result directories and files expected by the paper, so additional
result directories are ignored unless they match those selection
patterns.  The benchmark binary is therefore used only to produce or
refresh raw result files.  The `-jX` form is recommended here, for
example `make -j8 paper` or `make -j$(nproc) paper`, because extractor
compilation and gnuplot figure generation are parallelized by the
Makefile. The final `pdflatex` and `bibtex` stages remain ordered by
dependencies. The intermediate plots can also be generated separately
with `make -jX plots`.

## Make targets

| Target           | Description                                |
|------------------|--------------------------------------------|
| `make paper`     | Build IPT.pdf. Prefer `make -jX paper` to parallelize extractor builds and plot generation. |
| `make plots`     | Build or refresh the generated figure assets without running LaTeX. |
| `make verify`    | Compile and run all 16 cache combinations with assertions enabled (includes corner-steal and select-verify). |
| `make benchmark` | Run the default core benchmark suite (includes select/restrict). |
| `make benchmark-stress` | Run the optional large structured stress suite. |
| `make clean`     | Remove intermediate files and generated/.  |
| `make distclean` | clean plus remove IPT.pdf.                 |
| `make help`      | Show this help message.                    |
