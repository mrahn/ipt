// Compute IPT / SortedPoints time ratios for the 5D multiple-survey
// stress scenario across all platform/compiler combinations with a
// stress run.  Emits a stand-alone LaTeX tabular snippet on stdout.

#include "IPTPlot.hpp"

#include <array>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <map>
#include <optional>
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

  struct MetricSpec
  {
    std::string_view name;
    std::string_view label;
    bool is_throughput;
  };

  constexpr auto metric_specs() noexcept -> std::array<MetricSpec, 9>
  {
    return
      { MetricSpec
          { "construct"
          , "Construct time/point"
          , true
          }
      , MetricSpec
          { "pos_random"
          , "\\texttt{pos\\_random}"
          , true
          }
      , MetricSpec
          { "pos_all"
          , "\\texttt{pos\\_all}"
          , true
          }
      , MetricSpec
          { "at_random"
          , "\\texttt{at\\_random}"
          , true
          }
      , MetricSpec
          { "at_all"
          , "\\texttt{at\\_all}"
          , true
          }
      , MetricSpec
          { "select_d001"
          , "\\texttt{select\\_d0.01}"
          , false
          }
      , MetricSpec
          { "select_d01"
          , "\\texttt{select\\_d0.1}"
          , false
          }
      , MetricSpec
          { "restrict_d001"
          , "\\texttt{restrict\\_d0.01}"
          , false
          }
      , MetricSpec
          { "restrict_d01"
          , "\\texttt{restrict\\_d0.1}"
          , false
          }
      };
  }

  auto to_double (std::string_view sv) -> double
  {
    auto value {0.0};
    std::from_chars (sv.data(), sv.data() + sv.size(), value);
    return value;
  }

  auto is_supported_metric (std::string_view metric) -> bool
  {
    auto const specs {metric_specs()};
    return std::ranges::find
        ( specs
        , metric
        , [] (auto const& spec)
          {
            return spec.name;
          }
        )
      != std::cend (specs)
    ;
  }

  auto sample_value (ipt_plot::Row const& row, std::string_view metric) -> double
  {
    if (metric == "construct")
    {
      return to_double (row.value ("Mpoint_per_sec"));
    }

    if (metric == "pos_random" || metric == "pos_all")
    {
      return to_double (row.value ("Mpos_per_sec"));
    }

    if (metric == "at_random" || metric == "at_all")
    {
      return to_double (row.value ("Mat_per_sec"));
    }

    if (metric == "select_d001" || metric == "select_d01")
    {
      return to_double (row.value ("ns_per_select"));
    }

    return to_double (row.value ("ns_per_restrict"));
  }

  auto mean_of (std::vector<double> const* values) -> std::optional<double>
  {
    if (values == nullptr || values->empty())
    {
      return std::nullopt;
    }

    return ipt_plot::mean (*values);
  }

  auto format_number (double value) -> std::string
  {
    if (!std::isfinite (value) || value <= 0.0)
    {
      return "--";
    }

    char buffer[64];
    if (value < 0.01)
    {
      std::snprintf (buffer, sizeof (buffer), "%.4f", value);
    }
    else if (value < 0.1)
    {
      std::snprintf (buffer, sizeof (buffer), "%.3f", value);
    }
    else if (value < 10.0)
    {
      std::snprintf (buffer, sizeof (buffer), "%.2f", value);
    }
    else if (value < 100.0)
    {
      std::snprintf (buffer, sizeof (buffer), "%.1f", value);
    }
    else
    {
      std::snprintf (buffer, sizeof (buffer), "%.0f", value);
    }
    return buffer;
  }

  auto format_center (std::vector<double> const& ratios) -> std::string
  {
    if (ratios.empty())
    {
      return "--";
    }

    return format_number (ipt_plot::geomean (ratios));
  }

  auto format_percentiles (std::vector<double> const& ratios) -> std::string
  {
    if (ratios.empty())
    {
      return "--";
    }

    auto const low {ipt_plot::percentile (0.10, ratios)};
    auto const high {ipt_plot::percentile (0.90, ratios)};
    return std::format ("{}--{}", format_number (low), format_number (high));
  }
}

