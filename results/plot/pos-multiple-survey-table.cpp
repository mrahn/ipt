// Aggregate per-scenario pos_random ns/pos for the multiple-survey
// scenarios on the reference platform and default cache
// configuration, and emit a stand-alone LaTeX table with one row per
// algorithm (IPT first, absolute ns/pos) and one column per scenario
// (other rows are ratios relative to IPT).

#include "IPTPlot.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <format>
#include <map>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace
{
  constexpr auto dispersion_threshold {0.20};

  struct Algorithm
  {
    std::string_view name;
    std::string label;
  };

  auto algorithm_rows() -> std::vector<Algorithm>
  {
    return
      { { "greedy-plus-merge"
        , std::format
            ( "\\textsc{{IPT}} (\\texttt{{{}}})"
            , ipt_plot::default_ipt_config()
            )
        }
      , {"sorted-points", "\\textsc{SortedPoints}"}
      , {"dense-bitset",  "\\textsc{DenseBitset}"}
      , {"block-bitmap",  "\\textsc{BlockBitmap}"}
      , {"roaring",       "\\textsc{Roaring}"}
      , {"lex-run",       "\\textsc{LexRun}"}
      , {"row-csr-k1",    "\\textsc{RowCSR-k1}"}
      , {"row-csr-k2",    "\\textsc{RowCSR-k2}"}
      };
  }

  auto fmt_ns (double v) -> std::string
  {
    if (!std::isfinite (v) || v <= 0.0)
    {
      return "--";
    }

    char buf[64];
    std::snprintf (buf, sizeof (buf), "%.2f", v);
    return buf;
  }

  auto fmt_ratio (double value, double ipt_value) -> std::string
  {
    if ( !std::isfinite (value)
      || value <= 0.0
      || !std::isfinite (ipt_value)
      || ipt_value <= 0.0
       )
    {
      return "--";
    }

    auto const ratio {value / ipt_value};
    char buf[64];

    if (ratio < 0.005)
    {
      std::snprintf (buf, sizeof (buf), "%.3fx", ratio);
    }
    else if (ratio < 0.05)
    {
      std::snprintf (buf, sizeof (buf), "%.2fx", ratio);
    }
    else if (0.95 < ratio && ratio < 1.05)
    {
      std::snprintf (buf, sizeof (buf), "%.2fx", ratio);
    }
    else if (ratio < 10.0)
    {
      std::snprintf (buf, sizeof (buf), "%.1fx", ratio);
    }
    else
    {
      std::snprintf (buf, sizeof (buf), "%.0fx", ratio);
    }
    return buf;
  }

  auto bold (std::string text) -> std::string
  {
    return std::format ("\\textbf{{{}}}", text);
  }

  auto grey (std::string text) -> std::string
  {
    return std::format ("\\textcolor[gray]{{0.55}}{{{}}}", text);
  }

  auto with_dagger (std::string text) -> std::string
  {
    return "$^{\\dagger}$" + text;
  }

  auto cell_spread (std::vector<double> const& values) -> double
  {
    if (values.size() < 2)
    {
      return 0.0;
    }

    auto const average {ipt_plot::mean (values)};
    if (!std::isfinite (average) || average <= 0.0)
    {
      return 0.0;
    }

    auto const low {ipt_plot::percentile (0.10, values)};
    auto const high {ipt_plot::percentile (0.90, values)};
    return (high - low) / average;
  }
}

