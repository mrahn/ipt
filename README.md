# IPT

The paper and benchmark code for the Adaptive Index-Point Translator
(IPT), a flat rank/unrank representation for irregular spatial data.

## Contents

- `IPT.tex` — LaTeX source of the paper.
- `IPT.bib` — BibTeX database.
- `texmf/` — ACM `acmart` class and bundled style files.
- `Makefile` — build targets for the paper, plots, and benchmarks.
- `verify.cpp` — small driver that runs the corner-steal,
  cross-implementation select, and storage round-trip verifiers.
  Built once per cache-flag combination by `make verify`.
- `benchmark/Common.hpp` — shared benchmark infrastructure (`Data`,
  `BenchmarkContext`, `TimedBuild`, `TimedQueries`, the
  `run_scenario_*` and `run_storage_scenario` template functions,
  and the std::formatter specialisations).
- `benchmark/Verify.hpp` — verifier driver functions used by
  `verify.cpp`.
- `benchmark/Scenarios.hpp` — data factories for the structured
  multiple-survey scenarios.
- `benchmark/scenarios/<name>-cache-{dependent,independent}.cpp`,
  `benchmark/scenarios/storage.cpp` — one source file per scenario
  per algorithm class; each compiles to its own benchmark binary.
  No env-variable selection: scenario, seed count, query cap and
  algorithm subset are all hard-coded per file.
- `ipt/` — header-only IPT library included by the benchmark sources.
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

The benchmark sources include headers from the `ipt/` and
`benchmark/` directories and use the C++23 standard-library headers
`<format>` and `<print>`, so the repository root must be on the
include path and the compiler/library combination must provide those
headers.  In normal use you do not invoke the compiler directly; the
Makefile builds one binary per scenario in `build/benchmark/` and
links it against the once-built `build/benchmark/libroaring.a`.

If you want to build a single scenario by hand the invocation is:

```sh
cc  -std=c11   -O3 -DNDEBUG -I. -c third_party/CRoaring/roaring.c \
      -o build/benchmark/roaring.o
ar rcs build/benchmark/libroaring.a build/benchmark/roaring.o
g++ -std=c++23 -O3 -DNDEBUG -I. \
    -DIPT_BENCHMARK_CACHE_CUBOID_SIZE=0 \
    -DIPT_BENCHMARK_CACHE_RULER_SIZE=1 \
    -DIPT_BENCHMARK_CACHE_ENTRY_END=1 \
    -DIPT_BENCHMARK_CACHE_ENTRY_LUB=0 \
    benchmark/scenarios/multiple-survey-2-l-cache-dependent.cpp \
    build/benchmark/libroaring.a \
    -o build/benchmark/multiple-survey-2-l-cache-dependent-c0r1e1l0
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
cache-flag combinations with assertions enabled on a small
representative scenario subset, while `make benchmark` runs all 16
combinations of the cache-dependent algorithms across the structured
and grid scenarios and runs the cache-independent baselines once at
the reference cache configuration `c0r1e1l0`.

Seed count and `pos_all` / `at_all` query cap are now hard-coded per
scenario binary (30 seeds for the grid sweep, 5 seeds for the
structured scenarios, 10000-query cap for the heavy baselines and
100000 elsewhere).  Override per-binary by editing the corresponding
file under `benchmark/scenarios/`.

## Running the benchmarks

```sh
# Step 0 — verify all 16 cache-flag combinations compile and run
#          on a reduced assertion-enabled scenario subset:
make -j$(nproc) verify

# Step 1 — full paper-reproduction suite (all per-scenario binaries,
#          all 16 cache combinations for the cache-dependent
#          algorithms, plus storage and 5D stress runs):
make -j$(nproc) benchmark

