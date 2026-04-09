// Aggregate the 5D multiple-survey stress scenario across every
// platform that supplied a stress run. Emits a stand-alone LaTeX
// snippet with one tabular that mirrors the Table 7 layout:
// IPT-relative ratio columns, bold best values, and grey dominated
// baseline rows.

#include "IPTPlot.hpp"

#include <array>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <format>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace
{
  constexpr auto dispersion_threshold {0.20};

  constexpr auto scenario_name() noexcept -> std::string_view
  {
    return "multiple-survey-9-5d";
  }

  struct LayoutSpec
  {
    std::string_view algorithm;
    std::string_view label;
  };

  constexpr auto footprint_layouts() noexcept -> std::array<LayoutSpec, 5>
  {
    return
      { LayoutSpec {"greedy-plus-merge", "IPT"}
      , LayoutSpec {"sorted-points", "\\textsc{SortedPoints}"}
      , LayoutSpec {"lex-run", "\\textsc{LexRun}"}
      , LayoutSpec {"row-csr-k1", "\\textsc{RowCSR-k1}"}
      , LayoutSpec {"row-csr-k4", "\\textsc{RowCSR-k4}"}
      };
  }

  constexpr auto rate_layouts() noexcept -> std::array<LayoutSpec, 5>
  {
    return
      { LayoutSpec {"greedy-plus-merge", "IPT"}
      , LayoutSpec {"sorted-points", "\\textsc{SortedPoints}"}
      , LayoutSpec {"lex-run", "\\textsc{LexRun}"}
      , LayoutSpec {"row-csr-k1", "\\textsc{RowCSR-k1}"}
      , LayoutSpec {"row-csr-k4", "\\textsc{RowCSR-k4}"}
      };
  }

  auto to_double (std::string_view sv) -> double
  {
    auto value {0.0};
    std::from_chars (sv.data(), sv.data() + sv.size(), value);
    return value;
  }

  struct RateSamples
  {
    std::vector<double> values;
    std::map<std::string, std::vector<double>> values_by_platform;

    auto add (std::string const& platform_id, double value) -> void
    {
      if (value > 0.0)
      {
        values.push_back (value);
        values_by_platform[platform_id].push_back (value);
      }
    }

    auto has_high_spread() const -> bool
    {
      for (auto const& platform_values : values_by_platform)
      {
        if (platform_values.second.size() < 2)
        {
          continue;
        }

        auto const median_value
          {ipt_plot::percentile (0.50, platform_values.second)};
        if (!std::isfinite (median_value) || median_value <= 0.0)
        {
          continue;
        }

        auto const low {ipt_plot::percentile (0.10, platform_values.second)};
        auto const high {ipt_plot::percentile (0.90, platform_values.second)};
        if ((high - low) / median_value > dispersion_threshold)
        {
          return true;
        }
      }

      return false;
    }
  };

  auto fmt_rate_value (RateSamples const& samples) -> std::string
  {
    if (samples.values.empty())
    {
      return "--";
    }

    auto const center {ipt_plot::geomean (samples.values)};
    char buf[64];
    std::snprintf (buf, sizeof (buf), "%.1f", center);
    return buf;
  }

  auto fmt_long (long value) -> std::string
  {
    if (value <= 0)
    {
      return "--";
    }

    char buf[64];
    std::snprintf (buf, sizeof (buf), "%ld", value);
    return buf;
  }

  auto fmt_grouped_long (long value) -> std::string
  {
    if (value <= 0)
    {
      return "--";
    }

    auto digits {fmt_long (value)};
    auto grouped {std::string {}};
    auto first_group {digits.size() % 3};
    if (first_group == 0)
    {
      first_group = 3;
    }

    grouped.append (digits, 0, first_group);
    for (auto i {first_group}; i < digits.size(); i += 3)
    {
      grouped += "\\,";
      grouped.append (digits, i, 3);
    }

    return grouped;
  }

  auto fmt_ratio (double value, double ipt_value) -> std::string
  {
    if ( !std::isfinite (value)
      || value <= 0.0
      || !std::isfinite (ipt_value)
      || ipt_value <= 0.0
       )
    {
      return "--";
    }

    auto const ratio {value / ipt_value};
    char buffer[64];
    if (ratio < 0.005)
    {
      std::snprintf (buffer, sizeof (buffer), "%.3fx", ratio);
      return buffer;
    }

    if (ratio < 0.05)
    {
      std::snprintf (buffer, sizeof (buffer), "%.2fx", ratio);
      return buffer;
    }

    if (0.95 < ratio && ratio < 1.05)
    {
      std::snprintf (buffer, sizeof (buffer), "%.2fx", ratio);
      return buffer;
    }

    if (ratio > 10.0)
    {
      std::snprintf (buffer, sizeof (buffer), "%.0fx", ratio);
      return buffer;
    }

    std::snprintf (buffer, sizeof (buffer), "%.1fx", ratio);
    return buffer;
  }

  auto equal_with_tolerance (double left, double right) -> bool
  {
    if (!std::isfinite (left) || !std::isfinite (right))
    {
      return false;
    }

    auto scale {std::abs (left)};
    if (scale < std::abs (right))
    {
      scale = std::abs (right);
    }
    if (scale < 1.0)
    {
      scale = 1.0;
    }

    return std::abs (left - right) <= scale * 1.0e-9;
  }

  auto bold_if_best (std::string const& value, bool is_best) -> std::string
  {
    if (!is_best)
    {
      return value;
    }

    return std::format ("\\textbf{{{}}}", value);
  }

  auto with_dagger (std::string text, bool flag) -> std::string
  {
    if (!flag)
    {
      return text;
    }

    return "$^{\\dagger}$" + text;
  }

  auto is_strictly_smaller (double left, double right) -> bool
  {
    return left < right && !equal_with_tolerance (left, right);
  }

  auto is_strictly_larger (double left, double right) -> bool
  {
    return right < left && !equal_with_tolerance (left, right);
  }

  auto grey_if_dominated (std::string const& text, bool dominated) -> std::string
  {
    if (!dominated)
    {
      return text;
    }

    return std::format ("\\textcolor[gray]{{0.55}}{{{}}}", text);
  }
}

