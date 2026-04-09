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

  auto fmt_rate_value (RateSamples const& s) -> std::string
  {
    if (s.values.empty())
    {
      return "--";
    }

    auto const center {ipt_plot::geomean (s.values)};
    char buf[64];
    std::snprintf (buf, sizeof (buf), "%.1f", center);
    return buf;
  }

  auto fmt_rate_percentiles (RateSamples const& s) -> std::string
  {
    if (s.values.empty())
    {
      return "--";
    }

    auto const lo {ipt_plot::percentile (0.10, s.values)};
    auto const hi {ipt_plot::percentile (0.90, s.values)};
    char buf[64];
    std::snprintf (buf, sizeof (buf), "%.1f--%.1f", lo, hi);
    return buf;
  }

  auto fmt_latency_us_value (LatencySamples const& s) -> std::string
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
    char buf[64];
    std::snprintf (buf, sizeof (buf), "%.1f", center);
    return buf;
  }

  auto fmt_latency_us_percentiles (LatencySamples const& s) -> std::string
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

    auto const lo {ipt_plot::percentile (0.10, scaled)};
    auto const hi {ipt_plot::percentile (0.90, scaled)};
    char buf[64];
    std::snprintf (buf, sizeof (buf), "%.1f--%.1f", lo, hi);
    return buf;
  }

  auto fmt_bytes_per_point (long bytes, long points) -> std::string
  {
    if (bytes <= 0 || points <= 0)
    {
      return "--";
    }

    auto const value
      { static_cast<double> (bytes) / static_cast<double> (points)
      };
    char buf[64];
    if (value < 0.01)
    {
      std::snprintf (buf, sizeof (buf), "%.4f", value);
    }
    else if (value < 0.1)
    {
      std::snprintf (buf, sizeof (buf), "%.3f", value);
    }
    else if (value < 10.0)
    {
      std::snprintf (buf, sizeof (buf), "%.2f", value);
    }
    else
    {
      std::snprintf (buf, sizeof (buf), "%.1f", value);
    }
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
  std::printf ("\\begin{tabular}{llr}\n");
  std::printf ("\\toprule\n");
  std::printf ("\\multicolumn{2}{l}{Structural constant} & Value \\\\\n");
  std::printf ("\\midrule\n");
  std::printf
    ( "Cuboids & $\\kappa$ & \\multicolumn{1}{l}{%ld} \\\\\n"
    , cuboid_counts[algo]
    );
  std::printf
    ( "IPT size & bytes & \\multicolumn{1}{l}{%ld} \\\\\n"
    , ipt_bytes_values[algo]
    );
  std::printf
    ( " & bytes/point & \\multicolumn{1}{l}{%s} \\\\\n"
    , fmt_bytes_per_point (ipt_bytes_values[algo], point_count).c_str()
    );
  std::printf ("\\bottomrule\n");
  std::printf ("\\end{tabular}\n");
  std::printf ("\\medskip\n");
  std::printf ("\\begin{tabular}{llrr}\n");
  std::printf ("\\toprule\n");
  std::printf ("\\multicolumn{2}{l}{Metric} & Value & 10th--90th pct. \\\\\n");
  std::printf ("\\midrule\n");
  std::printf
    ( "Construct & Mpoint/s & %s & %s \\\\\n"
    , fmt_rate_value (rates[algo]["construct"]).c_str()
    , fmt_rate_percentiles (rates[algo]["construct"]).c_str()
    );
  std::printf
    ( "\\texttt{pos\\_random} & Mpos/s & %s & %s \\\\\n"
    , fmt_rate_value (rates[algo]["pos_random"]).c_str()
    , fmt_rate_percentiles (rates[algo]["pos_random"]).c_str()
    );
  std::printf
    ( "\\texttt{pos\\_all} & Mpos/s & %s & %s \\\\\n"
    , fmt_rate_value (rates[algo]["pos_all"]).c_str()
    , fmt_rate_percentiles (rates[algo]["pos_all"]).c_str()
    );
  std::printf
    ( "\\texttt{at\\_random} & Mat/s & %s & %s \\\\\n"
    , fmt_rate_value (rates[algo]["at_random"]).c_str()
    , fmt_rate_percentiles (rates[algo]["at_random"]).c_str()
    );
  std::printf
    ( "\\texttt{at\\_all} & Mat/s & %s & %s \\\\\n"
    , fmt_rate_value (rates[algo]["at_all"]).c_str()
    , fmt_rate_percentiles (rates[algo]["at_all"]).c_str()
    );
  std::printf
    ( "\\texttt{select\\_d0.01} & $\\mu$s/query & %s & %s \\\\\n"
    , fmt_latency_us_value (latencies[algo]["select_d001"]).c_str()
    , fmt_latency_us_percentiles (latencies[algo]["select_d001"]).c_str()
    );
  std::printf
    ( "\\texttt{select\\_d0.1} & $\\mu$s/query & %s & %s \\\\\n"
    , fmt_latency_us_value (latencies[algo]["select_d01"]).c_str()
    , fmt_latency_us_percentiles (latencies[algo]["select_d01"]).c_str()
    );
  std::printf
    ( "\\texttt{restrict\\_d0.01} & $\\mu$s/query & %s & %s \\\\\n"
    , fmt_latency_us_value (latencies[algo]["restrict_d001"]).c_str()
    , fmt_latency_us_percentiles (latencies[algo]["restrict_d001"]).c_str()
    );
  std::printf
    ( "\\texttt{restrict\\_d0.1} & $\\mu$s/query & %s & %s \\\\\n"
    , fmt_latency_us_value (latencies[algo]["restrict_d01"]).c_str()
    , fmt_latency_us_percentiles (latencies[algo]["restrict_d01"]).c_str()
    );
  std::printf ("\\bottomrule\n");
  std::printf ("\\end{tabular}\n");

  return 0;
}
