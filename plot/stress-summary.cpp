// Aggregate the 5D multiple-survey stress scenario across every
// platform that supplied a stress run.  Emits a stand-alone LaTeX
// tabular snippet on stdout.  The structural metrics (cuboid count,
// IPT byte size) are platform-independent; the timing metrics are
// summarised by their geometric mean across platforms with the 10th
// and 90th percentiles in parentheses.

#include "IPTPlot.hpp"

#include <array>
#include <charconv>
#include <cstdio>
#include <map>
#include <ranges>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace
{
  constexpr auto scenario_name() noexcept -> std::string_view
  {
    return "multiple-survey-9-5d";
  }

  auto to_double (std::string_view sv) -> double
  {
    auto value {0.0};
    std::from_chars (sv.data(), sv.data() + sv.size(), value);
    return value;
  }

  // Rate samples (Mpoint/s, Mpos/s, Mat/s) for which the geometric mean
  // is the natural across-platform aggregate.
  struct RateSamples
  {
    std::vector<double> values;

    auto add (double v) -> void
    {
      if (v > 0.0)
      {
        values.push_back (v);
      }
    }
  };

  // Latency samples (ns/op) for selection/restriction queries.
  struct LatencySamples
  {
    std::vector<double> values;

    auto add (double v) -> void
    {
      if (v > 0.0)
      {
        values.push_back (v);
      }
    }
  };

  auto fmt_rate (RateSamples const& s) -> std::string
  {
    if (s.values.empty())
    {
      return "--";
    }

    auto const center {ipt_plot::geomean (s.values)};
    auto const lo {ipt_plot::percentile (0.10, s.values)};
    auto const hi {ipt_plot::percentile (0.90, s.values)};
    char buf[128];
    std::snprintf
      (buf, sizeof (buf), "%.1f \\,(%.1f--%.1f)", center, lo, hi);
    return buf;
  }

  auto fmt_latency_us (LatencySamples const& s) -> std::string
  {
    if (s.values.empty())
    {
      return "--";
    }

    auto scaled {std::vector<double>{}};
    scaled.reserve (s.values.size());
    std::ranges::transform
      ( s.values
      , std::back_inserter (scaled)
      , [] (auto v) { return v / 1000.0; }
      );

    auto const center {ipt_plot::geomean (scaled)};
    auto const lo {ipt_plot::percentile (0.10, scaled)};
    auto const hi {ipt_plot::percentile (0.90, scaled)};
    char buf[128];
    std::snprintf
      (buf, sizeof (buf), "%.1f \\,(%.1f--%.1f)", center, lo, hi);
    return buf;
  }
}