auto main() -> int
{
  using namespace ipt_plot;

  auto const default_config {std::string {default_ipt_config()}};
  auto const target_scenario {std::string {scenario_name()}};

  auto rates {std::map<std::string, std::map<std::string, RateSamples>>{}};
  auto cuboid_counts {std::map<std::string, long>{}};
  auto byte_sizes {std::map<std::string, long>{}};
  auto platform_ids {std::set<std::string>{}};
  auto point_count {0L};
  auto grid_label {std::string {}};

  visit_rows
    ( [&] (auto const& platform, auto const& meta, auto const& row)
      {
        if ( meta.cache_kind != "stress"
           || meta.config != default_config
           || row.value ("scenario") != target_scenario
           )
        {
          return;
        }

        auto const algorithm {std::string {row.value ("algorithm")}};
        auto const metric {std::string {row.value ("metric")}};

        if (point_count == 0)
        {
          auto const point_count_sv {row.value ("point_count")};
          std::from_chars
            ( point_count_sv.data()
            , point_count_sv.data() + point_count_sv.size()
            , point_count
            );
        }

        if (grid_label.empty())
        {
          grid_label = std::string {row.value ("grid")};
        }

        platform_ids.insert (platform.id);

        if (metric == "construct")
        {
          rates[algorithm][metric].add
            (platform.id, to_double (row.value ("Mpoint_per_sec")));
          return;
        }

        if (metric == "pos_random" || metric == "pos_all")
        {
          rates[algorithm][metric].add
            (platform.id, to_double (row.value ("Mpos_per_sec")));
          return;
        }

        if (metric == "at_random" || metric == "at_all")
        {
          rates[algorithm][metric].add
            (platform.id, to_double (row.value ("Mat_per_sec")));
          return;
        }

        if (metric == "cuboids")
        {
          auto const value_sv {row.value ("value")};
          auto value {0L};
          std::from_chars
            (value_sv.data(), value_sv.data() + value_sv.size(), value);
          cuboid_counts[algorithm] = value;
          return;
        }

        if (  metric == "ipt_bytes"
           || metric == "point_bytes"
           || metric == "lexrun_bytes"
           || metric == "rowcsrk1_bytes"
           || metric == "rowcsrkm_bytes"
           )
        {
          auto const value_sv {row.value ("value")};
          auto value {0L};
          std::from_chars
            (value_sv.data(), value_sv.data() + value_sv.size(), value);
          byte_sizes[algorithm] = value;
          return;
        }
      }
    );

  if (platform_ids.empty())
  {
    std::fprintf
      ( stderr
      , "no stress data found for scenario %s\n"
      , target_scenario.c_str()
      );
    return 1;
  }

  auto const get_rate_value
    { [&] (std::string const& algorithm, std::string const& metric) -> double
      {
        auto const algorithm_it {rates.find (algorithm)};
        if (algorithm_it == std::cend (rates))
        {
          return std::nan ("");
        }

        auto const metric_it {algorithm_it->second.find (metric)};
        if (metric_it == std::cend (algorithm_it->second))
        {
          return std::nan ("");
        }

        if (metric_it->second.values.empty())
        {
          return std::nan ("");
        }

        return ipt_plot::geomean (metric_it->second.values);
      }
    };

  auto const has_high_spread
    { [&] (std::string const& algorithm, std::string const& metric) -> bool
      {
        auto const algorithm_it {rates.find (algorithm)};
        if (algorithm_it == std::cend (rates))
        {
          return false;
        }

        auto const metric_it {algorithm_it->second.find (metric)};
        if (metric_it == std::cend (algorithm_it->second))
        {
          return false;
        }

        return metric_it->second.has_high_spread();
      }
    };

  auto const ipt_algorithm {std::string {"greedy-plus-merge"}};
  auto const ipt_bytes
    {static_cast<double> (byte_sizes[ipt_algorithm])};
  auto const ipt_construct {get_rate_value (ipt_algorithm, "construct")};
  auto const ipt_pos_random {get_rate_value (ipt_algorithm, "pos_random")};
  auto const ipt_pos_all {get_rate_value (ipt_algorithm, "pos_all")};
  auto const ipt_at_random {get_rate_value (ipt_algorithm, "at_random")};
  auto const ipt_at_all {get_rate_value (ipt_algorithm, "at_all")};
  auto const ipt_construct_high
    {has_high_spread (ipt_algorithm, "construct")};
  auto const ipt_pos_random_high
    {has_high_spread (ipt_algorithm, "pos_random")};
  auto const ipt_pos_all_high {has_high_spread (ipt_algorithm, "pos_all")};
  auto const ipt_at_random_high
    {has_high_spread (ipt_algorithm, "at_random")};
  auto const ipt_at_all_high {has_high_spread (ipt_algorithm, "at_all")};

  auto best_bytes {std::nan ("")};
  auto best_construct {std::nan ("")};
  auto best_pos_random {std::nan ("")};
  auto best_pos_all {std::nan ("")};
  auto best_at_random {std::nan ("")};
  auto best_at_all {std::nan ("")};

  auto update_smaller
    { [&] (double& best, double value) -> void
      {
        if (!std::isfinite (value))
        {
          return;
        }
        if (!std::isfinite (best) || value < best)
        {
          best = value;
        }
      }
    };

  auto update_larger
    { [&] (double& best, double value) -> void
      {
        if (!std::isfinite (value))
        {
          return;
        }
        if (!std::isfinite (best) || best < value)
        {
          best = value;
        }
      }
    };

  for (auto const& layout : footprint_layouts())
  {
    auto const bytes {static_cast<double> (byte_sizes[std::string {layout.algorithm}])};
    update_smaller (best_bytes, bytes);
  }

  for (auto const& layout : rate_layouts())
  {
    auto const algorithm {std::string {layout.algorithm}};
    update_larger (best_construct, get_rate_value (algorithm, "construct"));
    update_larger (best_pos_random, get_rate_value (algorithm, "pos_random"));
    update_larger (best_pos_all, get_rate_value (algorithm, "pos_all"));
    update_larger (best_at_random, get_rate_value (algorithm, "at_random"));
    update_larger (best_at_all, get_rate_value (algorithm, "at_all"));
  }

  std::printf ("%% Auto-generated by results/plot/stress-summary.cpp.\n");
  std::printf
    ( "%% scenario %s, grid %s, %ld points, %zu platforms.\n"
    , target_scenario.c_str()
    , grid_label.c_str()
    , point_count
    , platform_ids.size()
    );
  std::printf
    ( "%% Dispersion dagger threshold: (p90-p10)/median > %.2f.\n"
    , dispersion_threshold
    );
  std::printf ("{\\footnotesize\n");
  std::printf ("\\begin{tabular}{@{}lrrrrrrrrrrrr@{}}\n");
  std::printf ("\\toprule\n");
  std::printf
    ( "Layout & \\multicolumn{2}{c}{bytes} & \\multicolumn{2}{c}{construct} & \\multicolumn{2}{c}{\\texttt{pos\\_random}} & \\multicolumn{2}{c}{\\texttt{pos\\_all}} & \\multicolumn{2}{c}{\\texttt{at\\_random}} & \\multicolumn{2}{c}{\\texttt{at\\_all}} \\\\\n"
    );
  std::printf
    ( "       & total & & \\multicolumn{2}{c}{Mpoint/s} & \\multicolumn{2}{c}{Mpos/s} & \\multicolumn{2}{c}{Mpos/s} & \\multicolumn{2}{c}{Mat/s} & \\multicolumn{2}{c}{Mat/s} \\\\\n"
    );
  std::printf
    ( "\\cmidrule(lr){2-3}\\cmidrule(lr){4-5}\\cmidrule(lr){6-7}\\cmidrule(lr){8-9}\\cmidrule(lr){10-11}\\cmidrule(l){12-13}\n"
    );

  auto print_row
    { [&] (LayoutSpec const& layout, bool print_separator_after) -> void
      {
        auto const algorithm {std::string {layout.algorithm}};
        auto const is_ipt {algorithm == ipt_algorithm};
        auto const bytes {static_cast<double> (byte_sizes[algorithm])};
        auto const construct {get_rate_value (algorithm, "construct")};
        auto const pos_random {get_rate_value (algorithm, "pos_random")};
        auto const pos_all {get_rate_value (algorithm, "pos_all")};
        auto const at_random {get_rate_value (algorithm, "at_random")};
        auto const at_all {get_rate_value (algorithm, "at_all")};
        auto const construct_high {has_high_spread (algorithm, "construct")};
        auto const pos_random_high
          {has_high_spread (algorithm, "pos_random")};
        auto const pos_all_high {has_high_spread (algorithm, "pos_all")};
        auto const at_random_high
          {has_high_spread (algorithm, "at_random")};
        auto const at_all_high {has_high_spread (algorithm, "at_all")};

        auto const bytes_text
          { bold_if_best
              ( fmt_grouped_long (static_cast<long> (bytes))
              , equal_with_tolerance (bytes, best_bytes)
              )
          };
        auto const bytes_ratio_text
          { bold_if_best
              ( fmt_ratio (bytes, ipt_bytes)
              , is_strictly_smaller (bytes, ipt_bytes)
              )
          };
        auto const construct_text
          { with_dagger
            ( bold_if_best
              ( fmt_rate_value (rates[algorithm]["construct"])
              , equal_with_tolerance (construct, best_construct)
              )
            , construct_high
            )
          };
        auto const pos_random_text
          { with_dagger
            ( bold_if_best
              ( fmt_rate_value (rates[algorithm]["pos_random"])
              , equal_with_tolerance (pos_random, best_pos_random)
              )
            , pos_random_high
            )
          };
        auto const pos_all_text
          { with_dagger
            ( bold_if_best
              ( fmt_rate_value (rates[algorithm]["pos_all"])
              , equal_with_tolerance (pos_all, best_pos_all)
              )
            , pos_all_high
            )
          };
        auto const at_random_text
          { with_dagger
            ( bold_if_best
              ( fmt_rate_value (rates[algorithm]["at_random"])
              , equal_with_tolerance (at_random, best_at_random)
              )
            , at_random_high
            )
          };
        auto const at_all_text
          { with_dagger
            ( bold_if_best
              ( fmt_rate_value (rates[algorithm]["at_all"])
              , equal_with_tolerance (at_all, best_at_all)
              )
            , at_all_high
            )
          };
        auto const construct_ratio_text
          { with_dagger
            ( bold_if_best
              ( fmt_ratio (construct, ipt_construct)
              , is_strictly_larger (construct, ipt_construct)
              )
            , construct_high || ipt_construct_high
            )
          };
        auto const pos_random_ratio_text
          { with_dagger
            ( bold_if_best
              ( fmt_ratio (pos_random, ipt_pos_random)
              , is_strictly_larger (pos_random, ipt_pos_random)
              )
            , pos_random_high || ipt_pos_random_high
            )
          };
        auto const pos_all_ratio_text
          { with_dagger
            ( bold_if_best
              ( fmt_ratio (pos_all, ipt_pos_all)
              , is_strictly_larger (pos_all, ipt_pos_all)
              )
            , pos_all_high || ipt_pos_all_high
            )
          };
        auto const at_random_ratio_text
          { with_dagger
            ( bold_if_best
              ( fmt_ratio (at_random, ipt_at_random)
              , is_strictly_larger (at_random, ipt_at_random)
              )
            , at_random_high || ipt_at_random_high
            )
          };
        auto const at_all_ratio_text
          { with_dagger
            ( bold_if_best
              ( fmt_ratio (at_all, ipt_at_all)
              , is_strictly_larger (at_all, ipt_at_all)
              )
            , at_all_high || ipt_at_all_high
            )
          };
        auto const dominated
          { !is_ipt
            && !equal_with_tolerance (bytes, best_bytes)
            && !equal_with_tolerance (construct, best_construct)
            && !equal_with_tolerance (pos_random, best_pos_random)
            && !equal_with_tolerance (pos_all, best_pos_all)
            && !equal_with_tolerance (at_random, best_at_random)
            && !equal_with_tolerance (at_all, best_at_all)
            && !is_strictly_smaller (bytes, ipt_bytes)
            && !is_strictly_larger (construct, ipt_construct)
            && !is_strictly_larger (pos_random, ipt_pos_random)
            && !is_strictly_larger (pos_all, ipt_pos_all)
            && !is_strictly_larger (at_random, ipt_at_random)
            && !is_strictly_larger (at_all, ipt_at_all)
          };

        std::printf
          ( "%s & %s & %s & %s & %s & %s & %s & %s & %s & %s & %s & %s & %s \\\\\n"
          , grey_if_dominated (std::string {layout.label}, dominated).c_str()
          , grey_if_dominated (bytes_text, dominated).c_str()
          , grey_if_dominated (bytes_ratio_text, dominated).c_str()
          , grey_if_dominated (construct_text, dominated).c_str()
          , grey_if_dominated (construct_ratio_text, dominated).c_str()
          , grey_if_dominated (pos_random_text, dominated).c_str()
          , grey_if_dominated (pos_random_ratio_text, dominated).c_str()
          , grey_if_dominated (pos_all_text, dominated).c_str()
          , grey_if_dominated (pos_all_ratio_text, dominated).c_str()
          , grey_if_dominated (at_random_text, dominated).c_str()
          , grey_if_dominated (at_random_ratio_text, dominated).c_str()
          , grey_if_dominated (at_all_text, dominated).c_str()
          , grey_if_dominated (at_all_ratio_text, dominated).c_str()
          );

        if (print_separator_after)
        {
          std::printf ("\\midrule\n");
        }
      }
    };

  for (auto i {std::size_t {0}}; i < rate_layouts().size(); ++i)
  {
    print_row (rate_layouts()[i], i == 0);
  }

  std::printf ("\\bottomrule\n");
  std::printf ("\\end{tabular}\n");
  std::printf ("}\n");

  return 0;
}