auto main() -> int
{
  using namespace ipt_plot;

  auto const platform_id {reference_platform_id (load_platforms())};
  auto const default_config {std::string {default_ipt_config()}};
  auto const algorithms {algorithm_rows()};

  // scenario -> algorithm -> mean ns
  auto values {std::map<std::string, std::map<std::string, double>>{}};
  auto spreads {std::map<std::string, std::map<std::string, double>>{}};
  auto buckets
    {std::map<std::string, std::map<std::string, std::vector<double>>>{}};

  visit_rows
    ( [&] (auto const& platform, auto const& meta, auto const& row)
      {
        if ( platform.id != platform_id
          || row.value ("metric") != "pos_random"
           )
        {
          return;
        }

        auto const scenario {std::string {row.value ("scenario")}};
        if (!is_multiple_survey (scenario))
        {
          return;
        }

        auto const algorithm {std::string {row.value ("algorithm")}};

        if ( algorithm == "greedy-plus-merge"
          && meta.cache_kind == "cache-dependent"
          && meta.config == default_config
           )
        {
          buckets[scenario][algorithm].push_back (row.measured_value());
          return;
        }

        if (meta.cache_kind != "cache-independent")
        {
          return;
        }

        auto const known
          { std::ranges::any_of
              ( algorithms
              , [&] (Algorithm const& a) noexcept
                {
                  return a.name == algorithm;
                }
              )
          };
        if (known && algorithm != "greedy-plus-merge")
        {
          buckets[scenario][algorithm].push_back (row.measured_value());
        }
      }
    );

  std::ranges::for_each
    ( buckets
    , [&] (auto const& by_scenario)
      {
        std::ranges::for_each
          ( by_scenario.second
          , [&] (auto const& by_alg)
            {
              values[by_scenario.first][by_alg.first] = mean (by_alg.second);
              spreads[by_scenario.first][by_alg.first]
                = cell_spread (by_alg.second);
            }
          );
      }
    );

  auto const scenarios {structured_scenarios()};
  // Keep only the multiple-survey scenarios that actually carry data.
  auto kept_scenarios {std::vector<std::string>{}};
  std::ranges::copy_if
    ( scenarios
    , std::back_inserter (kept_scenarios)
    , [&] (auto const& s)
      {
        if (!is_multiple_survey (s))
        {
          return false;
        }
        auto const v {value_ptr (values, s)};
        return v != nullptr && value_ptr (*v, std::string {"greedy-plus-merge"});
      }
    );

  std::printf ("%% Auto-generated by results/plot/pos-multiple-survey-table.cpp.\n");
  std::printf ("%% Reference platform: %s.\n", platform_id.c_str());
  std::printf ("%% Default IPT configuration: %s.\n", default_config.c_str());
  std::printf
    ( "%% Dispersion dagger threshold: (p90-p10)/mean > %.2f.\n"
    , dispersion_threshold
    );

  // Column spec: l (algorithm) + l (unit) + N r (per scenario).
  std::printf ("{\\footnotesize\n");
  std::printf ("\\begin{tabular}{ll");
  for (auto i {std::size_t {0}}; i < kept_scenarios.size(); ++i)
  {
    std::printf ("r");
  }
  std::printf ("}\n");
  std::printf ("\\toprule\n");
  std::printf ("Scenario &");
  std::ranges::for_each
    ( kept_scenarios
    , [&] (auto const& s)
      {
        auto const label {structured_scenario_label (s)};
        std::printf (" & \\texttt{%s}", label.c_str());
      }
    );
  std::printf (" \\\\\n");
  std::printf ("\\midrule\n");

  // IPT rows: absolute ns/pos and the equivalent throughput Mpos/s.
  auto const& ipt {algorithms.front()};
  std::printf ("%s & ns/\\texttt{pos}", ipt.label.c_str());
  std::ranges::for_each
    ( kept_scenarios
    , [&] (auto const& s)
      {
        auto const& by_alg {values.at (s)};
        auto const it {by_alg.find (std::string {ipt.name})};
        if (it == std::cend (by_alg) || !std::isfinite (it->second))
        {
          std::printf (" & --");
        }
        else
        {
          auto text {fmt_ns (it->second)};
          if (spreads.at (s).at (std::string {ipt.name})
              > dispersion_threshold)
          {
            text = with_dagger (text);
          }
          std::printf (" & %s", text.c_str());
        }
      }
    );
  std::printf (" \\\\\n");
  std::printf (" & Mpos/s");
  std::ranges::for_each
    ( kept_scenarios
    , [&] (auto const& s)
      {
        auto const& by_alg {values.at (s)};
        auto const it {by_alg.find (std::string {ipt.name})};
        if (it == std::cend (by_alg) || !std::isfinite (it->second) || it->second <= 0.0)
        {
          std::printf (" & --");
        }
        else
        {
          char buf[64];
          std::snprintf (buf, sizeof (buf), "%.1f", 1000.0 / it->second);
          auto text {std::string {buf}};
          if (spreads.at (s).at (std::string {ipt.name})
              > dispersion_threshold)
          {
            text = with_dagger (text);
          }
          std::printf (" & %s", text.c_str());
        }
      }
    );
  std::printf (" \\\\\n");
  std::printf ("\\midrule\n");

  // Baseline rows (ratio cells).
  for (auto i {std::size_t {1}}; i < algorithms.size(); ++i)
  {
    auto const& a {algorithms[i]};
    auto cells {std::vector<std::string>{}};
    auto has_bold {false};

    std::ranges::for_each
      ( kept_scenarios
      , [&] (auto const& s)
        {
          auto const& by_alg {values.at (s)};
          auto const ipt_it
            {by_alg.find (std::string {ipt.name})};
          auto const it {by_alg.find (std::string {a.name})};

          if ( ipt_it == std::cend (by_alg)
            || it == std::cend (by_alg)
            || !std::isfinite (ipt_it->second)
            || !std::isfinite (it->second)
             )
          {
            cells.push_back ("--");
            return;
          }

          auto const v {it->second};
          auto const ipt_v {ipt_it->second};
          auto text {fmt_ratio (v, ipt_v)};
          if (v < ipt_v && v / ipt_v < 0.995)
          {
            text = bold (text);
            has_bold = true;
          }

          auto const& scenario_spreads {spreads.at (s)};
          auto const ipt_spread
            {scenario_spreads.at (std::string {ipt.name})};
          auto const candidate_spread
            {scenario_spreads.at (std::string {a.name})};
          if ( ipt_spread > dispersion_threshold
            || candidate_spread > dispersion_threshold
             )
          {
            text = with_dagger (text);
          }
          cells.push_back (text);
        }
      );

    auto label_text {a.label};
    auto cell_texts {cells};
    if (!has_bold)
    {
      label_text = grey (label_text);
      std::ranges::transform
        ( cell_texts
        , std::begin (cell_texts)
        , [] (auto const& t) { return grey (t); }
        );
    }

    std::printf ("%s &", label_text.c_str());
    std::ranges::for_each
      ( cell_texts
      , [] (auto const& t) { std::printf (" & %s", t.c_str()); }
      );
    std::printf (" \\\\\n");
  }

  std::printf ("\\bottomrule\n");
  std::printf ("\\end{tabular}\n");
  std::printf ("}\n");

  return 0;
}
