// Aggregate native restrict-time ratios relative to IPT across every
// platform that supplied a restrict run. Emits a stand-alone LaTeX
// tabular that mirrors the Table 14 layout: one column for the
// structured 3D aggregate and one column for the 5D stress aggregate,
// each holding the IPT-relative geometric-mean restrict-time ratio of
// the applicable baselines.
//
// Structured 3D: geometric mean over the four structured restriction
// scenarios, the 27 steady-state restriction specifications, and every
// platform/compiler run that supplied the structured restrict files.
// 5D stress: geometric mean over the multiple-survey-9-5d scenario, the
// 19 steady-state distribution specifications, and every platform that
// supplied the 5D restrict file.
//
// Each matched cell is a (platform, scenario, specification) triple for
// which both IPT (greedy-plus-merge) and the baseline reported a
// steady-state ns/restrict value. Because the geometric mean is
// multiplicative, the geometric mean of the per-cell baseline/IPT
// ratios equals the ratio of the per-layout geometric-mean restrict
// times over the same matched cells.

#include "IPTPlot.hpp"

#include <array>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace
{
  struct LayoutSpec
  {
    std::string_view algorithm;
    std::string_view label;
  };

  // Row order of the restriction summary table. RowCSR-k2 is structured
  // only, RowCSR-k4 is 5D only, and the occupancy-bitmap layouts are
  // structured only, so the corresponding cells in the other column
  // stay blank.
  constexpr auto layout_order() noexcept -> std::array<LayoutSpec, 8>
  {
    return
      { LayoutSpec {"row-csr-k1", "\\textsc{RowCSR-k1}"}
      , LayoutSpec {"row-csr-k2", "\\textsc{RowCSR-k2}"}
      , LayoutSpec {"row-csr-k4", "\\textsc{RowCSR-k4}"}
      , LayoutSpec {"sorted-points", "\\textsc{SortedPoints}"}
      , LayoutSpec {"lex-run", "\\textsc{LexRun}"}
      , LayoutSpec {"block-bitmap", "\\textsc{BlockBitmap}"}
      , LayoutSpec {"dense-bitset", "\\textsc{DenseBitset}"}
      , LayoutSpec {"roaring", "\\textsc{Roaring}"}
      };
  }

  constexpr auto ipt_algorithm() noexcept -> std::string_view
  {
    return "greedy-plus-merge";
  }

  constexpr auto stress_scenario() noexcept -> std::string_view
  {
    return "multiple-survey-9-5d";
  }

  auto to_double (std::string_view sv) -> double
  {
    auto value {0.0};
    std::from_chars (sv.data(), sv.data() + sv.size(), value);
    return value;
  }

  // A steady-state restriction specification is a metric named
  // restrict_<spec> that carries an ns_per_restrict measurement and is
  // neither the first-response nor the full-materialization variant.
  auto is_steady_state_restrict
    ( std::string_view metric
    , std::string_view ns_per_restrict
    ) -> bool
  {
    return metric.starts_with ("restrict_")
        && metric.find ("first_response") == std::string_view::npos
        && metric.find ("full_materialization") == std::string_view::npos
        && !ns_per_restrict.empty()
        ;
  }

  // Per-(column, platform, scenario, specification) cell. Maps each
  // algorithm to its measured steady-state restrict times so that a
  // baseline/IPT ratio can be formed only when both operands exist.
  using CellKey = std::tuple<std::string, std::string, std::string>;

  struct RatioBag
  {
    std::map<std::string, std::vector<double>> ratios_by_layout;

    auto add (std::string const& layout, double ratio) -> void
    {
      if (ratio > 0.0 && std::isfinite (ratio))
      {
        ratios_by_layout[layout].push_back (ratio);
      }
    }

    auto geomean (std::string const& layout) const -> double
    {
      auto const it {ratios_by_layout.find (layout)};
      if (it == std::cend (ratios_by_layout) || it->second.empty())
      {
        return std::nan ("");
      }

      return ipt_plot::geomean (it->second);
    }
  };

  auto fmt_ratio (double ratio) -> std::string
  {
    if (!std::isfinite (ratio) || ratio <= 0.0)
    {
      return "";
    }

    char buffer[64];
    if (ratio < 100.0)
    {
      std::snprintf (buffer, sizeof (buffer), "$%.2f\\times$", ratio);
    }
    else
    {
      std::snprintf (buffer, sizeof (buffer), "$%.0f\\times$", ratio);
    }
    return buffer;
  }
}

