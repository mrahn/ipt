// Extract geometric-mean ratios relative to c0r0e0l0 for all cache
// configurations across all platforms, scenarios, and seeds.
//
// Equivalent to plot/at-cache-effect.pl.

#include "IPTPlot.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <map>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace
{
  struct SummaryRow
  {
    std::string config;
    std::string label;
    std::map<std::string, ipt_plot::DistributionSummary> summaries;
  };

  constexpr auto metrics() noexcept -> std::array<std::string_view, 4>
  {
    return {"construct", "pos_random", "at_random", "ipt_bytes"};
  }

  auto display_label (std::string_view config) -> std::string
  {
    if (config == ipt_plot::default_ipt_config() || config == "c0r1e1l1")
    {
      return std::format ("\\\\textbf{{{}}}", config);
    }

    return std::string {config};
  }

  auto summary_center
    ( std::map<std::string, ipt_plot::DistributionSummary> const& summaries
    , std::string_view metric
    ) -> double
  {
    if (auto const* summary = ipt_plot::value_ptr (summaries, std::string {metric}))
    {
      return summary->center;
    }
    return 0.0;
  }

  auto measurement_key
    ( ipt_plot::Platform const& platform
    , ipt_plot::Row const& row
    , std::string_view metric
    ) -> std::string
  {
    return std::format
      ( "{}|{}|{}|{}|{}"
      , platform.id
      , row.value ("grid")
      , row.value ("scenario")
      , row.value ("seed")
      , metric
      );
  }

  auto metric_from_key (std::string const& key) -> std::string_view
  {
    auto const pos {key.rfind ('|')};
    return pos == std::string::npos
      ? std::string_view {key}
      : std::string_view {key}.substr (pos + 1)
      ;
  }

  auto is_wanted_metric (std::string_view metric) -> bool
  {
    auto const wanted {metrics()};
    return std::ranges::find (wanted, metric) != std::cend (wanted);
  }
}

auto main() -> int
{
  using namespace ipt_plot;

  auto const baseline {std::string {reference_cache_config()}};
  auto values {std::map<std::string, std::map<std::string, double>>{}};

  visit_rows
    ( [&] (auto const& platform, auto const& meta, auto const& row)
      {
        if ( meta.cache_kind != "cache-dependent"
           || row.value ("algorithm") != "greedy-plus-merge"
           || meta.config.empty()
           )
        {
          return;
        }

        auto const metric {row.value ("metric")};
        if (!is_wanted_metric (metric))
        {
          return;
        }

        values[meta.config][measurement_key (platform, row, metric)]
          = row.measured_value()
          ;
      }
    );

  auto const metric_names {metrics()};
  if (auto const* baseline_values = value_ptr (values, baseline))
  {
    auto rows {std::vector<SummaryRow>{}};
    rows.reserve (values.size());

    std::ranges::for_each
      ( values
      , [&] (auto const& value_entry)
        {
          auto const& config {value_entry.first};
          auto const& config_values {value_entry.second};
          auto ratios {std::map<std::string, std::vector<double>>{}};

          std::ranges::for_each
            ( *baseline_values
            , [&] (auto const& baseline_entry)
              {
                auto const& key {baseline_entry.first};
                auto const baseline_value {baseline_entry.second};
                if (auto const* value = value_ptr (config_values, key))
                {
                  if (baseline_value > 0.0 && *value > 0.0)
                  {
                    ratios[std::string {metric_from_key (key)}]
                      .push_back (*value / baseline_value)
                      ;
                  }
                }
              }
            );

          auto summaries {std::map<std::string, DistributionSummary>{}};
          std::ranges::for_each
            ( metric_names
            , [&] (auto metric)
              {
                auto const metric_key {std::string {metric}};
                summaries[metric_key]
                  = summarize_or_default (value_ptr (ratios, metric_key), geomean);
              }
            );
          rows.push_back
            ( SummaryRow
              { .config = config
              , .label = display_label (config)
              , .summaries = std::move (summaries)
              }
            );
        }
      );

    std::ranges::sort
      ( rows
      , [] (auto const& lhs, auto const& rhs)
        {
          auto const lhs_memory {summary_center (lhs.summaries, "ipt_bytes")};
          auto const rhs_memory {summary_center (rhs.summaries, "ipt_bytes")};
          if (lhs_memory != rhs_memory)
          {
            return lhs_memory < rhs_memory;
          }

          return lhs.config < rhs.config;
        }
      );

    std::printf ("# config\tlabel");
    std::ranges::for_each
      ( metric_names
      , [] (auto metric)
        {
          auto const label {std::string {metric}};
          std::printf
            ( "\t%s_gmean\t%s_p10\t%s_p90"
            , label.c_str()
            , label.c_str()
            , label.c_str()
            );
        }
      );
    std::printf ("\n");

    std::ranges::for_each
      ( rows
      , [&] (auto const& row)
        {
          std::printf ("%s\t%s", row.config.c_str(), row.label.c_str());
          std::ranges::for_each
            ( metric_names
            , [&] (auto metric)
              {
                if ( auto const* summary = value_ptr (row.summaries, std::string {metric})
                   ; summary != nullptr && summary->center > 0.0
                   )
                {
                  std::printf
                    ( "\t%.4f\t%.4f\t%.4f"
                    , summary->center
                    , summary->p10
                    , summary->p90
                    );
                }
                else
                {
                  std::printf ("\t0\t0\t0");
                }
              }
            );
          std::printf ("\n");
        }
      );
  }

  return EXIT_SUCCESS;
}
