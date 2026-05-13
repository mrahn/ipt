// Extract pos_random_ns/pos for multiple-survey scenarios on the
// reference platform and default cache configuration. Equivalent to
// results/plot/pos-multiple-survey.pl.

#include "IPTPlot.hpp"

#include <array>
#include <cstdio>
#include <map>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

auto main() -> int
{
  using namespace ipt_plot;

  struct Algorithm
  {
    std::string_view name;
    std::string_view heading;
  };

  constexpr auto algorithms
    { std::to_array<Algorithm>
      ( { {"greedy-plus-merge", "IPT"}
        , {"sorted-points", "SortedPoints"}
        , {"dense-bitset", "DenseBitset"}
        , {"block-bitmap", "BlockBitmap"}
        , {"roaring", "Roaring"}
        , {"lex-run", "LexRun"}
        , {"row-csr-k1", "RowCSR1"}
        , {"row-csr-k2", "RowCSR2"}
        }
      )
    };

  auto const platform_id {reference_platform_id (load_platforms())};
  auto const default_config {std::string {default_ipt_config()}};
  auto values
    {std::map<std::string, std::map<std::string, std::vector<double>>>{}};

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
          values[scenario][std::string {algorithm}].push_back
            (row.measured_value());
          return;
        }

        if (meta.cache_kind != "cache-independent")
        {
          return;
        }

        if ( std::ranges::any_of
               ( algorithms
               , [&] (Algorithm const& candidate) noexcept
                 {
                   return candidate.name == algorithm;
                 }
               )
           )
        {
          values[scenario][std::string {algorithm}].push_back
            (row.measured_value());
        }
      }
    );

  std::printf ("# scenario");
  std::ranges::for_each
    ( algorithms
    , [] (Algorithm const& algorithm)
      {
        std::printf
          ( "\t%s_mean\t%s_p10\t%s_p90"
          , algorithm.heading.data()
          , algorithm.heading.data()
          , algorithm.heading.data()
          );
      }
    );
  std::printf ("\n");

  auto const scenarios {structured_scenarios()};
  std::ranges::for_each
    ( scenarios
    , [&] (auto const& scenario)
      {
        auto const label {structured_scenario_label (scenario)};
        auto const* scenario_values {value_ptr (values, scenario)};

        std::printf ("%s", label.c_str());
        std::ranges::for_each
          ( algorithms
          , [&] (Algorithm const& algorithm)
            {
              auto const summary
                { summarize_or_default
                    ( scenario_values == nullptr
                    ? nullptr
                    : value_ptr (*scenario_values, std::string {algorithm.name})
                    , mean
                    )
                };

              if (summary.center > 0.0)
              {
                std::printf
                  ( "\t%.3f\t%.3f\t%.3f"
                  , summary.center
                  , summary.p10
                  , summary.p90
                  );
              }
              else
              {
                std::printf ("\tNaN\tNaN\tNaN");
              }
            }
          );
        std::printf ("\n");
      }
    );

  return 0;
}
