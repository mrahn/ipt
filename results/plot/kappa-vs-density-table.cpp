// Aggregate IPT greedy-plus-merge cuboid count vs. density across the
// four grid scenarios, and emit a stand-alone LaTeX table. Columns:
// mean kappa per grid (4), then mean kappa / N per grid (4). Rows: one
// per density (1, 2, 5, 10, 25, 50, 75, 90, 95, 98, 99, 100).

#include "IPTPlot.hpp"

#include <charconv>
#include <cmath>
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
          auto const result
            { std::from_chars ( part.data()
                              , part.data() + part.size()
                              , value
                              )
            };

          if ( result.ec == std::errc {}
             && result.ptr == part.data() + part.size()
             )
          {
            volume *= value;
          }
        }
      );

    return volume;
  }

  auto format_count (double value) -> std::string
  {
    if (!std::isfinite (value))
    {
      return "--";
    }

    char buffer[64];
    std::snprintf (buffer, sizeof (buffer), "%.2f", value);
    return buffer;
  }

  auto format_ratio (double value) -> std::string
  {
    if (!std::isfinite (value))
    {
      return "--";
    }

    char buffer[64];
    std::snprintf (buffer, sizeof (buffer), "%.6f", value);
    return buffer;
  }

  auto grid_header (std::string_view grid) -> std::string
  {
    if (grid == "10x10x10")
    {
      return "$10^{3}$";
    }
    if (grid == "100x10x10")
    {
      return "$100{\\times}10^{2}$";
    }
    if (grid == "100x100x10")
    {
      return "$100^{2}{\\times}10$";
    }
    if (grid == "100x100x100")
    {
      return "$100^{3}$";
    }

    auto output {std::string {"$"}};
    std::ranges::for_each
      ( grid
      , [&] (auto const character)
        {
          if (character == 'x')
          {
            output += "{\\times}";
          }
          else
          {
            output += character;
          }
        }
      );
    output += '$';
    return output;
  }
}

auto main() -> int
{
  using namespace ipt_plot;

  auto const platform_id {reference_platform_id (load_platforms())};
  auto const reference_config {std::string {reference_cache_config()}};
  auto const grids {grid_order()};
  auto const densities {density_order()};
  auto volumes {std::map<std::string, long>{}};
  auto buckets {std::map<std::string, std::map<int, std::vector<double>>>{}};

  std::ranges::for_each
    ( grids
    , [&] (auto grid)
      {
        auto const grid_key {std::string {grid}};
        volumes[grid_key] = grid_volume (grid);
      }
    );

  visit_rows
    ( [&] (auto const& platform, auto const& meta, auto const& row)
      {
        if ( platform.id != platform_id
           || meta.cache_kind != "cache-dependent"
           || meta.config != reference_config
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
          buckets[grid][100].push_back (row.measured_value());
          return;
        }

        auto const density {parse_random_density (scenario)};
        if (!density || !(*density < 100))
        {
          return;
        }

        buckets[grid][*density].push_back (row.measured_value());
      }
    );

  auto column_spec {std::string {"r"}};
  std::ranges::for_each (grids, [&] (auto) { column_spec += 'r'; });
  std::ranges::for_each (grids, [&] (auto) { column_spec += 'r'; });

  std::printf ("{\n");
  std::printf ("\\begin{tabular}{%s}\n", column_spec.c_str());
  std::printf ("\\toprule\n");
  std::printf
    ( " & \\multicolumn{%zu}{c}{Mean $\\kappa$}"
      " & \\multicolumn{%zu}{c}{Mean $\\kappa/N$} \\\\\n"
    , grids.size()
    , grids.size()
    );

  std::printf ("Density");
  std::ranges::for_each
    ( grids
    , [] (auto grid) { std::printf (" & %s", grid_header (grid).c_str()); }
    );
  std::ranges::for_each
    ( grids
    , [] (auto grid) { std::printf (" & %s", grid_header (grid).c_str()); }
    );
  std::printf (" \\\\\n");
  std::printf ("\\cmidrule(lr){1-1}");
  std::printf
    ( "\\cmidrule(lr){2-%zu}\\cmidrule(lr){%zu-%zu}\n"
    , 1 + grids.size()
    , 2 + grids.size()
    , 1 + 2 * grids.size()
    );

  std::ranges::for_each
    ( densities
    , [&] (auto density)
      {
        std::printf ("%d\\%%", density);

        auto means {std::vector<double>{}};
        means.reserve (grids.size());
        std::ranges::for_each
          ( grids
          , [&] (auto grid)
            {
              auto const grid_key {std::string {grid}};
              auto value {std::nan ("")};
              if (auto const* by_density = value_ptr (buckets, grid_key))
              {
                if ( auto const* counts = value_ptr (*by_density, density)
                   ; counts != nullptr && !counts->empty()
                   )
                {
                  value = mean (*counts);
                }
              }
              means.push_back (value);
            }
          );

        std::ranges::for_each
          ( means
          , [] (auto value)
            {
              std::printf (" & %s", format_count (value).c_str());
            }
          );

        for (auto index {std::size_t {0}}; index < grids.size(); ++index)
        {
          auto const grid_key {std::string {grids[index]}};
          auto const point_count
            { static_cast<double> (volumes[grid_key])
            * static_cast<double> (density) / 100.0
            };
          auto const ratio {means[index] / point_count};
          std::printf (" & %s", format_ratio (ratio).c_str());
        }

        std::printf (" \\\\\n");
      }
    );

  std::printf ("\\bottomrule\n");
  std::printf ("\\end{tabular}\n");
  std::printf ("}\n");

  return 0;
}