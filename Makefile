CXX           ?= g++
JOBS          ?= $(shell nproc)

EXTRACT_CXXFLAGS ?= -std=c++23 -O2 -Wall -Wextra
EXTRACT_LDFLAGS  ?=

TEXENV  = TEXINPUTS=".:./texmf/:" \
          BSTINPUTS=".:./texmf/:"

# -- source file lists ------------------------------------------------

PLOT_NAMES :=
PLOT_NAMES += at-cache-effect
PLOT_NAMES += at-multiple-survey
PLOT_NAMES += construct-multiple-survey
PLOT_NAMES += construct-vs-density
PLOT_NAMES += kappa-vs-density
PLOT_NAMES += pos-multiple-survey
PLOT_NAMES += pos-ratio-vs-density

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

.PHONY: paper verify benchmark benchmark-stress plots clean distclean help

help:
	@echo 'Available targets:'
	@echo '  make paper            Build IPT.pdf.'
	@echo '  make verify           Run reduced verification for all 16 cache combinations.'
	@echo '  make benchmark        Run the core benchmark suite (includes select/restrict).'
	@echo '  make benchmark-stress Run the opt-in large structured stress scenarios.'
	@echo '  make clean            Remove intermediate files and generated/.'
	@echo '  make distclean        Run clean and remove IPT.pdf.'
	@echo '  make help             Show this help message.'

paper: IPT.pdf

benchmark:
	CXX="$(CXX)" bash run/benchmarks.sh

benchmark-stress:
	CXX="$(CXX)" bash run/benchmark-stress.sh

verify:
	CXX="$(CXX)" JOBS="$(JOBS)" bash run/verify-combinations.sh

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
	rm -f $(TSVS)
	rm -rf generated

distclean: clean
	rm -f IPT.pdf
