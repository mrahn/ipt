// Aggregate pos_random time ratios (IPT / selected cache-independent
// baseline) vs. density across the four grid scenarios on the reference
// platform and default cache configuration, and emit a stand-alone LaTeX
// table. Columns: min--max geometric-mean ratio over baselines per grid.
// Rows: one density.

#include "IPTPlot.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <compare>
#include <cstdio>
#include <map>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
  constexpr auto dispersion_threshold {0.20};

  struct Key
  {
    std::string grid;
    int density{};
    std::string seed;

    auto operator<=> (Key const& other) const = default;
  };

  struct Baseline
  {
    char const* name;
    char const* label;
  };

  using ValuesByKey = std::map<Key, double>;
  using DensityBuckets = std::map<int, std::vector<double>>;
  using GridBuckets = std::map<std::string, DensityBuckets>;
  using BaselineBuckets = std::map<std::string, GridBuckets>;

  auto baseline_order() -> std::array<Baseline, 4>
  {
    return
      { Baseline {"sorted-points", "\\textsc{SortedPoints}"}
      , Baseline {"lex-run", "\\textsc{LexRun}"}
      , Baseline {"row-csr-k1", "\\textsc{RowCSR-k1}"}
      , Baseline {"row-csr-k2", "\\textsc{RowCSR-k2}"}
      };
  }

  auto format_ratio (double value) -> std::string
  {
    if (!std::isfinite (value) || value <= 0.0)
    {
      return "--";
    }

    char buffer[64];
    std::snprintf (buffer, sizeof (buffer), "%.4f", value);
    return buffer;
  }

  auto with_dagger (std::string text, bool flag) -> std::string
  {
    if (!flag)
    {
      return text;
    }

    return "$^{\\dagger}$" + text;
  }

  auto format_range (std::vector<double> values) -> std::string
  {
    std::erase_if
      ( values
      , [] (auto value) noexcept
        {
          return !std::isfinite (value) || value <= 0.0;
        }
      );

    if (values.empty())
    {
      return "--";
    }

    auto const [low, high] {std::ranges::minmax (values)};
    return format_ratio (low) + "--" + format_ratio (high);
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
  auto const default_config {std::string {default_ipt_config()}};
  auto const baselines {baseline_order()};
  auto ipt {ValuesByKey{}};
  auto baselines_by_algorithm {std::map<std::string, ValuesByKey>{}};

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
        auto const algorithm {std::string {row.value ("algorithm")}};

        if ( algorithm == "greedy-plus-merge"
          && meta.cache_kind == "cache-dependent"
          && meta.config == default_config
           )
        {
          ipt[std::move (key)] = row.measured_value();
          return;
        }

        auto const selected_baseline
          { std::ranges::any_of
              ( baselines
              , [&] (auto const& baseline) noexcept
                {
                  return algorithm == baseline.name;
                }
              )
          };
        if (selected_baseline && meta.cache_kind == "cache-independent")
        {
          baselines_by_algorithm[algorithm][std::move (key)]
            = row.measured_value();
        }
      }
    );

  auto ratios {BaselineBuckets{}};
  std::ranges::for_each
    ( ipt
    , [&] (auto const& entry)
      {
        auto const& key {entry.first};
        auto const ipt_value {entry.second};
        std::ranges::for_each
          ( baselines
          , [&] (auto const& baseline)
            {
              auto const baseline_name {std::string {baseline.name}};
              auto const* baseline_values
                {value_ptr (baselines_by_algorithm, baseline_name)};
              if (baseline_values == nullptr)
              {
                return;
              }

              auto const* baseline_value {value_ptr (*baseline_values, key)};
              if (baseline_value != nullptr && *baseline_value > 0.0)
              {
                ratios[baseline_name][key.grid][key.density].push_back
                  (ipt_value / *baseline_value);
              }
            }
          );
      }
    );

  auto const grids {grid_order()};
  auto const densities {density_order()};

  std::printf ("%% Auto-generated by results/plot/pos-ratio-vs-density-table.cpp.\n");
  std::printf ("%% Reference platform: %s.\n", platform_id.c_str());
  std::printf ("%% Default IPT configuration: %s.\n", default_config.c_str());
  std::printf
    ( "%% Dispersion dagger threshold: (p90-p10)/geomean > %.2f.\n"
    , dispersion_threshold
    );

  std::printf ("\\begin{tabular}{rrrrr}\n");
  std::printf ("\\toprule\n");
  std::printf
    ( " & \\multicolumn{%zu}{c}{Range of geomean IPT / baseline} \\\\\n"
    , grids.size()
    );
  std::printf ("Density");
  std::ranges::for_each
    ( grids
    , [] (auto grid) { std::printf (" & %s", grid_header (grid).c_str()); }
    );
  std::printf (" \\\\\n");
  std::printf ("\\midrule\n");

  std::ranges::for_each
    ( densities
    , [&] (auto density)
      {
        std::printf ("%d\\%%", density);
        std::ranges::for_each
          ( grids
          , [&] (auto grid)
            {
              auto centers {std::vector<double>{}};
              auto high_spread {false};
              auto const grid_key {std::string {grid}};
              std::ranges::for_each
                ( baselines
                , [&] (auto const& baseline)
                  {
                    auto const baseline_name {std::string {baseline.name}};
                    if (auto const* by_grid = value_ptr (ratios, baseline_name))
                    {
                      if (auto const* by_density = value_ptr (*by_grid, grid_key))
                      {
                        if ( auto const* values = value_ptr (*by_density, density)
                           ; values != nullptr && !values->empty()
                           )
                        {
                          auto const summary {summarize (*values, geomean)};
                          centers.push_back (summary.center);
                          if (summary.center > 0.0)
                          {
                            high_spread = high_spread
                              || (summary.p90 - summary.p10)
                               / summary.center > dispersion_threshold;
                          }
                        }
                      }
                    }
                  }
                );

              std::printf
                ( " & %s"
                , with_dagger (format_range (std::move (centers)), high_spread)
                  .c_str()
                );
            }
          );
        std::printf (" \\\\\n");
      }
    );

  std::printf ("\\bottomrule\n");
  std::printf ("\\end{tabular}\n");

  return 0;
}