# Or run individual pieces:
make benchmark-grid                                # grid sweep, both axes, reference combo
make benchmark-multiple-survey-2-l-cache-dependent  # one scenario, cache-dependent
make benchmark-multiple-survey-2-l-cache-independent
make benchmark-cache-sweep                         # 16 combos x cache-dependent scenarios
make benchmark-storage                             # storage benchmark for c0r0e0l0 and c0r1e1l0
make benchmark-5D                                  # multiple-survey-9-5d stress
```

There are no longer any environment variables that influence which
scenarios or algorithms run.  Each scenario binary is one translation
unit with its own constants; rebuild from a different cache combo by
selecting a different Make target.  Per-scenario targets append to
the shared per-combo result file (`${result_dir}/<combo>.txt`).  The
top-level `make benchmark` target truncates each aggregate file
first, then runs all scenarios sequentially in a fixed order so that
the aggregated text files are deterministic.

`make verify` compiles and runs `verify.cpp` for every combination of
the four cache flags (2⁴ = 16) without `-DNDEBUG`, so assertions stay
enabled.  It only runs the verifier paths (intersect, corner-steal,
select, storage round-trip), not the full benchmark scenarios.

`make benchmark` auto-detects the compiler, OS, CPU, and kernel and
writes results into a directory named
`results/os_<os>__kernel_<kernel>__cpu_<cpu>__compiler_<compiler>`.
The directory is created automatically if needed. It compiles without
assertions and emits the same result groups as before:

- `${result_dir}/<combo>.txt` contain the cache-dependent algorithms
  (`greedy-plus-merge`, `greedy-plus-combine`, `regular-ipt`) for the
  given cache combination.  The `c0r1e1l0` file is produced by
  `make benchmark`'s reference run; the other 15 files come from
  `make benchmark-cache-sweep`.
- `${result_dir}/cache-independent-c0r0e0l0.txt` contains the
  cache-independent baselines (`sorted-points`, `lex-run`,
  `row-csr-k1`, `row-csr-km`, `grid`) for all scenarios, plus the
  heavy baselines (`dense-bitset`, `block-bitmap`, `roaring`) for
  the smaller structured scenarios `multiple-survey-{2..8}` at a
  10000-query cap. The file is named after the no-caches combination
  for backward compatibility with the plot extractors; the
  cache-independent algorithms are unaffected by the cache flags so
  the actual binary used is built at the reference combination.
- `${result_dir}/c0r1e1l0-stress.txt` contains the 5D stress
  scenario (`multiple-survey-9-5d`) for both algorithm classes.
- `${result_dir}/storage-c0r0e0l0.txt` and
  `${result_dir}/storage-c0r1e1l0.txt` contain the
  serialize/load/first-response measurements for the structured
  scenarios on the two reported cache combinations.

The `roaring` baseline is implemented on top of CRoaring v4.6.1, which
is vendored unmodified under `third_party/CRoaring/` (dual Apache-2.0 /
MIT licensed; see `third_party/CRoaring/LICENSE`). The Makefile
compiles `third_party/CRoaring/roaring.c` once into
`build/benchmark/libroaring.a` and links every scenario binary against
it; no system installation of CRoaring is required.

Set `CXX` to override the compiler:

```sh
make -j$(nproc) benchmark CXX=clang++
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
| `make verify`    | Compile and run all 16 cache combinations with assertions enabled (corner-steal, select-verify, storage round-trip). |
| `make benchmark` | Full paper-reproduction suite (all per-scenario binaries, all 16 cache combinations of the cache-dependent algorithms, storage, 5D stress). |
| `make benchmark-grid` | Grid sweep (both algorithm classes) for the reference cache combo. |
| `make benchmark-X-cache-dependent` | One scenario, cache-dependent algorithms, reference combo. |
| `make benchmark-X-cache-independent` | One scenario, cache-independent baselines, reference combo. |
| `make benchmark-cache-sweep` | All 16 cache combinations of the cache-dependent scenarios. |
| `make benchmark-storage` | Storage benchmark for `c0r0e0l0` and `c0r1e1l0`. |
| `make benchmark-5D` | Stress benchmark (`multiple-survey-9-5d`), reference combo. |
| `make clean`     | Remove intermediate files, `generated/` and `build/`. |
| `make distclean` | clean plus remove IPT.pdf.                 |
| `make help`      | Show this help message.                    |