auto main() -> int
{
  using namespace ipt_plot;

  auto const structured_scenarios
    { std::set<std::string>
      { "multiple-survey-2-l"
      , "multiple-survey-4-overlap"
      , "multiple-survey-5-mixed"
      , "multiple-survey-8-threed"
      }
    };

  // column 0: structured 3D, column 1: 5D stress.
  auto cells {std::array<std::map<CellKey, std::map<std::string, std::vector<double>>>, 2>{}};
  auto structured_platforms {std::set<std::string>{}};
  auto stress_platforms {std::set<std::string>{}};
  auto structured_specs {std::set<std::string>{}};
  auto stress_specs {std::set<std::string>{}};

  visit_rows
    ( [&] (auto const& platform, auto const& meta, auto const& row)
      {
        auto const metric {row.value ("metric")};
        auto const ns_per_restrict {row.value ("ns_per_restrict")};

        if (!is_steady_state_restrict (metric, ns_per_restrict))
        {
          return;
        }

        auto const scenario {std::string {row.value ("scenario")}};
        auto const algorithm {std::string {row.value ("algorithm")}};
        auto const ns {to_double (ns_per_restrict)};

        if (ns <= 0.0)
        {
          return;
        }

        auto const spec {std::string {metric}};
        auto const key {CellKey {platform.id, scenario, spec}};

        if (scenario == std::string {stress_scenario()})
        {
          cells[1][key][algorithm].push_back (ns);
          stress_platforms.insert (platform.id);
          stress_specs.insert (spec);
        }
        else if (structured_scenarios.contains (scenario))
        {
          cells[0][key][algorithm].push_back (ns);
          structured_platforms.insert (platform.id);
          structured_specs.insert (spec);
        }

        // The meta argument is unused: the steady-state restrict rows
        // are uniquely identified by metric, scenario, and algorithm.
        std::ignore = meta;
      }
    );

  auto bags {std::array<RatioBag, 2>{}};
  auto matched_cells {std::array<std::size_t, 2>{}};

  for (auto column {std::size_t {0}}; column < cells.size(); ++column)
  {
    for (auto const& [key, by_algorithm] : cells[column])
    {
      auto const ipt_it {by_algorithm.find (std::string {ipt_algorithm()})};
      if (ipt_it == std::cend (by_algorithm))
      {
        continue;
      }

      auto const ipt_ns {mean (ipt_it->second)};
      if (!std::isfinite (ipt_ns) || ipt_ns <= 0.0)
      {
        continue;
      }

      auto counted {false};
      for (auto const& [algorithm, values] : by_algorithm)
      {
        if (algorithm == std::string {ipt_algorithm()})
        {
          continue;
        }

        auto const baseline_ns {mean (values)};
        if (std::isfinite (baseline_ns) && baseline_ns > 0.0)
        {
          bags[column].add (algorithm, baseline_ns / ipt_ns);
          counted = true;
        }
      }

      if (counted)
      {
        ++matched_cells[column];
      }
    }
  }

  if (structured_platforms.empty() && stress_platforms.empty())
  {
    std::fprintf (stderr, "no restrict data found\n");
    return 1;
  }

  std::printf
    ("%% Auto-generated by results/plot/restrict-summary.cpp.\n");
  std::printf
    ( "%% Structured 3D: %zu scenarios x %zu specs x %zu platforms"
      " (%zu matched cells).\n"
    , structured_scenarios.size()
    , structured_specs.size()
    , structured_platforms.size()
    , matched_cells[0]
    );
  std::printf
    ( "%% 5D stress: %s, %zu specs x %zu platforms (%zu matched cells).\n"
    , std::string {stress_scenario()}.c_str()
    , stress_specs.size()
    , stress_platforms.size()
    , matched_cells[1]
    );
  std::printf ("%% Ratios are geometric-mean baseline/IPT restrict time.\n");
  std::printf ("\\begin{tabular}{lrr}\n");
  std::printf ("\\toprule\n");
  std::printf ("Layout & Structured 3D & 5D stress \\\\\n");
  std::printf ("\\midrule\n");

  for (auto const& layout : layout_order())
  {
    auto const algorithm {std::string {layout.algorithm}};
    auto const structured {fmt_ratio (bags[0].geomean (algorithm))};
    auto const stress {fmt_ratio (bags[1].geomean (algorithm))};

    std::printf
      ( "%s & %s & %s \\\\\n"
      , std::string {layout.label}.c_str()
      , structured.c_str()
      , stress.c_str()
      );
  }

  std::printf ("\\bottomrule\n");
  std::printf ("\\end{tabular}\n");

  return 0;
}
