// Aggregate persistence/storage measurements across the seven smaller
// structured multiple-survey scenarios on the reference platform.
// Emits a stand-alone LaTeX tabular snippet on stdout summarising per
// algorithm: bytes on disk, cold first-response latency for
// pos/at/select, and steady-state mmap-backed pos/at throughput.

#include "IPTPlot.hpp"

#include <array>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <format>
#include <map>
#include <ranges>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace
{
  auto to_double (std::string_view sv) -> double
  {
    auto value {0.0};
    std::from_chars (sv.data(), sv.data() + sv.size(), value);
    return value;
  }

  auto to_long (std::string_view sv) -> long
  {
    auto value {0L};
    std::from_chars (sv.data(), sv.data() + sv.size(), value);
    return value;
  }

  // Eight storage-capable layouts in the order used in the paper text.
  auto algorithm_order() -> std::vector<std::pair<std::string, std::string>>
  {
    return
      { { "greedy-plus-merge-mmap"     , "\\textsc{IPT}" }
      , { "sorted-points-mmap"         , "\\textsc{SortedPoints}" }
      , { "dense-bitset-mmap"          , "\\textsc{DenseBitset}" }
      , { "block-bitmap-mmap"          , "\\textsc{BlockBitmap}" }
      , { "roaring-mmap"               , "\\textsc{Roaring}" }
      , { "lex-run-mmap"               , "\\textsc{LexRun}" }
      , { "row-csr-k1-mmap"            , "\\textsc{RowCSR-k1}" }
      , { "row-csr-k2-mmap"            , "\\textsc{RowCSR-k$d{-}1$}" }
      };
  }

  // Per-(algorithm, scenario) bag of samples for one (metric, column)
  // pair.  We accumulate the per-seed mean for each scenario and then
  // take the geometric mean across the seven smaller structured
  // multiple-survey scenarios.
  struct Sample
  {
    std::map<std::string, std::vector<double>> per_scenario;

    auto add (std::string const& scenario, double v) -> void
    {
      if (v > 0.0)
      {
        per_scenario[scenario].push_back (v);
      }
    }

    auto geom_across_scenarios() const -> double
    {
      auto means {std::vector<double>{}};
      means.reserve (per_scenario.size());
      std::ranges::for_each
        ( per_scenario
        , [&] (auto const& kv)
          {
            if (!kv.second.empty())
            {
              auto const sum
                {std::ranges::fold_left (kv.second, 0.0, std::plus{})};
              means.push_back (sum / static_cast<double> (kv.second.size()));
            }
          }
        );
      return ipt_plot::geomean (means);
    }
  };

  auto fmt_bytes (double v) -> std::string
  {
    if (!std::isfinite (v) || v <= 0.0)
    {
      return "--";
    }

    char buf[64];
    std::snprintf (buf, sizeof (buf), "%.0f", v);

    auto const digits {std::string {buf}};
    if (digits.size() <= 3)
    {
      return digits;
    }

    auto grouped {std::string {}};
    grouped.reserve (digits.size() + 2 * ((digits.size() - 1) / 3));

    auto first_group_size {digits.size() % 3};
    if (first_group_size == 0)
    {
      first_group_size = 3;
    }

    grouped.append (digits, 0, first_group_size);

    for (auto offset {first_group_size}; offset < digits.size(); offset += 3)
    {
      grouped += "\\,";
      grouped.append (digits, offset, 3);
    }

    return grouped;
  }

  auto fmt_bytes_per_point (double v) -> std::string
  {
    if (!std::isfinite (v) || v <= 0.0)
    {
      return "--";
    }

    char buf[64];
    std::snprintf (buf, sizeof (buf), "%.2f", v);
    return buf;
  }

  auto fmt_us (double ns) -> std::string
  {
    if (!std::isfinite (ns) || ns <= 0.0)
    {
      return "--";
    }

    char buf[64];
    auto const us {ns / 1000.0};
    std::snprintf (buf, sizeof (buf), "%.2f", us);
    return buf;
  }

  auto fmt_rate (double v) -> std::string
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
    if (ratio < 0.1)
    {
      return "0 x";
    }
    if (ratio > 10.0)
    {
      return "$\\infty$ x";
    }

    char buffer[64];
    std::snprintf (buffer, sizeof (buffer), "%.1fx", ratio);
    return buffer;
  }

  auto equal_with_tolerance (double left, double right) -> bool
  {
    if (!std::isfinite (left) || !std::isfinite (right))
    {
      return false;
    }

    auto scale {std::abs (left)};
    if (scale < std::abs (right))
    {
      scale = std::abs (right);
    }
    if (scale < 1.0)
    {
      scale = 1.0;
    }

    return std::abs (left - right) <= scale * 1.0e-9;
  }

  auto bold_if_best (std::string value, bool is_best) -> std::string
  {
    if (!is_best)
    {
      return value;
    }

    return std::format ("\\textbf{{{}}}", value);
  }

  auto is_strictly_smaller (double left, double right) -> bool
  {
    return left < right && !equal_with_tolerance (left, right);
  }

  auto is_strictly_larger (double left, double right) -> bool
  {
    return right < left && !equal_with_tolerance (left, right);
  }

  constexpr auto scenario_order() noexcept -> std::array<std::string_view, 7>
  {
    return
      { "multiple-survey-2-l"
      , "multiple-survey-3-steps"
      , "multiple-survey-4-overlap"
      , "multiple-survey-5-mixed"
      , "multiple-survey-6-bands"
      , "multiple-survey-7-large"
      , "multiple-survey-8-threed"
      };
  }

  auto is_storage_scenario (std::string_view scenario) -> bool
  {
    auto const scenarios {scenario_order()};
    return std::ranges::find (scenarios, scenario) != std::cend (scenarios);
  }
}