auto main() -> int
{
  using namespace ipt_plot;

  auto const default_config {std::string {default_ipt_config()}};
  auto const target_scenario {std::string {scenario_name()}};

  // [algorithm][metric] -> samples
  auto rates {std::map<std::string, std::map<std::string, RateSamples>>{}};
  auto latencies
    {std::map<std::string, std::map<std::string, LatencySamples>>{}};
  auto cuboid_counts {std::map<std::string, long>{}};
  auto ipt_bytes_values {std::map<std::string, long>{}};
  auto platform_ids {std::set<std::string>{}};
  auto point_count {0L};
  auto grid_label {std::string{}};

  visit_rows
    ( [&] (auto const& platform, auto const& meta, auto const& row)
      {
        if ( meta.cache_kind != "stress"
           || meta.config != default_config
           || row.value ("scenario") != target_scenario
           )
        {
          return;
        }

        auto const algorithm {std::string {row.value ("algorithm")}};
        auto const metric {std::string {row.value ("metric")}};

        if (point_count == 0)
        {
          auto const pc_sv {row.value ("point_count")};
          std::from_chars
            (pc_sv.data(), pc_sv.data() + pc_sv.size(), point_count);
        }

        if (grid_label.empty())
        {
          grid_label = std::string {row.value ("grid")};
        }

        platform_ids.insert (platform.id);

        if (metric == "construct")
        {
          rates[algorithm][metric].add
            (to_double (row.value ("Mpoint_per_sec")));
          return;
        }

        if (metric == "pos_random" || metric == "pos_all")
        {
          rates[algorithm][metric].add
            (to_double (row.value ("Mpos_per_sec")));
          return;
        }

        if (metric == "at_random" || metric == "at_all")
        {
          rates[algorithm][metric].add
            (to_double (row.value ("Mat_per_sec")));
          return;
        }

        if (  metric == "select_d0001"
           || metric == "select_d001"
           || metric == "select_d01"
           || metric == "select_d1"
           )
        {
          latencies[algorithm][metric].add
            (to_double (row.value ("ns_per_select")));
          return;
        }

        if (  metric == "restrict_d0001"
           || metric == "restrict_d001"
           || metric == "restrict_d01"
           || metric == "restrict_d1"
           )
        {
          latencies[algorithm][metric].add
            (to_double (row.value ("ns_per_restrict")));
          return;
        }

        if (metric == "cuboids")
        {
          auto const v_sv {row.value ("value")};
          auto v {0L};
          std::from_chars (v_sv.data(), v_sv.data() + v_sv.size(), v);
          cuboid_counts[algorithm] = v;
          return;
        }

        if (metric == "ipt_bytes")
        {
          auto const v_sv {row.value ("value")};
          auto v {0L};
          std::from_chars (v_sv.data(), v_sv.data() + v_sv.size(), v);
          ipt_bytes_values[algorithm] = v;
          return;
        }
      }
    );

  if (platform_ids.empty())
  {
    std::fprintf
      ( stderr
      , "no stress data found for scenario %s\n"
      , target_scenario.c_str()
      );
    return 1;
  }

  auto const& algo {std::string {"greedy-plus-merge"}};

  std::printf ("%% Auto-generated by plot/stress-summary.cpp.\n");
  std::printf
    ( "%% scenario %s, grid %s, %ld points, %zu platforms.\n"
    , target_scenario.c_str()
    , grid_label.c_str()
    , point_count
    , platform_ids.size()
    );
  std::printf ("\\begin{tabular}{lr}\n");
  std::printf ("\\toprule\n");
  std::printf ("Metric & Geomean across %zu platforms (10th--90th pct.) \\\\\n"
              , platform_ids.size());
  std::printf ("\\midrule\n");
  std::printf
    ( "Cuboids ($\\kappa$) & \\multicolumn{1}{l}{%ld (constant)} \\\\\n"
    , cuboid_counts[algo]
    );
  std::printf
    ( "IPT bytes & \\multicolumn{1}{l}{%ld (constant)} \\\\\n"
    , ipt_bytes_values[algo]
    );
  std::printf
    ( "Construct (Mpoint/s) & %s \\\\\n"
    , fmt_rate (rates[algo]["construct"]).c_str()
    );
  std::printf
    ( "\\texttt{pos\\_random} (Mpos/s) & %s \\\\\n"
    , fmt_rate (rates[algo]["pos_random"]).c_str()
    );
  std::printf
    ( "\\texttt{pos\\_all} (Mpos/s) & %s \\\\\n"
    , fmt_rate (rates[algo]["pos_all"]).c_str()
    );
  std::printf
    ( "\\texttt{at\\_random} (Mat/s) & %s \\\\\n"
    , fmt_rate (rates[algo]["at_random"]).c_str()
    );
  std::printf
    ( "\\texttt{at\\_all} (Mat/s) & %s \\\\\n"
    , fmt_rate (rates[algo]["at_all"]).c_str()
    );
  std::printf
    ( "\\texttt{select\\_d0.01} ($\\mu$s/query) & %s \\\\\n"
    , fmt_latency_us (latencies[algo]["select_d001"]).c_str()
    );
  std::printf
    ( "\\texttt{select\\_d0.1} ($\\mu$s/query) & %s \\\\\n"
    , fmt_latency_us (latencies[algo]["select_d01"]).c_str()
    );
  std::printf
    ( "\\texttt{restrict\\_d0.01} ($\\mu$s/query) & %s \\\\\n"
    , fmt_latency_us (latencies[algo]["restrict_d001"]).c_str()
    );
  std::printf
    ( "\\texttt{restrict\\_d0.1} ($\\mu$s/query) & %s \\\\\n"
    , fmt_latency_us (latencies[algo]["restrict_d01"]).c_str()
    );
  std::printf ("\\bottomrule\n");
  std::printf ("\\end{tabular}\n");

  return 0;
}
