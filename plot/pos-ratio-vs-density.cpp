// Extract pos_random time ratio (IPT / SortedPoints) vs density.
// Equivalent to plot/pos-ratio-vs-density.pl.

#include "IPTPlot.hpp"

#include <compare>
#include <cstdio>
#include <map>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace
{
  struct Key
  {
    std::string grid;
    int density{};
    std::string seed;

    auto operator<=> (Key const& other) const = default;
  };
}

auto main() -> int
{
  using namespace ipt_plot;

  auto const platform_id {reference_platform_id (load_platforms())};
  auto const default_config {std::string {default_ipt_config()}};
  auto ipt {std::map<Key, double>{}};
  auto sp {std::map<Key, double>{}};

  visit_rows
    ( [&] (auto const& platform, auto const& meta, auto const& row)
      {
        if (platform.id != platform_id || row.value ("metric") != "pos_random")
        {
          return;
        }

        auto const scenario {row.value ("scenario")};
        auto density {0};
        if (scenario == "regular")
        {
          density = 100;
        }
        else
        {
          auto const parsed_density {parse_random_density (scenario)};
          if (!parsed_density || !(*parsed_density < 100))
          {
            return;
          }

          density = *parsed_density;
        }

        auto key
          { Key
            { std::string {row.value ("grid")}
            , density
            , std::string {row.value ("seed")}
            }
          };
        auto const algorithm {row.value ("algorithm")};

        if ( algorithm == "greedy-plus-merge"
           && meta.cache_kind == "cache-dependent"
           && meta.config == default_config
           )
        {
          ipt[std::move (key)] = row.measured_value();
          return;
        }

        if (algorithm == "sorted-points" && meta.cache_kind == "cache-independent")
        {
          sp[std::move (key)] = row.measured_value();
        }
      }
    );

  auto ratios {std::map<std::string, std::map<int, std::vector<double>>>{}};
  std::ranges::for_each
    ( ipt
    , [&] (auto const& entry)
      {
        auto const& key {entry.first};
        auto const ipt_value {entry.second};
        if (auto const* sp_value = value_ptr (sp, key))
        {
          if (*sp_value > 0.0)
          {
            ratios[key.grid][key.density].push_back (ipt_value / *sp_value);
          }
        }
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
          ( "\t%s_gmean\t%s_p10\t%s_p90"
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
              if (auto const* by_density = value_ptr (ratios, grid_key))
              {
                if ( auto const* values = value_ptr (*by_density, density)
                   ; values != nullptr && !values->empty()
                   )
                {
                  auto const summary {summarize (*values, geomean)};
                  std::printf ("\t%.4f\t%.4f\t%.4f", summary.center, summary.p10, summary.p90);
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
