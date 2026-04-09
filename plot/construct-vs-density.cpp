// Extract construction ns/point vs point-set density for grid scenarios.
// Equivalent to plot/construct-vs-density.pl.

#include "IPTPlot.hpp"

#include <cstdio>
#include <map>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

auto main() -> int
{
  using namespace ipt_plot;

  auto const platform_id {reference_platform_id (load_platforms())};
  auto const default_config {std::string {default_ipt_config()}};
  auto data {std::map<std::string, std::map<int, std::vector<double>>>{}};

  visit_rows
    ( [&] (auto const& platform, auto const& meta, auto const& row)
      {
        if ( platform.id != platform_id
           || meta.cache_kind != "cache-dependent"
           || meta.config != default_config
           || row.value ("algorithm") != "greedy-plus-merge"
           || row.value ("metric") != "construct"
           )
        {
          return;
        }

        auto const scenario {row.value ("scenario")};
        auto const grid {std::string {row.value ("grid")}};
        if (scenario == "regular")
        {
          data[grid][100].push_back (row.measured_value());
          return;
        }

        auto const density {parse_random_density (scenario)};
        if (!density || !(*density < 100))
        {
          return;
        }

        data[grid][*density].push_back (row.measured_value());
      }
    );

  auto const grids {grid_order()};
  auto const densities {density_order()};

  std::printf ("# density");
  std::ranges::for_each
    ( grids
    , [] (auto grid)
      {
        auto const label {std::string {grid}};
        std::printf
          ( "\t%s_mean\t%s_p10\t%s_p90"
          , label.c_str()
          , label.c_str()
          , label.c_str()
          );
      }
    );
  std::printf ("\n");

  std::ranges::for_each
    ( densities
    , [&] (auto density)
      {
        std::printf ("%d", density);
        std::ranges::for_each
          ( grids
          , [&] (auto grid)
            {
              auto const grid_key {std::string {grid}};
              if (auto const* by_density = value_ptr (data, grid_key))
              {
                if ( auto const* values = value_ptr (*by_density, density)
                   ; values != nullptr && !values->empty()
                   )
                {
                  auto const summary {summarize (*values, mean)};
                  std::printf
                    ( "\t%.3f\t%.3f\t%.3f"
                    , summary.center
                    , summary.p10
                    , summary.p90
                    );
                }
                else
                {
                  std::printf ("\t\t\t");
                }
              }
              else
              {
                std::printf ("\t\t\t");
              }
            }
          );
        std::printf ("\n");
      }
    );

  return 0;
}
