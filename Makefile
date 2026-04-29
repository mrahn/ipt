CXX           ?= g++
JOBS          ?= $(shell nproc)

EXTRACT_CXXFLAGS ?= -std=c++23 -O2 -Wall -Wextra
EXTRACT_LDFLAGS  ?=

TEXENV  = TEXINPUTS=".:./texmf/:" \
          BSTINPUTS=".:./texmf/:"

# Result directory: results/os_<id>__kernel_<id>__cpu_<id>__compiler_<id>.
# Each component is lower-cased and reduced to [a-z0-9._-].  The CPU
# label comes from /proc/cpuinfo's "model name" / "Processor" / "cpu
# model" line (whichever appears first); the OS label from
# /etc/os-release ($ID-$VERSION_ID, falling back to $PRETTY_NAME or
# $NAME); the kernel from `uname -r`; and the compiler from the first
# line of `$(CXX) --version`.  The same tag is used to give every
# host its own build/<platform>/benchmark/ directory so that two
# concurrent `make benchmark` invocations on a shared checkout do not
# clobber each other's binaries.
RESULT_DIR := $(shell \
  sanitize() { printf '%s' "$$1" | tr '[:upper:]' '[:lower:]' | tr -cs 'a-z0-9._-' '-'; }; \
  cpu="$$(awk -F': *' '/^(model name|Processor|cpu model)[[:space:]]*:/ { print $$2; exit }' /proc/cpuinfo)"; \
  if [ -z "$$cpu" ]; then cpu="$$(uname -m)"; fi; \
  os=""; \
  if [ -r /etc/os-release ]; then \
    . /etc/os-release; \
    if [ -n "$$ID" ] && [ -n "$$VERSION_ID" ]; then os="$$ID-$$VERSION_ID"; \
    elif [ -n "$$PRETTY_NAME" ]; then os="$$PRETTY_NAME"; \
    elif [ -n "$$NAME" ]; then os="$$NAME"; \
    fi; \
  fi; \
  if [ -z "$$os" ]; then os="$$(uname -s)"; fi; \
  kernel="$$(uname -r)"; \
  compiler="$$($(CXX) --version | head -n 1)"; \
  printf 'results/os_%s__kernel_%s__cpu_%s__compiler_%s\n' \
    "$$(sanitize "$$os")" "$$(sanitize "$$kernel")" \
    "$$(sanitize "$$cpu")" "$$(sanitize "$$compiler")")
PLATFORM_TAG := $(notdir $(RESULT_DIR))

# -- source file lists ------------------------------------------------

PLOT_NAMES :=
PLOT_NAMES += at-cache-effect
PLOT_NAMES += at-multiple-survey
PLOT_NAMES += construct-multiple-survey
PLOT_NAMES += construct-vs-density
PLOT_NAMES += kappa-vs-density
PLOT_NAMES += pos-multiple-survey
PLOT_NAMES += pos-ratio-vs-density
PLOT_NAMES += pos-mix-summary

# Extra extractor that produces a stand-alone summary report.
EXTRA_EXTRACTORS := select-restrict-summary stress-summary

EXTRACT_NAMES := $(PLOT_NAMES) $(EXTRA_EXTRACTORS)
EXTRACT_BIN_DIR := generated/extract
EXTRACT_HEADER  := plot/IPTPlot.hpp
EXTRACT_BINS    := $(patsubst %,$(EXTRACT_BIN_DIR)/%,$(EXTRACT_NAMES))

