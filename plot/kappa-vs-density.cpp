// Extract cuboid count (kappa) / N vs point-set density for grid
// scenarios. Equivalent to plot/kappa-vs-density.pl.

#include "IPTPlot.hpp"

#include <charconv>
#include <cstdio>
#include <map>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace
{
  auto grid_volume (std::string_view grid) -> long
  {
    auto volume {1L};

    std::ranges::for_each
      ( std::views::split (grid, 'x')
      , [&] (auto&& part_range)
        {
          auto const part {ipt_plot::string_view_from (part_range)};
          auto value {0L};
          std::from_chars (part.data(), part.data() + part.size(), value);
          volume *= value;
        }
      );

    return volume;
  }
}

auto main() -> int
{
  using namespace ipt_plot;

  auto const platform_id {reference_platform_id (load_platforms())};
  auto const ref_config {std::string {reference_cache_config()}};
  auto const grids {grid_order()};
  auto const densities {density_order()};
  auto volume {std::map<std::string, long>{}};
  auto data {std::map<std::string, std::map<int, std::vector<double>>>{}};

  std::ranges::for_each
    ( grids
    , [&] (auto grid)
      {
        volume[std::string {grid}] = grid_volume (grid);
      }
    );

  visit_rows
    ( [&] (auto const& platform, auto const& meta, auto const& row)
      {
        if ( platform.id != platform_id
           || meta.cache_kind != "cache-dependent"
           || meta.config != ref_config
           || row.value ("algorithm") != "greedy-plus-merge"
           || row.value ("metric") != "cuboids"
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
                if ( auto const* cuboids = value_ptr (*by_density, density)
                   ; cuboids != nullptr && !cuboids->empty()
                   )
                {
                  auto ratios {std::vector<double>{}};
                  auto const divisor
                    { static_cast<double> (volume[grid_key])
                        * static_cast<double> (density) / 100.0
                    };
                  ratios.reserve (cuboids->size());
                  std::ranges::transform
                    ( *cuboids
                    , std::back_inserter (ratios)
                    , [&] (auto cuboid_count) { return cuboid_count / divisor; }
                    );

                  auto const summary {summarize (ratios, mean)};
                  std::printf ("\t%.6g\t%.6g\t%.6g", summary.center, summary.p10, summary.p90);
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
