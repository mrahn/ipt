// Extract hit-vs-miss try_pos timings from pos-mix-<combo>.txt files
// for the reference platform.  One TSV row per miss_ratio; columns are
// ratio followed by ns_per_pos for each (algorithm, miss_kind) pair,
// aggregated as geometric mean across the seven smaller structured
// multiple-survey scenarios (averaged first across seeds within each
// scenario).

#include "IPTPlot.hpp"

#include <array>
#include <cstdio>
#include <map>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
  using namespace ipt_plot;

  // Order matches benchmark/Common.hpp run_pos_mix_scenario.
  // The structured 3D scenarios all use D=3, so the second RowCSR
  // variant is RowCSR<3, 2> -> emitted as "row-csr-k2".
  constexpr std::array<std::string_view, 8> algorithm_order
    { "greedy-plus-merge"
    , "sorted-points"
    , "dense-bitset"
    , "block-bitmap"
    , "roaring"
    , "lex-run"
    , "row-csr-k1"
    , "row-csr-k2"
    };

  // Order matches Common.hpp::pos_mix_miss_ratio_tags.
  constexpr std::array<std::string_view, 5> ratio_tags
    { "0.000", "0.010", "0.100", "0.500", "1.000" };

  constexpr std::array<std::string_view, 2> kind_order {"in_grid", "out_of_grid"};

  constexpr std::array<std::string_view, 7> scenario_order
    { "multiple-survey-2-l"
    , "multiple-survey-3-steps"
    , "multiple-survey-4-overlap"
    , "multiple-survey-5-mixed"
    , "multiple-survey-6-bands"
    , "multiple-survey-7-large"
    , "multiple-survey-8-threed"
    };
}

auto main() -> int
{
  auto const platform_id {reference_platform_id (load_platforms())};
  auto const default_config {std::string {default_ipt_config()}};

  // (algorithm, miss_kind, miss_ratio, scenario) -> seed measurements
  using Key = std::tuple<std::string, std::string, std::string, std::string>;
  auto buckets {std::map<Key, std::vector<double>>{}};

  visit_rows
    ( [&] (auto const& platform, auto const& meta, auto const& row)
      {
        if (platform.id != platform_id)
        {
          return;
        }
        if (meta.cache_kind != "pos-mix" || meta.config != default_config)
        {
          return;
        }
        if (row.value ("metric") != "pos_mix")
        {
          return;
        }

        auto key
          { Key
            { std::string {row.value ("algorithm")}
            , std::string {row.value ("miss_kind")}
            , std::string {row.value ("miss_ratio")}
            , std::string {row.value ("scenario")}
            }
          };
        buckets[std::move (key)].push_back (row.measured_value());
      }
    );

  // Header.
  std::printf ("# miss_ratio");
  std::ranges::for_each
    ( kind_order
    , [&] (auto kind)
      {
        std::ranges::for_each
          ( algorithm_order
          , [&] (auto alg) { std::printf ("\t%.*s_%.*s"
                                         , int (kind.size()), kind.data()
                                         , int (alg.size()), alg.data()); }
          );
      }
    );
  std::printf ("\n");

  std::ranges::for_each
    ( ratio_tags
    , [&] (auto ratio)
      {
        std::printf ("%.*s", int (ratio.size()), ratio.data());
        std::ranges::for_each
          ( kind_order
          , [&] (auto kind)
            {
              std::ranges::for_each
                ( algorithm_order
                , [&] (auto alg)
                  {
                    auto per_scenario_means {std::vector<double>{}};
                    std::ranges::for_each
                      ( scenario_order
                      , [&] (auto const& scenario)
                        {
                          auto const* values
                            { value_ptr
                              ( buckets
                              , Key { std::string {alg}
                                    , std::string {kind}
                                    , std::string {ratio}
                                    , scenario
                                    }
                              )
                            };
                          if (values != nullptr && !values->empty())
                          {
                            per_scenario_means.push_back (mean (*values));
                          }
                        }
                      );
                    auto const aggregate
                      { per_scenario_means.empty()
                          ? 0.0
                          : geomean (per_scenario_means)
                      };
                    std::printf ("\t%.3f", aggregate);
                  }
                );
            }
          );
        std::printf ("\n");
      }
    );

  return 0;
}