auto main() -> int
{
  using namespace ipt_plot;

  auto const default_config {std::string {default_ipt_config()}};
  auto const baseline_config {std::string {reference_cache_config()}};
  auto const platforms {load_platforms()};
  auto const reference {reference_platform_id (platforms)};

  // [config][algorithm][metric] -> Sample
  using AlgMap = std::map<std::string, std::map<std::string, Sample>>;
  auto data {std::map<std::string, AlgMap>{}};

  // Per-algorithm IPT bytes-on-disk for the seven-scenario storage subset,
  // for the side-by-side narrative line.
  auto bytes_per_scenario
    {std::map<std::string, std::map<std::string, long>>{}};

  visit_rows
    ( platforms
    , [&] (auto const& platform, auto const& meta, auto const& row)
      {
        if (meta.cache_kind != "storage")
        {
          return;
        }
        if (platform.id != reference)
        {
          return;
        }
        if (meta.config != default_config && meta.config != baseline_config)
        {
          return;
        }

        auto const scenario {std::string {row.value ("scenario")}};
        if (!is_storage_scenario (scenario))
        {
          return;
        }

        auto const algorithm {std::string {row.value ("algorithm")}};
        auto const metric {std::string {row.value ("metric")}};
        auto& bag {data[meta.config][algorithm][metric]};

        if (metric == "bytes_on_disk")
        {
          auto const v {to_long (row.value ("value"))};
          auto const point_count {to_long (row.value ("point_count"))};
          bag.add (scenario, static_cast<double> (v));
          if (point_count > 0)
          {
            data[meta.config][algorithm]["bytes_per_point"].add
              ( scenario
              , static_cast<double> (v)
                / static_cast<double> (point_count)
              );
          }
          if (meta.config == default_config)
          {
            bytes_per_scenario[algorithm][scenario] = v;
          }
          return;
        }
        if (metric == "first_pos")
        {
          bag.add (scenario, to_double (row.value ("ns_per_pos")));
          return;
        }
        if (metric == "first_at")
        {
          bag.add (scenario, to_double (row.value ("ns_per_at")));
          return;
        }
        if (metric == "first_select")
        {
          bag.add (scenario, to_double (row.value ("ns_per_select")));
          return;
        }
        if (metric == "load")
        {
          bag.add (scenario, to_double (row.value ("ns_per_load")));
          return;
        }
        if (metric == "serialize")
        {
          bag.add (scenario, to_double (row.value ("ns_per_point")));
          return;
        }
        if (metric == "pos_random")
        {
          bag.add (scenario, to_double (row.value ("Mpos_per_sec")));
          return;
        }
        if (metric == "at_random")
        {
          bag.add (scenario, to_double (row.value ("Mat_per_sec")));
          return;
        }
      }
    );

  if (data.empty())
  {
    std::fprintf (stderr, "no storage rows found on reference platform\n");
    return 1;
  }

  std::printf ("%% Auto-generated by plot/storage-summary.cpp.\n");
  std::printf
    ( "%% Reference platform: %s, geom-mean across the seven smaller structured scenarios\n"
    , reference.c_str()
    );
  std::printf ("%% Configuration: %s.\n", default_config.c_str());

  std::printf ("\\begin{tabular}{lrrr*{5}{rr}}\n");
  std::printf ("\\toprule\n");
  std::printf
    ( "Layout & \\multicolumn{3}{c}{bytes}"
      " & \\multicolumn{2}{c}{first \\texttt{pos}}"
      " & \\multicolumn{2}{c}{first \\texttt{at}}"
      " & \\multicolumn{2}{c}{first \\texttt{select}}"
      " & \\multicolumn{2}{c}{\\texttt{pos}}"
      " & \\multicolumn{2}{c}{\\texttt{at}} \\\\\n"
    );
  std::printf
    ( "       & total & /point & ratio"
      " & \\multicolumn{2}{c}{\\(\\mu\\)s}"
      " & \\multicolumn{2}{c}{\\(\\mu\\)s}"
      " & \\multicolumn{2}{c}{\\(\\mu\\)s}"
      " & \\multicolumn{2}{c}{Mpos/s}"
      " & \\multicolumn{2}{c}{Mat/s} \\\\\n"
    );
  std::printf ("\\midrule\n");

  auto const& cfg {data[default_config]};
  auto const get_metric_value
    { [&] (std::string const& algorithm, std::string const& metric) -> double
      {
        auto const algorithm_iterator {cfg.find (algorithm)};
        if (algorithm_iterator == cfg.end())
        {
          return std::nan ("");
        }

        auto const metric_iterator {algorithm_iterator->second.find (metric)};
        if (metric_iterator == algorithm_iterator->second.end())
        {
          return std::nan ("");
        }

        return metric_iterator->second.geom_across_scenarios();
      }
    };

  auto const ipt_algorithm {std::string {"greedy-plus-merge-mmap"}};
  auto const ipt_bytes_on_disk {get_metric_value (ipt_algorithm, "bytes_on_disk")};
  auto const ipt_bytes_per_point
    {get_metric_value (ipt_algorithm, "bytes_per_point")};
  auto const ipt_first_pos {get_metric_value (ipt_algorithm, "first_pos")};
  auto const ipt_first_at {get_metric_value (ipt_algorithm, "first_at")};
  auto const ipt_first_select {get_metric_value (ipt_algorithm, "first_select")};
  auto const ipt_pos_random {get_metric_value (ipt_algorithm, "pos_random")};
  auto const ipt_at_random {get_metric_value (ipt_algorithm, "at_random")};
  auto const ipt_default {ipt_bytes_on_disk};
  auto const ipt_default_bpp {ipt_bytes_per_point};
  auto const ipt_minimal
    {data[baseline_config]["greedy-plus-merge-mmap"]
       ["bytes_on_disk"].geom_across_scenarios()};
  auto const ipt_minimal_bpp
    {data[baseline_config]["greedy-plus-merge-mmap"]
       ["bytes_per_point"].geom_across_scenarios()};

  if ( !std::isfinite (ipt_bytes_on_disk)
    || !std::isfinite (ipt_bytes_per_point)
    || !std::isfinite (ipt_first_pos)
    || !std::isfinite (ipt_first_at)
    || !std::isfinite (ipt_first_select)
    || !std::isfinite (ipt_pos_random)
    || !std::isfinite (ipt_at_random)
     )
  {
    std::fprintf (stderr, "missing IPT baseline row in storage summary\n");
    return 1;
  }

  auto best_bytes_on_disk {std::nan ("")};
  auto best_bytes_per_point {std::nan ("")};
  auto best_first_pos {std::nan ("")};
  auto best_first_at {std::nan ("")};
  auto best_first_select {std::nan ("")};
  auto best_pos_random {std::nan ("")};
  auto best_at_random {std::nan ("")};

  auto update_smaller
    { [&] (double& best, double value) -> void
      {
        if (!std::isfinite (value))
        {
          return;
        }
        if (!std::isfinite (best) || value < best)
        {
          best = value;
        }
      }
    };

  auto update_larger
    { [&] (double& best, double value) -> void
      {
        if (!std::isfinite (value))
        {
          return;
        }
        if (!std::isfinite (best) || best < value)
        {
          best = value;
        }
      }
    };

  for (auto const& [algorithm, label] : algorithm_order())
  {
    update_smaller (best_bytes_on_disk, get_metric_value (algorithm, "bytes_on_disk"));
    update_smaller (best_bytes_per_point, get_metric_value (algorithm, "bytes_per_point"));
    update_smaller (best_first_pos, get_metric_value (algorithm, "first_pos"));
    update_smaller (best_first_at, get_metric_value (algorithm, "first_at"));
    update_smaller (best_first_select, get_metric_value (algorithm, "first_select"));
    update_larger (best_pos_random, get_metric_value (algorithm, "pos_random"));
    update_larger (best_at_random, get_metric_value (algorithm, "at_random"));
  }

  update_smaller (best_bytes_on_disk, ipt_minimal);
  update_smaller (best_bytes_per_point, ipt_minimal_bpp);

  auto any_better_bytes_on_disk {false};
  auto any_better_first_pos {false};
  auto any_better_first_at {false};
  auto any_better_first_select {false};
  auto any_better_pos_random {false};
  auto any_better_at_random {false};

  for (auto const& [algorithm, label] : algorithm_order())
  {
    if (algorithm == ipt_algorithm)
    {
      continue;
    }

    auto const bytes_on_disk {get_metric_value (algorithm, "bytes_on_disk")};
    if (!std::isfinite (bytes_on_disk))
    {
      continue;
    }

    auto const first_pos {get_metric_value (algorithm, "first_pos")};
    auto const first_at {get_metric_value (algorithm, "first_at")};
    auto const first_select {get_metric_value (algorithm, "first_select")};
    auto const pos_random {get_metric_value (algorithm, "pos_random")};
    auto const at_random {get_metric_value (algorithm, "at_random")};

    any_better_bytes_on_disk
      = any_better_bytes_on_disk
     || is_strictly_smaller (bytes_on_disk, ipt_bytes_on_disk);
    any_better_first_pos
      = any_better_first_pos
     || is_strictly_smaller (first_pos, ipt_first_pos);
    any_better_first_at
      = any_better_first_at
     || is_strictly_smaller (first_at, ipt_first_at);
    any_better_first_select
      = any_better_first_select
     || is_strictly_smaller (first_select, ipt_first_select);
    any_better_pos_random
      = any_better_pos_random
     || is_strictly_larger (pos_random, ipt_pos_random);
    any_better_at_random
      = any_better_at_random
     || is_strictly_larger (at_random, ipt_at_random);
  }

  for (auto const& [algo, label] : algorithm_order())
  {
    auto const bytes_on_disk {get_metric_value (algo, "bytes_on_disk")};
    if (!std::isfinite (bytes_on_disk))
    {
      continue;
    }

    auto const bytes_per_point {get_metric_value (algo, "bytes_per_point")};
    auto const first_pos {get_metric_value (algo, "first_pos")};
    auto const first_at {get_metric_value (algo, "first_at")};
    auto const first_select {get_metric_value (algo, "first_select")};
    auto const pos_random {get_metric_value (algo, "pos_random")};
    auto const at_random {get_metric_value (algo, "at_random")};
    auto const bytes_on_disk_text
      {bold_if_best
          ( fmt_bytes (bytes_on_disk)
          , equal_with_tolerance (bytes_on_disk, best_bytes_on_disk)
          )};
    auto const bytes_per_point_text
      {bold_if_best
          ( fmt_bytes_per_point (bytes_per_point)
          , equal_with_tolerance (bytes_per_point, best_bytes_per_point)
          )};
    auto const first_pos_text
      {bold_if_best
          ( fmt_us (first_pos)
          , equal_with_tolerance (first_pos, best_first_pos)
          )};
    auto const first_at_text
      {bold_if_best
          ( fmt_us (first_at)
          , equal_with_tolerance (first_at, best_first_at)
          )};
    auto const first_select_text
      {bold_if_best
          ( fmt_us (first_select)
          , equal_with_tolerance (first_select, best_first_select)
          )};
    auto const pos_random_text
      {bold_if_best
          ( fmt_rate (pos_random)
          , equal_with_tolerance (pos_random, best_pos_random)
          )};
    auto const at_random_text
      {bold_if_best
          ( fmt_rate (at_random)
          , equal_with_tolerance (at_random, best_at_random)
          )};
    auto const bytes_ratio_text
      {bold_if_best
          ( fmt_ratio (bytes_on_disk, ipt_bytes_on_disk)
          , algo == ipt_algorithm
            ? !any_better_bytes_on_disk
            : is_strictly_smaller (bytes_on_disk, ipt_bytes_on_disk)
          )};
    auto const first_pos_ratio_text
      {bold_if_best
          ( fmt_ratio (first_pos, ipt_first_pos)
          , algo == ipt_algorithm
            ? !any_better_first_pos
            : is_strictly_smaller (first_pos, ipt_first_pos)
          )};
    auto const first_at_ratio_text
      {bold_if_best
          ( fmt_ratio (first_at, ipt_first_at)
          , algo == ipt_algorithm
            ? !any_better_first_at
            : is_strictly_smaller (first_at, ipt_first_at)
          )};
    auto const first_select_ratio_text
      {bold_if_best
          ( fmt_ratio (first_select, ipt_first_select)
          , algo == ipt_algorithm
            ? !any_better_first_select
            : is_strictly_smaller (first_select, ipt_first_select)
          )};
    auto const pos_random_ratio_text
      {bold_if_best
          ( fmt_ratio (pos_random, ipt_pos_random)
          , algo == ipt_algorithm
            ? !any_better_pos_random
            : is_strictly_larger (pos_random, ipt_pos_random)
          )};
    auto const at_random_ratio_text
      {bold_if_best
          ( fmt_ratio (at_random, ipt_at_random)
          , algo == ipt_algorithm
            ? !any_better_at_random
            : is_strictly_larger (at_random, ipt_at_random)
          )};

    std::printf
      ( "%s & %s & %s & %s & %s & %s & %s"
        " & %s & %s & %s & %s & %s & %s & %s \\\\\n"
      , label.c_str()
      , bytes_on_disk_text.c_str()
      , bytes_per_point_text.c_str()
      , bytes_ratio_text.c_str()
      , first_pos_text.c_str()
      , first_pos_ratio_text.c_str()
      , first_at_text.c_str()
      , first_at_ratio_text.c_str()
      , first_select_text.c_str()
      , first_select_ratio_text.c_str()
      , pos_random_text.c_str()
      , pos_random_ratio_text.c_str()
      , at_random_text.c_str()
      , at_random_ratio_text.c_str()
      );

    if (algo == ipt_algorithm)
    {
      std::printf ("\\midrule\n");
    }
  }

  // Footnote line: minimal IPT footprint with c0r0e0l0.
  if ( std::isfinite (ipt_default)
    && std::isfinite (ipt_default_bpp)
    && std::isfinite (ipt_minimal)
    && std::isfinite (ipt_minimal_bpp)
     )
  {
    auto const ipt_minimal_text
      {bold_if_best
          ( fmt_bytes (ipt_minimal)
          , equal_with_tolerance (ipt_minimal, best_bytes_on_disk)
          )};
    auto const ipt_minimal_bpp_text
      {bold_if_best
          ( fmt_bytes_per_point (ipt_minimal_bpp)
          , equal_with_tolerance (ipt_minimal_bpp, best_bytes_per_point)
          )};

    std::printf ("\\midrule\n");
    std::printf
      ( "\\textsc{IPT} (\\texttt{c0r0e0l0}) & %s & %s & %s"
        " & \\multicolumn{10}{c}{} \\\\\n"
      , ipt_minimal_text.c_str()
      , ipt_minimal_bpp_text.c_str()
      , fmt_ratio (ipt_minimal, ipt_default).c_str()
      );
  }

  std::printf ("\\bottomrule\n");
  std::printf ("\\end{tabular}\n");

  return 0;
}