auto main() -> int
{
  using namespace ipt_plot;

  auto const default_config {std::string {default_ipt_config()}};
  auto const target_scenario {std::string {scenario_name()}};

  using MetricSamples = std::map<std::string, std::vector<double>>;
  using AlgorithmSamples = std::map<std::string, MetricSamples>;
  auto platform_samples {std::map<std::string, AlgorithmSamples>{}};
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
        if (algorithm != "greedy-plus-merge" && algorithm != "sorted-points")
        {
          return;
        }

        auto const metric {std::string {row.value ("metric")}};
        if (!is_supported_metric (metric))
        {
          return;
        }

        if (point_count == 0)
        {
          auto const point_count_sv {row.value ("point_count")};
          std::from_chars
            ( point_count_sv.data()
            , point_count_sv.data() + point_count_sv.size()
            , point_count
            );
        }

        if (grid_label.empty())
        {
          grid_label = std::string {row.value ("grid")};
        }

        auto const value {sample_value (row, metric)};
        if (value <= 0.0)
        {
          return;
        }

        platform_samples[platform.id][algorithm][metric].push_back (value);
      }
    );

  auto ratios_by_metric {std::map<std::string, std::vector<double>>{}};
  auto ratio_platforms {std::set<std::string>{}};
  auto const specs {metric_specs()};

  std::ranges::for_each
    ( platform_samples
    , [&] (auto const& platform_entry)
      {
        auto const& platform_id {platform_entry.first};
        auto const& algorithms {platform_entry.second};
        auto const* ipt_metrics {value_ptr (algorithms, std::string {"greedy-plus-merge"})};
        auto const* sorted_points_metrics {value_ptr (algorithms, std::string {"sorted-points"})};
        if (ipt_metrics == nullptr || sorted_points_metrics == nullptr)
        {
          return;
        }

        auto added_any_ratio {false};
        std::ranges::for_each
          ( specs
          , [&] (auto const& spec)
            {
              auto const ipt_mean
                {mean_of (value_ptr (*ipt_metrics, std::string {spec.name}))};
              auto const sorted_points_mean
                { mean_of
                    ( value_ptr
                        ( *sorted_points_metrics
                        , std::string {spec.name}
                        )
                    )
                };
              if (!ipt_mean || !sorted_points_mean)
              {
                return;
              }
              if (*ipt_mean <= 0.0 || *sorted_points_mean <= 0.0)
              {
                return;
              }

              auto const ratio
                { spec.is_throughput
                    ? *sorted_points_mean / *ipt_mean
                    : *ipt_mean / *sorted_points_mean
                };
              ratios_by_metric[std::string {spec.name}].push_back (ratio);
              added_any_ratio = true;
            }
          );

        if (added_any_ratio)
        {
          ratio_platforms.insert (platform_id);
        }
      }
    );

  if (ratio_platforms.empty())
  {
    std::fprintf
      ( stderr
      , "no complete stress ratio data found for scenario %s\n"
      , target_scenario.c_str()
      );
    return 1;
  }

  std::printf ("%% Auto-generated by plot/stress-ratio-summary.cpp.\n");
  std::printf
    ( "%% scenario %s, grid %s, %ld points, %zu platforms.\n"
    , target_scenario.c_str()
    , grid_label.c_str()
    , point_count
    , ratio_platforms.size()
    );
  std::printf ("\\begin{tabular}{lrr}\n");
  std::printf ("\\toprule\n");
  std::printf ("Metric & Value & 10th--90th pct. \\\\\n");
  std::printf ("\\midrule\n");
  std::ranges::for_each
    ( specs
    , [&] (auto const& spec)
      {
        auto const* ratios {value_ptr (ratios_by_metric, std::string {spec.name})};
        auto const empty {std::vector<double>{}};
        auto const& values {ratios == nullptr ? empty : *ratios};
        std::printf
          ( "%s & %s & %s \\\\\n"
          , std::string {spec.label}.c_str()
          , format_center (values).c_str()
          , format_percentiles (values).c_str()
          );
      }
    );
  std::printf ("\\bottomrule\n");
  std::printf ("\\end{tabular}\n");

  return 0;
}