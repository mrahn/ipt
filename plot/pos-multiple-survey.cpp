// Extract pos_random_ns/pos for multiple-survey scenarios on the
// reference platform and default cache configuration. Equivalent to
// plot/pos-multiple-survey.pl.

#include "IPTPlot.hpp"

#include <cstdio>
#include <map>
#include <ranges>
#include <string>
#include <vector>

auto main() -> int
{
  using namespace ipt_plot;

  auto const platform_id {reference_platform_id (load_platforms())};
  auto const default_config {std::string {default_ipt_config()}};
  auto ipt {std::map<std::string, std::vector<double>>{}};
  auto sp {std::map<std::string, std::vector<double>>{}};

  visit_rows
    ( [&] (auto const& platform, auto const& meta, auto const& row)
      {
        if (platform.id != platform_id || row.value ("metric") != "pos_random")
        {
          return;
        }

        auto const scenario {std::string {row.value ("scenario")}};
        if (!is_multiple_survey (scenario))
        {
          return;
        }

        auto const algorithm {row.value ("algorithm")};
        if ( algorithm == "greedy-plus-merge"
           && meta.cache_kind == "cache-dependent"
           && meta.config == default_config
           )
        {
          ipt[scenario].push_back (row.measured_value());
          return;
        }

        if (algorithm == "sorted-points" && meta.cache_kind == "cache-independent")
        {
          sp[scenario].push_back (row.measured_value());
        }
      }
    );

  std::printf ("# scenario\tIPT_mean\tIPT_p10\tIPT_p90\t"
               "SortedPoints_mean\tSortedPoints_p10\tSortedPoints_p90\n");

  auto const scenarios {structured_scenarios()};
  std::ranges::for_each
    ( scenarios
    , [&] (auto const& scenario)
      {
        auto const ipt_summary {summarize_or_default (value_ptr (ipt, scenario), mean)};
        auto const sp_summary {summarize_or_default (value_ptr (sp, scenario), mean)};
        auto const label {structured_scenario_label (scenario)};

        std::printf
          ( "%s\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\n"
          , label.c_str()
          , ipt_summary.center
          , ipt_summary.p10
          , ipt_summary.p90
          , sp_summary.center
          , sp_summary.p10
          , sp_summary.p90
          );
      }
    );

  return 0;
}