# Prefer the newer Rocky 9.7 i7 reruns on kernel 7 over the older
# kernel 6.19 snapshots.
RESULT_DIRS_ALL := $(sort $(patsubst %/,%,$(wildcard results/*/)))
RESULT_DIRS := $(filter-out results/os_rocky-9.7__kernel_6.19.%__cpu_13th-gen-intel-r-core-tm-i7-1370p__compiler_%,$(RESULT_DIRS_ALL))
RESULT_FILES := $(sort $(foreach dir,$(RESULT_DIRS),$(wildcard $(dir)/*.txt)))

# -- top-level targets ------------------------------------------------

.PHONY: paper verify benchmark benchmark-grid benchmark-cache-sweep \
        benchmark-storage benchmark-pos-mix benchmark-5D \
        plots clean distclean help \
        benchmark-build

help:
	@echo 'Available targets (REFERENCE_COMBINATION = c0r1e1l0):'
	@echo ''
	@echo 'Paper and figures:'
	@echo '  paper                            Build IPT.pdf.'
	@echo '  plots                            Build TSVs and figure assets only.'
	@echo ''
	@echo 'Verification (assertions enabled, no NDEBUG):'
	@echo '  verify                           Build and run verify.cpp for all 16'
	@echo '                                   cache-flag combinations in parallel.'
	@echo '  verify-<combination>             Single combination, e.g. verify-c0r1e1l0.'
	@echo ''
	@echo 'Benchmarks (NDEBUG, results appended to $${result_dir}/...):'
	@echo '  benchmark                        Full paper-reproduction suite.'
	@echo '  benchmark-grid                   Grid sweep, both algorithm classes,'
	@echo '                                   reference combination only.'
	@echo '  benchmark-<scenario>-cache-dependent'
	@echo '                                   One scenario, IPT (cache-dependent)'
	@echo '                                   algorithms, reference combination.'
	@echo '  benchmark-<scenario>-cache-independent'
	@echo '                                   One scenario, baseline (cache-'
	@echo '                                   independent) algorithms, reference'
	@echo '                                   combination.'
	@echo '  benchmark-cache-sweep            All cache-dependent scenarios across'
	@echo '                                   all 16 cache-flag combinations.'
	@echo '  benchmark-storage                Storage benchmark for combinations'
	@echo '                                   c0r0e0l0 and c0r1e1l0.'
	@echo '  benchmark-pos-mix                Hit-vs-miss try_pos benchmark for the'
	@echo '                                   7 structured 3D scenarios, reference'
	@echo '                                   combination.'
	@echo '  benchmark-<scenario>-pos-mix     One scenario, hit-vs-miss pos.'
	@echo '  benchmark-5D                     5D stress benchmark'
	@echo '                                   (multiple-survey-9-5d), reference'
	@echo '                                   combination.'
	@echo ''
	@echo 'Valid <scenario> values:'
	@echo '  grid'
	@echo '  multiple-survey-2-l         multiple-survey-3-steps'
	@echo '  multiple-survey-4-overlap   multiple-survey-5-mixed'
	@echo '  multiple-survey-6-bands     multiple-survey-7-large'
	@echo '  multiple-survey-8-threed    multiple-survey-9-5d'
	@echo '  multiple-survey-10-sixteen  multiple-survey-11-thirtysix'
	@echo '  multiple-survey-12-fortynine'
	@echo ''
	@echo 'Valid <combination> values: c<0|1>r<0|1>e<0|1>l<0|1>'
	@echo '  c = cache cuboid size,  r = cache ruler size,'
	@echo '  e = cache entry end,    l = cache entry least-upper-bound.'
	@echo ''
	@echo 'Cleanup:'
	@echo '  clean                            Remove generated/ and build/.'
	@echo '  distclean                        Run clean and remove IPT.pdf.'
	@echo '  help                             Show this message.'

paper: IPT.pdf

# -- benchmark binaries -----------------------------------------------
#
# Each scenario .cpp in benchmark/scenarios/ becomes its own binary.
# All binaries share benchmark/Common.hpp (algorithm machinery) and
# benchmark/Scenarios.hpp (data factories).  No env vars; the cache
# combination is selected at compile time via -DIPT_BENCHMARK_CACHE_*.
#
BENCHMARK_DIR        := $(CURDIR)/build/$(PLATFORM_TAG)/benchmark

BENCHMARK_CXXFLAGS   := -std=c++23 -O3 -DNDEBUG -I.
BENCHMARK_LDFLAGS    :=

ROARING_LIB      := $(BENCHMARK_DIR)/libroaring.a
ROARING_OBJ      := $(BENCHMARK_DIR)/roaring.o
ROARING_SRC      := third_party/CRoaring/roaring.c

# Companion C compiler derived from $(CXX) the same way the runner
# scripts do (g++ -> gcc, clang++ -> clang, c++ -> cc).  Make has a
# built-in default of "cc" with origin "default", which we override
# here; explicit CC= on the command line or in the environment wins.
ifeq ($(origin CC),default)
CXX_BASENAME := $(notdir $(CXX))
# $(dir foo) returns "./" when $(CXX) has no slash; strip that so the
# companion compiler name is searched on $PATH like $(CXX) was.
CXX_DIR      := $(patsubst ./,,$(dir $(CXX)))
ifneq (,$(filter g++%,$(CXX_BASENAME)))
CC := $(CXX_DIR)$(subst g++,gcc,$(CXX_BASENAME))
else ifneq (,$(filter clang++%,$(CXX_BASENAME)))
CC := $(CXX_DIR)$(subst clang++,clang,$(CXX_BASENAME))
else ifneq (,$(filter c++%,$(CXX_BASENAME)))
CC := $(CXX_DIR)$(subst c++,cc,$(CXX_BASENAME))
else
CC := $(CXX_DIR)cc
endif
endif

# Cache-flag presets (defined as recipes for use with $(call)).
# Format: c<cuboid>r<ruler>e<entryEnd>l<entryLub>
CACHE_DEFINES_FOR = -DIPT_BENCHMARK_CACHE_CUBOID_SIZE=$(word 1,$(subst c, ,$(subst r, ,$(subst e, ,$(subst l, ,$(1)))))) \
                   -DIPT_BENCHMARK_CACHE_RULER_SIZE=$(word 2,$(subst c, ,$(subst r, ,$(subst e, ,$(subst l, ,$(1)))))) \
                   -DIPT_BENCHMARK_CACHE_ENTRY_END=$(word 3,$(subst c, ,$(subst r, ,$(subst e, ,$(subst l, ,$(1)))))) \
                   -DIPT_BENCHMARK_CACHE_ENTRY_LUB=$(word 4,$(subst c, ,$(subst r, ,$(subst e, ,$(subst l, ,$(1))))))

# Reference cache combination ("good default") used by single-scenario
# targets and the cache-independent reference run.
REFERENCE_COMBINATION := c0r1e1l0
# Combo used for the storage benchmark with no caches.
BASELINE_COMBINATION  := c0r0e0l0

# All 16 cache combinations.
ALL_COMBINATIONS := $(foreach c,0 1, $(foreach r,0 1, $(foreach e,0 1, $(foreach l,0 1,c$(c)r$(r)e$(e)l$(l)))))

# All scenarios.  Order matters because some recipes append to a
# shared output file; we run scenarios in this fixed order so the
# aggregated text files are deterministic.
GRID_SCENARIO     := grid
STRUCTURED_SCENARIOS_3D := \
  multiple-survey-2-l \
  multiple-survey-3-steps \
  multiple-survey-4-overlap \
  multiple-survey-5-mixed \
  multiple-survey-6-bands \
  multiple-survey-7-large \
  multiple-survey-8-threed \
  multiple-survey-10-sixteen \
  multiple-survey-11-thirtysix \
  multiple-survey-12-fortynine
STRESS_SCENARIO   := multiple-survey-9-5d

# All scenarios that participate in the per-combination cache-dependent run.
CACHE_DEPENDENT_SCENARIOS := $(GRID_SCENARIO) $(STRUCTURED_SCENARIOS_3D)
# All scenarios that participate in the cache-independent reference run.
CACHE_INDEPENDENT_SCENARIOS := $(GRID_SCENARIO) $(STRUCTURED_SCENARIOS_3D)
# Storage subset: only the structured 3D scenarios that have substantial IPT structure.
STORAGE_SCENARIOS := \
  multiple-survey-2-l \
  multiple-survey-3-steps \
  multiple-survey-4-overlap \
  multiple-survey-5-mixed \
  multiple-survey-6-bands \
  multiple-survey-7-large \
  multiple-survey-8-threed
# Hit-vs-miss pos benchmark: same 7 structured 3D scenarios as storage.
POS_MIX_SCENARIOS := $(STORAGE_SCENARIOS)

.PHONY: paper verify benchmark benchmark-grid benchmark-cache-sweep \
        benchmark-storage benchmark-pos-mix benchmark-5D \
        plots clean distclean help \
        benchmark-build

# ---- libroaring.a (built once) --------------------------------------
$(BENCHMARK_DIR):
	mkdir -p $@

$(ROARING_OBJ): $(ROARING_SRC) | $(BENCHMARK_DIR)
	$(CC) -std=c11 -O3 -DNDEBUG -I. -c $< -o $@

$(ROARING_LIB): $(ROARING_OBJ)
	ar rcs $@ $<

# ---- verify ---------------------------------------------------------
#
# verify-c<cuboid>r<r>e<e>l<l> compiles verify.cpp WITHOUT NDEBUG so
# the verify::* assertions are active, then runs it.  `make verify`
# runs all 16 in parallel via $(MAKE) -j.
#
verify: $(addprefix verify-,$(ALL_COMBINATIONS))

define VERIFY_RULE
.PHONY: verify-$(1)
verify-$(1): $$(BENCHMARK_DIR)/verify-$(1)
	$$<
$$(BENCHMARK_DIR)/verify-$(1): verify.cpp benchmark/Common.hpp benchmark/Verify.hpp $$(ROARING_LIB) | $$(BENCHMARK_DIR)
	$$(CXX) -std=c++23 -O3 -I. $$(call CACHE_DEFINES_FOR,$(1)) $$< $$(ROARING_LIB) -o $$@
endef
$(foreach combination,$(ALL_COMBINATIONS),$(eval $(call VERIFY_RULE,$(combination))))

# ---- per-scenario binary build rules --------------------------------
#
# Pattern: $(BENCHMARK_DIR)/<scenario>-<axis>-<combination>
#   <scenario> e.g. multiple-survey-2-l
#   <axis>     cache-dependent | cache-independent | storage
#
# Build rule: <basename>-<combination> -> compiles benchmark/scenarios/<basename>.cpp
define SCENARIO_BUILD_RULE
$$(BENCHMARK_DIR)/$(1)-$(2): benchmark/scenarios/$(1).cpp benchmark/Common.hpp benchmark/Scenarios.hpp $$(ROARING_LIB) | $$(BENCHMARK_DIR)
	$$(CXX) $$(BENCHMARK_CXXFLAGS) $$(call CACHE_DEFINES_FOR,$(2)) $$< $$(ROARING_LIB) $$(BENCHMARK_LDFLAGS) -o $$@
endef

# ---- per-scenario "run" targets -------------------------------------
#
# Each per-scenario target builds its binary (reference combination only)
# and APPENDS its stdout to the matching aggregate result file.  Use
# `make benchmark` to truncate aggregates first and rebuild from
# scratch.  Use `make benchmark-X-...` to ad-hoc append a single
# scenario.
#
# Aggregate file naming follows the legacy scheme so that the plot
# extractors continue to work unchanged:
#   ${result_dir}/<combination>.txt                 cache-dependent
#   ${result_dir}/cache-independent-<combination>.txt   cache-independent
#   ${result_dir}/<combination>-stress.txt          stress (5D)
#   ${result_dir}/storage-<combination>.txt         storage
#
$(RESULT_DIR):
	mkdir -p $@

# Cache-dependent reference run targets (one per scenario):
define SCENARIO_RUN_CD_RULE
$$(eval $$(call SCENARIO_BUILD_RULE,$(1)-cache-dependent,$$(REFERENCE_COMBINATION)))
.PHONY: benchmark-$(1)-cache-dependent
benchmark-$(1)-cache-dependent: $$(BENCHMARK_DIR)/$(1)-cache-dependent-$$(REFERENCE_COMBINATION) | $$(RESULT_DIR)
	$$< >> $$(RESULT_DIR)/$$(REFERENCE_COMBINATION).txt
endef
$(foreach s,$(CACHE_DEPENDENT_SCENARIOS),$(eval $(call SCENARIO_RUN_CD_RULE,$(s))))

# Cache-independent reference run targets (one per scenario):
#
# Cache-independent algorithms ignore the cache flags, so the file is
# named with $(BASELINE_COMBINATION) (c0r0e0l0) for backwards-
# compatibility with the plot extractors and the existing per-platform
# results.  The binary itself is still compiled with the reference
# combination because we have to pick one; the choice has no effect on
# the measurements.
define SCENARIO_RUN_CI_RULE
$$(eval $$(call SCENARIO_BUILD_RULE,$(1)-cache-independent,$$(REFERENCE_COMBINATION)))
.PHONY: benchmark-$(1)-cache-independent
benchmark-$(1)-cache-independent: $$(BENCHMARK_DIR)/$(1)-cache-independent-$$(REFERENCE_COMBINATION) | $$(RESULT_DIR)
	$$< >> $$(RESULT_DIR)/cache-independent-$$(BASELINE_COMBINATION).txt
endef
$(foreach s,$(CACHE_INDEPENDENT_SCENARIOS),$(eval $(call SCENARIO_RUN_CI_RULE,$(s))))

# 5D stress targets (cache-dependent + cache-independent):
$(eval $(call SCENARIO_BUILD_RULE,$(STRESS_SCENARIO)-cache-dependent,$(REFERENCE_COMBINATION)))
$(eval $(call SCENARIO_BUILD_RULE,$(STRESS_SCENARIO)-cache-independent,$(REFERENCE_COMBINATION)))
.PHONY: benchmark-$(STRESS_SCENARIO)-cache-dependent benchmark-$(STRESS_SCENARIO)-cache-independent
benchmark-$(STRESS_SCENARIO)-cache-dependent: $(BENCHMARK_DIR)/$(STRESS_SCENARIO)-cache-dependent-$(REFERENCE_COMBINATION) | $(RESULT_DIR)
	$< >> $(RESULT_DIR)/$(REFERENCE_COMBINATION)-stress.txt
benchmark-$(STRESS_SCENARIO)-cache-independent: $(BENCHMARK_DIR)/$(STRESS_SCENARIO)-cache-independent-$(REFERENCE_COMBINATION) | $(RESULT_DIR)
	$< >> $(RESULT_DIR)/$(REFERENCE_COMBINATION)-stress.txt

benchmark-5D: benchmark-$(STRESS_SCENARIO)-cache-dependent benchmark-$(STRESS_SCENARIO)-cache-independent

benchmark-grid: benchmark-grid-cache-dependent benchmark-grid-cache-independent

# Cache sweep: build each cache-dependent scenario for each of the 16
# cache combinations; recipe for each combination runs all scenarios in
# order and appends to ${result_dir}/<combination>.txt.  The reference
# combination's binaries are also defined by SCENARIO_RUN_CD_RULE above, so
# we only define the build rules for the OTHER 15 combinations here.
define CACHE_SWEEP_PER_COMBINATION_NON_REFERENCE
$$(foreach s,$$(CACHE_DEPENDENT_SCENARIOS),$$(eval $$(call SCENARIO_BUILD_RULE,$$(s)-cache-dependent,$(1))))
.PHONY: benchmark-cache-sweep-$(1)
benchmark-cache-sweep-$(1): $$(foreach s,$$(CACHE_DEPENDENT_SCENARIOS),$$(BENCHMARK_DIR)/$$(s)-cache-dependent-$(1)) | $$(RESULT_DIR)
	rm -f $$(RESULT_DIR)/$(1).txt
	@for s in $$(CACHE_DEPENDENT_SCENARIOS); do \
	  echo "running $$$$s cache-dependent ($(1))"; \
	  $$(BENCHMARK_DIR)/$$$$s-cache-dependent-$(1) >> $$(RESULT_DIR)/$(1).txt; \
	done
endef
define CACHE_SWEEP_PER_COMBINATION_REFERENCE
.PHONY: benchmark-cache-sweep-$(1)
benchmark-cache-sweep-$(1): $$(foreach s,$$(CACHE_DEPENDENT_SCENARIOS),$$(BENCHMARK_DIR)/$$(s)-cache-dependent-$(1)) | $$(RESULT_DIR)
	rm -f $$(RESULT_DIR)/$(1).txt
	@for s in $$(CACHE_DEPENDENT_SCENARIOS); do \
	  echo "running $$$$s cache-dependent ($(1))"; \
	  $$(BENCHMARK_DIR)/$$$$s-cache-dependent-$(1) >> $$(RESULT_DIR)/$(1).txt; \
	done
endef
$(foreach combination,$(filter-out $(REFERENCE_COMBINATION),$(ALL_COMBINATIONS)),$(eval $(call CACHE_SWEEP_PER_COMBINATION_NON_REFERENCE,$(combination))))
$(eval $(call CACHE_SWEEP_PER_COMBINATION_REFERENCE,$(REFERENCE_COMBINATION)))

benchmark-cache-sweep: $(addprefix benchmark-cache-sweep-,$(ALL_COMBINATIONS))

# Storage benchmark: two combinations, one binary per combination.
$(eval $(call SCENARIO_BUILD_RULE,storage,$(REFERENCE_COMBINATION)))
$(eval $(call SCENARIO_BUILD_RULE,storage,$(BASELINE_COMBINATION)))
.PHONY: benchmark-storage benchmark-storage-$(REFERENCE_COMBINATION) benchmark-storage-$(BASELINE_COMBINATION)
benchmark-storage-$(REFERENCE_COMBINATION): $(BENCHMARK_DIR)/storage-$(REFERENCE_COMBINATION) | $(RESULT_DIR)
	rm -f $(RESULT_DIR)/storage-$(REFERENCE_COMBINATION).txt
	$< > $(RESULT_DIR)/storage-$(REFERENCE_COMBINATION).txt
benchmark-storage-$(BASELINE_COMBINATION): $(BENCHMARK_DIR)/storage-$(BASELINE_COMBINATION) | $(RESULT_DIR)
	rm -f $(RESULT_DIR)/storage-$(BASELINE_COMBINATION).txt
	$< > $(RESULT_DIR)/storage-$(BASELINE_COMBINATION).txt
benchmark-storage: benchmark-storage-$(REFERENCE_COMBINATION) benchmark-storage-$(BASELINE_COMBINATION)

# Pos-mix benchmark: hit-vs-miss try_pos timings.  One binary per
# scenario, all writing the reference combination to a single file.
define SCENARIO_RUN_PM_RULE
$$(eval $$(call SCENARIO_BUILD_RULE,$(1)-pos-mix,$$(REFERENCE_COMBINATION)))
.PHONY: benchmark-$(1)-pos-mix
benchmark-$(1)-pos-mix: $$(BENCHMARK_DIR)/$(1)-pos-mix-$$(REFERENCE_COMBINATION) | $$(RESULT_DIR)
	$$< >> $$(RESULT_DIR)/pos-mix-$$(REFERENCE_COMBINATION).txt
endef
$(foreach s,$(POS_MIX_SCENARIOS),$(eval $(call SCENARIO_RUN_PM_RULE,$(s))))

benchmark-pos-mix: $(foreach s,$(POS_MIX_SCENARIOS),$(BENCHMARK_DIR)/$(s)-pos-mix-$(REFERENCE_COMBINATION)) | $(RESULT_DIR)
	rm -f $(RESULT_DIR)/pos-mix-$(REFERENCE_COMBINATION).txt
	@for s in $(POS_MIX_SCENARIOS); do \
	  echo "running $$s pos-mix"; \
	  $(BENCHMARK_DIR)/$$s-pos-mix-$(REFERENCE_COMBINATION) >> $(RESULT_DIR)/pos-mix-$(REFERENCE_COMBINATION).txt; \
	done

# Top-level benchmark target: full paper-reproduction run.
#
# Truncates aggregate files first, then sequentially runs each
# scenario binary appending to its target file.  Sequencing is
# required because multiple scenarios share an output file.  Builds
# happen first under -j via the prerequisite list.
#
benchmark-build: \
  $(foreach s,$(CACHE_DEPENDENT_SCENARIOS),$(BENCHMARK_DIR)/$(s)-cache-dependent-$(REFERENCE_COMBINATION)) \
  $(foreach s,$(CACHE_INDEPENDENT_SCENARIOS),$(BENCHMARK_DIR)/$(s)-cache-independent-$(REFERENCE_COMBINATION)) \
  $(BENCHMARK_DIR)/$(STRESS_SCENARIO)-cache-dependent-$(REFERENCE_COMBINATION) \
  $(BENCHMARK_DIR)/$(STRESS_SCENARIO)-cache-independent-$(REFERENCE_COMBINATION) \
  $(foreach combination,$(ALL_COMBINATIONS),$(foreach s,$(CACHE_DEPENDENT_SCENARIOS),$(BENCHMARK_DIR)/$(s)-cache-dependent-$(combination))) \
  $(BENCHMARK_DIR)/storage-$(REFERENCE_COMBINATION) \
  $(BENCHMARK_DIR)/storage-$(BASELINE_COMBINATION) \
  $(foreach s,$(POS_MIX_SCENARIOS),$(BENCHMARK_DIR)/$(s)-pos-mix-$(REFERENCE_COMBINATION))

benchmark: benchmark-build | $(RESULT_DIR)
	rm -f $(RESULT_DIR)/$(REFERENCE_COMBINATION).txt
	@for s in $(CACHE_DEPENDENT_SCENARIOS); do \
	  echo "running $$s cache-dependent"; \
	  $(BENCHMARK_DIR)/$$s-cache-dependent-$(REFERENCE_COMBINATION) >> $(RESULT_DIR)/$(REFERENCE_COMBINATION).txt; \
	done
	rm -f $(RESULT_DIR)/cache-independent-$(BASELINE_COMBINATION).txt
	@for s in $(CACHE_INDEPENDENT_SCENARIOS); do \
	  echo "running $$s cache-independent"; \
	  $(BENCHMARK_DIR)/$$s-cache-independent-$(REFERENCE_COMBINATION) >> $(RESULT_DIR)/cache-independent-$(BASELINE_COMBINATION).txt; \
	done
	rm -f $(RESULT_DIR)/$(REFERENCE_COMBINATION)-stress.txt
	@echo "running 5D stress (cache-dependent)"; $(BENCHMARK_DIR)/$(STRESS_SCENARIO)-cache-dependent-$(REFERENCE_COMBINATION) >> $(RESULT_DIR)/$(REFERENCE_COMBINATION)-stress.txt
	@echo "running 5D stress (cache-independent)"; $(BENCHMARK_DIR)/$(STRESS_SCENARIO)-cache-independent-$(REFERENCE_COMBINATION) >> $(RESULT_DIR)/$(REFERENCE_COMBINATION)-stress.txt
	@for combination in $(ALL_COMBINATIONS); do \
	  if [ "$$combination" = "$(REFERENCE_COMBINATION)" ]; then continue; fi; \
	  rm -f $(RESULT_DIR)/$$combination.txt; \
	  for s in $(CACHE_DEPENDENT_SCENARIOS); do \
	    echo "running $$s cache-dependent ($$combination)"; \
	    $(BENCHMARK_DIR)/$$s-cache-dependent-$$combination >> $(RESULT_DIR)/$$combination.txt; \
	  done; \
	done
	$(MAKE) benchmark-storage
	$(MAKE) benchmark-pos-mix
	@echo 'saved results to $(RESULT_DIR)'

# -- plots: tsv extraction then gnuplot -------------------------------

TSVS := $(patsubst %,plot/%.tsv,$(PLOT_NAMES))
PLOTS := $(foreach p,$(PLOT_NAMES),generated/$(p).tex generated/$(p).pdf)

plots: $(PLOTS)

# Compile each extractor on the fly. They share IPTPlot.hpp; touching
# the header rebuilds them all. Multiple extractors build (and run) in
# parallel under `make -j`.
$(EXTRACT_BIN_DIR):
	mkdir -p $@

$(EXTRACT_BIN_DIR)/%: plot/%.cpp plot/IPTPlot.cpp $(EXTRACT_HEADER) | $(EXTRACT_BIN_DIR)
	$(CXX) $(EXTRACT_CXXFLAGS) $(EXTRACT_LDFLAGS) -o $@ $< plot/IPTPlot.cpp

define TSV_RULE
plot/$(1).tsv: $$(EXTRACT_BIN_DIR)/$(1) $$(RESULT_FILES)
	$$< > $$@
endef
$(foreach p,$(PLOT_NAMES),$(eval $(call TSV_RULE,$(p))))

define PLOT_RULE
generated/$(1).tex generated/$(1).pdf &: \
    plot/$(1).gp plot/$(1).tsv | generated
	gnuplot $$<
endef
$(foreach p,$(PLOT_NAMES),$(eval $(call PLOT_RULE,$(p))))

# Stress summary: extractor writes the LaTeX table directly.
generated/stress-summary.tex: $(EXTRACT_BIN_DIR)/stress-summary $(RESULT_FILES) | generated
	$< > $@

PLOTS += generated/stress-summary.tex

generated:
	mkdir -p generated

# -- paper: bibtex + pdflatex ----------------------------------------

IPT.pdf: IPT.tex IPT.bbl $(PLOTS) $(wildcard texmf/*)
	$(TEXENV) pdflatex -halt-on-error IPT.tex
	$(TEXENV) pdflatex -halt-on-error IPT.tex

IPT.bbl: IPT.bib IPT.tex $(PLOTS) texmf/ACM-Reference-Format.bst
	$(TEXENV) pdflatex -halt-on-error IPT.tex
	$(TEXENV) bibtex IPT

# -- clean ------------------------------------------------------------

clean:
	rm -f IPT.bbl IPT.blg IPT.aux IPT.log IPT.out
	rm -rf generated build

distclean: clean
	rm -f IPT.pdf
