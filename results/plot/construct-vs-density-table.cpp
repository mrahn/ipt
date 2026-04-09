// Aggregate IPT greedy-plus-merge construction cost vs. density
// across the four grid scenarios on the reference platform and
// default cache configuration, and emit a stand-alone LaTeX table.
// Columns: ns/point per grid (4), then Mpoints/s per grid (4).
// Rows: one per density (1, 2, 5, 10, 25, 50, 75, 90, 95, 98, 99, 100).

#include "IPTPlot.hpp"

#include <cmath>
#include <cstdio>
#include <format>
#include <map>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace
{
  constexpr auto dispersion_threshold {0.20};

  auto fmt_value (double v) -> std::string
  {
    if (!std::isfinite (v) || v <= 0.0)
    {
      return "--";
    }

    char buf[64];
    std::snprintf (buf, sizeof (buf), "%.2f", v);
    return buf;
  }

  auto with_dagger (std::string text, bool flag) -> std::string
  {
    if (!flag)
    {
      return text;
    }
    return "$^{\\dagger}$" + text;
  }

  auto grid_header (std::string_view grid) -> std::string
  {
    // Compact powers-of-ten rendering for the four grid scenarios.
    if (grid == "10x10x10")    { return "$10^{3}$"; }
    if (grid == "100x10x10")   { return "$100{\\times}10^{2}$"; }
    if (grid == "100x100x10")  { return "$100^{2}{\\times}10$"; }
    if (grid == "100x100x100") { return "$100^{3}$"; }

    // Fallback: render literal Nx...xN as $N{\times}...{\times}N$.
    auto out {std::string {"$"}};
    for (auto const ch : grid)
    {
      if (ch == 'x')
      {
        out += "{\\times}";
      }
      else
      {
        out += ch;
      }
    }
    out += '$';
    return out;
  }
}

auto main() -> int
{
  using namespace ipt_plot;

  auto const platform_id {reference_platform_id (load_platforms())};
  auto const default_config {std::string {default_ipt_config()}};
  auto buckets {std::map<std::string, std::map<int, std::vector<double>>>{}};

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

  auto const grids {grid_order()};
  auto const densities {density_order()};

  // Column spec: density + 4 ns + (small gap) + 4 Mp/s.
  auto column_spec {std::string {"r"}};
  for (auto i {0u}; i < grids.size(); ++i)
  {
    column_spec += 'r';
  }
  for (auto i {0u}; i < grids.size(); ++i)
  {
    column_spec += 'r';
  }

  std::printf ("{\n");
  std::printf ("\\begin{tabular}{%s}\n", column_spec.c_str());
  std::printf ("\\toprule\n");

  // Block header row: empty under density, then multicolumn ns/point and Mp/s.
  std::printf
    ( " & \\multicolumn{%zu}{c}{ns/point}"
      " & \\multicolumn{%zu}{c}{Mpoints/s} \\\\\n"
    , grids.size()
    , grids.size()
    );

  // Sub-header row: density, grid labels twice.
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

        // Compute mean ns/point per grid and per-cell spread flag.
        auto means {std::vector<double>{}};
        auto over_threshold {std::vector<bool>{}};
        means.reserve (grids.size());
        over_threshold.reserve (grids.size());
        std::ranges::for_each
          ( grids
          , [&] (auto grid)
            {
              auto const grid_key {std::string {grid}};
              auto value {std::nan ("")};
              auto flag {false};
              if (auto const* by_density = value_ptr (buckets, grid_key))
              {
                if ( auto const* values = value_ptr (*by_density, density)
                   ; values != nullptr && !values->empty()
                   )
                {
                  auto const m {mean (*values)};
                  value = m;
                  if (values->size() >= 2)
                  {
                    auto const med {percentile (0.50, *values)};
                    if (std::isfinite (med) && med > 0.0)
                    {
                      auto const p10 {percentile (0.10, *values)};
                      auto const p90 {percentile (0.90, *values)};
                      flag = (p90 - p10) / med > dispersion_threshold;
                    }
                  }
                }
              }
              means.push_back (value);
              over_threshold.push_back (flag);
            }
          );

        // ns/point block.
        for (auto i {0u}; i < means.size(); ++i)
        {
          std::printf
            ( " & %s"
            , with_dagger (fmt_value (means[i]), over_threshold[i]).c_str()
            );
        }
        // Mp/s block = 1000 / ns.
        for (auto i {0u}; i < means.size(); ++i)
        {
          auto const v {means[i]};
          auto const mp {(std::isfinite (v) && v > 0.0) ? 1000.0 / v
                                                        : std::nan ("")};
          std::printf
            ( " & %s"
            , with_dagger (fmt_value (mp), over_threshold[i]).c_str()
            );
        }
        std::printf (" \\\\\n");
      }
    );

  std::printf ("\\bottomrule\n");
  std::printf ("\\end{tabular}\n");
  std::printf ("}\n");

  return 0;
}
