// Compute geometric-mean restrict time ratios between baselines and
// the IPT (greedy-plus-merge, c0r1e1l0) on the EPYC 9454 reference
// platform across the structured multiple-survey scenarios.

#include "IPTPlot.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace
{
  constexpr auto density_suffixes() noexcept -> std::array<std::string_view, 4>
  {
    return {"d0001", "d001", "d01", "d1"};
  }

  constexpr auto ipt_algorithms() noexcept -> std::array<std::string_view, 3>
  {
    return {"greedy-plus-merge", "greedy-plus-combine", "regular-ipt"};
  }

  constexpr auto baselines() noexcept -> std::array<std::string_view, 7>
  {
    return
      { "sorted-points"
      , "lex-run"
      , "row-csr-k1"
      , "row-csr-k2"
      , "dense-bitset"
      , "block-bitmap"
      , "roaring"
      };
  }

  auto is_ipt_algorithm (std::string_view algorithm) -> bool
  {
    auto const names {ipt_algorithms()};
    return std::ranges::find (names, algorithm) != std::cend (names);
  }

  auto is_supported_metric (std::string_view metric) -> bool
  {
    auto const prefix {std::string_view {"restrict_"}};

    if (!metric.starts_with (prefix))
    {
      return false;
    }

    auto const density {metric.substr (prefix.size())};
    auto const supported_densities {density_suffixes()};
    return std::ranges::find (supported_densities, density)
      != std::cend (supported_densities)
    ;
  }

  auto mean_of (std::vector<double> const* values) -> std::optional<double>
  {
    if (values == nullptr || values->empty())
    {
      return std::nullopt;
    }

    return ipt_plot::mean (*values);
  }
}

auto main() -> int
{
  using namespace ipt_plot;

  auto const platform_id {reference_platform_id (load_platforms())};
  if (platform_id.empty())
  {
    std::fprintf (stderr, "no reference platform dir found\n");
    return 1;
  }

  auto const ipt_config {std::string {default_ipt_config()}};
  auto const scenarios {structured_scenarios()};

  using SampleMap = std::map<std::string,
                      std::map<std::string,
                        std::map<std::string, std::vector<double>>>>;
  auto samples {SampleMap {}};

  visit_rows
    ( [&] (auto const& platform, auto const& meta, auto const& row)
      {
        if (platform.id != platform_id)
        {
          return;
        }

        auto const scenario {std::string {row.value ("scenario")}};
        if (std::ranges::find (scenarios, scenario) == std::cend (scenarios))
        {
          return;
        }

        auto const metric {row.value ("metric")};
        if (!is_supported_metric (metric))
        {
          return;
        }

        auto const algorithm {row.value ("algorithm")};
        if (is_ipt_algorithm (algorithm))
        {
          if (meta.cache_kind != "cache-dependent" || meta.config != ipt_config)
          {
            return;
          }
        }
        else if (meta.cache_kind != "cache-independent")
        {
          return;
        }

        auto const value {row.value ("ns_per_restrict")};
        if (value.empty())
        {
          return;
        }

        auto* end {static_cast<char*> (nullptr)};
        auto buffer {std::array<char, 64> {}};
        auto const n {std::min (value.size(), buffer.size() - 1)};
        std::memcpy (buffer.data(), value.data(), n);
        buffer[n] = '\0';
        samples[scenario][std::string {algorithm}][std::string {metric}]
          .push_back (std::strtod (buffer.data(), std::addressof (end)));
      }
    );

  auto const suffixes {density_suffixes()};
  auto const baseline_names {baselines()};
  auto const ipt_name {std::string {"greedy-plus-merge"}};

  std::printf ("metric\tbaseline\tn_scenarios\tgeomean(baseline/IPT)\n");
  std::ranges::for_each
    ( suffixes
    , [&] (auto density)
      {
        auto metric {std::string {"restrict_"}};
        metric.append (density);

        std::ranges::for_each
          ( baseline_names
          , [&] (auto baseline)
            {
              auto ratios {std::vector<double> {}};

              std::ranges::for_each
                ( scenarios
                , [&] (auto const& scenario)
                  {
                    if (auto const* by_algorithm = value_ptr (samples, scenario))
                    {
                      if (auto const* ipt_metrics = value_ptr (*by_algorithm, ipt_name))
                      {
                        if (auto const* base_metrics
                              = value_ptr (*by_algorithm, std::string {baseline}))
                        {
                          auto const ipt_mean
                            {mean_of (value_ptr (*ipt_metrics, metric))};
                          auto const base_mean
                            {mean_of (value_ptr (*base_metrics, metric))};
                          if (ipt_mean && base_mean && *ipt_mean > 0.0)
                          {
                            ratios.push_back (*base_mean / *ipt_mean);
                          }
                        }
                      }
                    }
                  }
                );

              auto const baseline_name {std::string {baseline}};
              auto const g {ratios.empty() ? 0.0 : geomean (ratios)};
              std::printf
                ( "%s\t%-14s\t%d\t%.3f\n"
                , metric.c_str()
                , baseline_name.c_str()
                , static_cast<int> (ratios.size())
                , g
                );
            }
          );

        std::printf ("\n");
      }
    );

  return 0;
}
