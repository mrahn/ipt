// Aggregate persistence/storage measurements across the seven smaller
// structured multiple-survey scenarios on the reference platform.
// Emits a stand-alone LaTeX snippet with two left-aligned tabulars:
// an upper panel for size, serialization, and mmap-load latency, and
// a lower panel for steady-state mmap-backed throughput. The default
// IPT configuration is shown explicitly. The non-IPT layouts pool the
// repetitions from both storage binaries.

#include "IPTPlot.hpp"

#include <array>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <format>
#include <map>
#include <ranges>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace
{
  constexpr auto dispersion_threshold {0.20};

  struct SummaryRow
  {
    std::vector<std::string> configs;
    std::string algorithm;
    std::string label;
  };

  auto to_double (std::string_view sv) -> double
  {
    auto value {0.0};
    std::from_chars (sv.data(), sv.data() + sv.size(), value);
    return value;
  }

  auto to_long (std::string_view sv) -> long
  {
    auto value {0L};
    std::from_chars (sv.data(), sv.data() + sv.size(), value);
    return value;
  }

  // The default IPT row is reported explicitly. The non-IPT layouts
  // pool the repetitions from both storage binaries so the summary
  // uses every available storage run without duplicating baseline
  // rows.
  auto summary_rows
    ( std::string const& default_config
    , std::string const& baseline_config
    ) -> std::vector<SummaryRow>
  {
    auto rows
      { std::vector<SummaryRow>
        { SummaryRow
            { {default_config}
            , "greedy-plus-merge-mmap"
            , std::format
                ("\\textsc{{IPT}} (\\texttt{{{}}})", default_config)
            }
        }
      };

    auto baseline_configs {std::vector<std::string> {default_config}};
    if (! (baseline_config == default_config))
    {
      baseline_configs.push_back (baseline_config);
    }

    rows.push_back
      ( SummaryRow
        { baseline_configs
        , "sorted-points-mmap"
        , "\\textsc{SortedPoints}"
        }
      );
    rows.push_back
      ( SummaryRow
        { baseline_configs
        , "dense-bitset-mmap"
        , "\\textsc{DenseBitset}"
        }
      );
    rows.push_back
      ( SummaryRow
        { baseline_configs
        , "block-bitmap-mmap"
        , "\\textsc{BlockBitmap}"
        }
      );
    rows.push_back
      ( SummaryRow
        { baseline_configs
        , "roaring-mmap"
        , "\\textsc{Roaring}"
        }
      );
    rows.push_back
      ( SummaryRow
        { baseline_configs
        , "lex-run-mmap"
        , "\\textsc{LexRun}"
        }
      );
    rows.push_back
      ( SummaryRow
        { baseline_configs
        , "row-csr-k1-mmap"
        , "\\textsc{RowCSR-k1}"
        }
      );
    rows.push_back
      ( SummaryRow
        { baseline_configs
        , "row-csr-k2-mmap"
        , "\\textsc{RowCSR-k2}"
        }
      );

    return rows;
  }

  // Per-(algorithm, scenario) bag of samples for one (metric, column)
  // pair.  We accumulate the per-seed mean for each scenario and then
  // take the geometric mean across the seven smaller structured
  // multiple-survey scenarios.
  auto repetition_spread (std::vector<double> const& values) -> double
  {
    if (values.size() < 2)
    {
      return 0.0;
    }

    auto const median_value {ipt_plot::percentile (0.50, values)};
    if (!std::isfinite (median_value) || median_value <= 0.0)
    {
      return 0.0;
    }

    auto const low {ipt_plot::percentile (0.10, values)};
    auto const high {ipt_plot::percentile (0.90, values)};
    return (high - low) / median_value;
  }

  struct Sample
  {
    std::map<std::string, std::vector<double>> per_scenario;

    auto add (std::string const& scenario, double v) -> void
    {
      if (v > 0.0)
      {
        per_scenario[scenario].push_back (v);
      }
    }

    auto geom_across_scenarios() const -> double
    {
      auto means {std::vector<double>{}};
      means.reserve (per_scenario.size());
      std::ranges::for_each
        ( per_scenario
        , [&] (auto const& kv)
          {
            if (!kv.second.empty())
            {
              auto const sum
                {std::ranges::fold_left (kv.second, 0.0, std::plus{})};
              means.push_back (sum / static_cast<double> (kv.second.size()));
            }
          }
        );
      return ipt_plot::geomean (means);
    }

    auto empty() const -> bool
    {
      return per_scenario.empty();
    }

    auto has_high_repetition_spread() const -> bool
    {
      return std::ranges::any_of
        ( per_scenario
        , [] (auto const& scenario_values)
          {
            return repetition_spread (scenario_values.second)
              > dispersion_threshold;
          }
        );
    }
  };

  auto fmt_bytes (double v) -> std::string
  {
    if (!std::isfinite (v) || v <= 0.0)
    {
      return "--";
    }

    char buf[64];
    std::snprintf (buf, sizeof (buf), "%.0f", v);

    auto const digits {std::string {buf}};
    if (digits.size() <= 3)
    {
      return digits;
    }

    auto grouped {std::string {}};
    grouped.reserve (digits.size() + 2 * ((digits.size() - 1) / 3));

    auto first_group_size {digits.size() % 3};
    if (first_group_size == 0)
    {
      first_group_size = 3;
    }

    grouped.append (digits, 0, first_group_size);

    for (auto offset {first_group_size}; offset < digits.size(); offset += 3)
    {
      grouped += "\\,";
      grouped.append (digits, offset, 3);
    }

    return grouped;
  }

  auto fmt_bytes_per_point (double v) -> std::string
  {
    if (!std::isfinite (v) || v <= 0.0)
    {
      return "--";
    }

    char buf[64];
    std::snprintf (buf, sizeof (buf), "%.2f", v);
    return buf;
  }

  auto fmt_us (double ns) -> std::string
  {
    if (!std::isfinite (ns) || ns <= 0.0)
    {
      return "--";
    }

    char buf[64];
    auto const us {ns / 1000.0};
    std::snprintf (buf, sizeof (buf), "%.2f", us);
    return buf;
  }

  auto fmt_rate (double v) -> std::string
  {
    if (!std::isfinite (v) || v <= 0.0)
    {
      return "--";
    }

    char buf[64];
    std::snprintf (buf, sizeof (buf), "%.2f", v);
    return buf;
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
    if (ratio < 0.005)
    {
      char buffer[64];
      std::snprintf (buffer, sizeof (buffer), "%.3fx", ratio);
      return buffer;
    }

    if (ratio < 0.05)
    {
      char buffer[64];
      std::snprintf (buffer, sizeof (buffer), "%.2fx", ratio);
      return buffer;
    }

    if (0.95 < ratio && ratio < 1.05)
    {
      char buffer[64];
      std::snprintf (buffer, sizeof (buffer), "%.2fx", ratio);
      return buffer;
    }

    if (ratio > 10.0)
    {
      char buffer[64];
      std::snprintf (buffer, sizeof (buffer), "%.0fx", ratio);
      return buffer;
    }

    char buffer[64];
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

  auto bold_if_best (std::string value, bool is_best) -> std::string
  {
    if (!is_best)
    {
      return value;
    }

    return std::format ("\\textbf{{{}}}", value);
  }

  auto with_dagger (std::string text) -> std::string
  {
    return "$^{\\dagger}$" + text;
  }

  auto with_dagger (std::string text, bool flag) -> std::string
  {
    if (!flag)
    {
      return text;
    }

    return with_dagger (text);
  }

  auto is_strictly_smaller (double left, double right) -> bool
  {
    return left < right && !equal_with_tolerance (left, right);
  }

  auto is_strictly_larger (double left, double right) -> bool
  {
    return right < left && !equal_with_tolerance (left, right);
  }

  constexpr auto scenario_order() noexcept -> std::array<std::string_view, 7>
  {
    return
      { "multiple-survey-2-l"
      , "multiple-survey-3-steps"
      , "multiple-survey-4-overlap"
      , "multiple-survey-5-mixed"
      , "multiple-survey-6-bands"
      , "multiple-survey-7-large"
      , "multiple-survey-8-threed"
      };
  }

  auto is_storage_scenario (std::string_view scenario) -> bool
  {
    auto const scenarios {scenario_order()};
    return std::ranges::find (scenarios, scenario) != std::cend (scenarios);
  }
}

auto main() -> int
{
  using namespace ipt_plot;

  auto const default_config {std::string {default_ipt_config()}};
  auto const baseline_config {std::string {reference_cache_config()}};
  auto const platforms {load_platforms()};
  auto const reference {reference_platform_id (platforms)};

  // [config][algorithm][metric] -> Sample
  using AlgMap = std::map<std::string, std::map<std::string, Sample>>;
  auto data {std::map<std::string, AlgMap>{}};

  visit_rows
    ( platforms
    , [&] (auto const& platform, auto const& meta, auto const& row)
      {
        if (meta.cache_kind != "storage")
        {
          return;
        }
        if (platform.id != reference)
        {
          return;
        }
        if (meta.config != default_config && meta.config != baseline_config)
        {
          return;
        }

        auto const scenario {std::string {row.value ("scenario")}};
        if (!is_storage_scenario (scenario))
        {
          return;
        }

        auto const algorithm {std::string {row.value ("algorithm")}};
        auto const metric {std::string {row.value ("metric")}};
        auto& bag {data[meta.config][algorithm][metric]};

        if (metric == "bytes_on_disk")
        {
          auto const v {to_long (row.value ("value"))};
          auto const point_count {to_long (row.value ("point_count"))};
          bag.add (scenario, static_cast<double> (v));
          if (point_count > 0)
          {
            data[meta.config][algorithm]["bytes_per_point"].add
              ( scenario
              , static_cast<double> (v)
                / static_cast<double> (point_count)
              );
          }
          return;
        }
        if (metric == "first_pos")
        {
          bag.add (scenario, to_double (row.value ("ns_per_pos")));
          return;
        }
        if (metric == "first_at")
        {
          bag.add (scenario, to_double (row.value ("ns_per_at")));
          return;
        }
        if (metric == "first_restrict")
        {
          bag.add (scenario, to_double (row.value ("ns_per_restrict")));
          return;
        }
        if (metric == "load")
        {
          bag.add (scenario, to_double (row.value ("ns_per_load")));
          return;
        }
        if (metric == "serialize")
        {
          bag.add (scenario, to_double (row.value ("ns_per_point")));
          return;
        }
        if (metric == "pos_random")
        {
          bag.add (scenario, to_double (row.value ("Mpos_per_sec")));
          return;
        }
        if (metric == "pos_all")
        {
          bag.add (scenario, to_double (row.value ("Mpos_per_sec")));
          return;
        }
        if (metric == "at_random")
        {
          bag.add (scenario, to_double (row.value ("Mat_per_sec")));
          return;
        }
        if (metric == "at_all")
        {
          bag.add (scenario, to_double (row.value ("Mat_per_sec")));
          return;
        }
      }
    );

  if (data.empty())
  {
    std::fprintf (stderr, "no storage rows found on reference platform\n");
    return 1;
  }

  std::printf ("%% Auto-generated by results/plot/storage-summary.cpp.\n");
  std::printf
    ( "%% Reference platform: %s, geom-mean across the seven smaller structured scenarios\n"
    , reference.c_str()
    );
  std::printf ("%% Default IPT configuration: %s.\n", default_config.c_str());
  std::printf
    ( "%% Non-IPT rows pool both storage binaries"
      " (secondary IPT configuration: %s).\n"
    , baseline_config.c_str()
    );
  std::printf
    ( "%% Dispersion dagger threshold: (p90-p10)/median > %.2f.\n"
    , dispersion_threshold
    );

  auto const rows {summary_rows (default_config, baseline_config)};

  auto const merged_sample
    { [&]
      ( SummaryRow const& row
      , std::string const& metric
      ) -> Sample
      {
        auto merged {Sample {}};

        std::ranges::for_each
          ( row.configs
          , [&] (auto const& config)
            {
              auto const config_iterator {data.find (config)};
              if (config_iterator == data.end())
              {
                return;
              }

              auto const algorithm_iterator
                {config_iterator->second.find (row.algorithm)};
              if (algorithm_iterator == std::cend (config_iterator->second))
              {
                return;
              }

              auto const metric_iterator
                {algorithm_iterator->second.find (metric)};
              if (metric_iterator == std::cend (algorithm_iterator->second))
              {
                return;
              }

              std::ranges::for_each
                ( metric_iterator->second.per_scenario
                , [&] (auto const& kv)
                  {
                    auto& destination {merged.per_scenario[kv.first]};
                    destination.insert
                      ( std::end (destination)
                      , std::cbegin (kv.second)
                      , std::cend (kv.second)
                      );
                  }
                );
            }
          );

        return merged;
      }
    };

  auto const get_metric_value
    { [&] (SummaryRow const& row, std::string const& metric) -> double
      {
        auto const merged {merged_sample (row, metric)};
        if (merged.empty())
        {
          return std::nan ("");
        }

        return merged.geom_across_scenarios();
      }
    };

  auto const has_high_spread
    { [&] (SummaryRow const& row, std::string const& metric) -> bool
      {
        auto const merged {merged_sample (row, metric)};
        return merged.has_high_repetition_spread();
      }
    };

  std::printf ("{\\footnotesize\n");
  std::printf ("\\begin{tabular}{@{}l@{}}\n");
  std::printf ("\\begin{tabular}{lrrrrrrr}\n");
  std::printf ("\\toprule\n");
  std::printf
    ( "Size and latency & \\multicolumn{3}{c}{bytes}"
      " & \\multicolumn{2}{c}{serialize}"
      " & \\multicolumn{2}{c}{mmap load} \\\\\n"
    );
  std::printf
    ( "       & total & /point &"
      " & \\multicolumn{2}{c}{ns/point}"
      " & \\multicolumn{2}{c}{\\(\\mu\\)s} \\\\\n"
    );
  std::printf
    ( "\\cmidrule(lr){2-4}\\cmidrule(lr){5-6}\\cmidrule(l){7-8}\n"
    );

  auto const& ipt_default_row {rows.front()};
  auto const ipt_bytes_on_disk
    {get_metric_value (ipt_default_row, "bytes_on_disk")};
  auto const ipt_bytes_per_point
    {get_metric_value (ipt_default_row, "bytes_per_point")};
  auto const ipt_serialize {get_metric_value (ipt_default_row, "serialize")};
  auto const ipt_load {get_metric_value (ipt_default_row, "load")};
  auto const ipt_pos_random {get_metric_value (ipt_default_row, "pos_random")};
  auto const ipt_pos_all {get_metric_value (ipt_default_row, "pos_all")};
  auto const ipt_at_random {get_metric_value (ipt_default_row, "at_random")};
  auto const ipt_at_all {get_metric_value (ipt_default_row, "at_all")};
  auto const ipt_bytes_on_disk_high
    {has_high_spread (ipt_default_row, "bytes_on_disk")};
  auto const ipt_serialize_high
    {has_high_spread (ipt_default_row, "serialize")};
  auto const ipt_load_high {has_high_spread (ipt_default_row, "load")};
  auto const ipt_pos_random_high
    {has_high_spread (ipt_default_row, "pos_random")};
  auto const ipt_pos_all_high {has_high_spread (ipt_default_row, "pos_all")};
  auto const ipt_at_random_high
    {has_high_spread (ipt_default_row, "at_random")};
  auto const ipt_at_all_high {has_high_spread (ipt_default_row, "at_all")};
  if ( !std::isfinite (ipt_bytes_on_disk)
    || !std::isfinite (ipt_bytes_per_point)
    || !std::isfinite (ipt_serialize)
    || !std::isfinite (ipt_load)
    || !std::isfinite (ipt_pos_random)
    || !std::isfinite (ipt_pos_all)
    || !std::isfinite (ipt_at_random)
    || !std::isfinite (ipt_at_all)
     )
  {
    std::fprintf (stderr, "missing IPT baseline row in storage summary\n");
    return 1;
  }

  auto best_bytes_on_disk {std::nan ("")};
  auto best_bytes_per_point {std::nan ("")};
  auto best_serialize {std::nan ("")};
  auto best_load {std::nan ("")};
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

  std::ranges::for_each
    ( rows
    , [&] (auto const& row)
      {
        update_smaller
          (best_bytes_on_disk, get_metric_value (row, "bytes_on_disk"));
        update_smaller
          (best_bytes_per_point, get_metric_value (row, "bytes_per_point"));
        update_smaller (best_serialize, get_metric_value (row, "serialize"));
        update_smaller (best_load, get_metric_value (row, "load"));
        update_larger (best_pos_random, get_metric_value (row, "pos_random"));
        update_larger (best_pos_all, get_metric_value (row, "pos_all"));
        update_larger (best_at_random, get_metric_value (row, "at_random"));
        update_larger (best_at_all, get_metric_value (row, "at_all"));
      }
    );

  auto constexpr ipt_row_count {std::size_t {1}};

  auto grey_if_dominated
    { [] (std::string const& text, bool dominated) -> std::string
      {
        if (!dominated)
        {
          return text;
        }

        return std::format ("\\textcolor[gray]{{0.55}}{{{}}}", text);
      }
    };

  auto print_size_row
    { [&]
      ( SummaryRow const& row
      , bool print_separator_after
      ) -> void
      {
        auto const bytes_on_disk {get_metric_value (row, "bytes_on_disk")};
        if (!std::isfinite (bytes_on_disk))
        {
          return;
        }

        auto const bytes_per_point {get_metric_value (row, "bytes_per_point")};
        auto const serialize {get_metric_value (row, "serialize")};
        auto const load {get_metric_value (row, "load")};
        auto const bytes_on_disk_high
          {has_high_spread (row, "bytes_on_disk")};
        auto const serialize_high {has_high_spread (row, "serialize")};
        auto const load_high {has_high_spread (row, "load")};

        auto const bytes_on_disk_text
          { with_dagger
            ( bold_if_best
              ( fmt_bytes (bytes_on_disk)
              , equal_with_tolerance (bytes_on_disk, best_bytes_on_disk)
              )
            , bytes_on_disk_high
            )
          };
        auto const bytes_per_point_text
          { with_dagger
            ( bold_if_best
              ( fmt_bytes_per_point (bytes_per_point)
              , equal_with_tolerance
                (bytes_per_point, best_bytes_per_point)
              )
            , bytes_on_disk_high
            )
          };
        auto const serialize_text
          { with_dagger
            ( bold_if_best
              ( fmt_rate (serialize)
              , equal_with_tolerance (serialize, best_serialize)
              )
            , serialize_high
            )
          };
        auto const load_text
          { with_dagger
            ( bold_if_best
              ( fmt_us (load)
              , equal_with_tolerance (load, best_load)
              )
            , load_high
            )
          };
        auto const bytes_ratio_text
          { with_dagger
            ( bold_if_best
              ( fmt_ratio (bytes_on_disk, ipt_bytes_on_disk)
              , is_strictly_smaller (bytes_on_disk, ipt_bytes_on_disk)
              )
            , bytes_on_disk_high || ipt_bytes_on_disk_high
            )
          };
        auto const serialize_ratio_text
          { with_dagger
            ( bold_if_best
              ( fmt_ratio (serialize, ipt_serialize)
              , is_strictly_smaller (serialize, ipt_serialize)
              )
            , serialize_high || ipt_serialize_high
            )
          };
        auto const load_ratio_text
          { with_dagger
            ( bold_if_best
              ( fmt_ratio (load, ipt_load)
              , is_strictly_smaller (load, ipt_load)
              )
            , load_high || ipt_load_high
            )
          };
        auto const dominated
          { !row.label.starts_with ("\\textsc{IPT}")
            && !equal_with_tolerance (bytes_on_disk, best_bytes_on_disk)
            && !equal_with_tolerance (bytes_per_point, best_bytes_per_point)
            && !equal_with_tolerance (serialize, best_serialize)
            && !equal_with_tolerance (load, best_load)
            && !is_strictly_smaller (bytes_on_disk, ipt_bytes_on_disk)
            && !is_strictly_smaller (serialize, ipt_serialize)
            && !is_strictly_smaller (load, ipt_load)
          };
        auto const row_label_text {grey_if_dominated (row.label, dominated)};
        auto const row_bytes_on_disk_text
          {grey_if_dominated (bytes_on_disk_text, dominated)};
        auto const row_bytes_per_point_text
          {grey_if_dominated (bytes_per_point_text, dominated)};
        auto const row_bytes_ratio_text
          {grey_if_dominated (bytes_ratio_text, dominated)};
        auto const row_serialize_text
          {grey_if_dominated (serialize_text, dominated)};
        auto const row_serialize_ratio_text
          {grey_if_dominated (serialize_ratio_text, dominated)};
        auto const row_load_text {grey_if_dominated (load_text, dominated)};
        auto const row_load_ratio_text
          {grey_if_dominated (load_ratio_text, dominated)};

        std::printf
          ( "%s & %s & %s & %s & %s & %s & %s & %s \\\\\n"
          , row_label_text.c_str()
          , row_bytes_on_disk_text.c_str()
          , row_bytes_per_point_text.c_str()
          , row_bytes_ratio_text.c_str()
          , row_serialize_text.c_str()
          , row_serialize_ratio_text.c_str()
          , row_load_text.c_str()
          , row_load_ratio_text.c_str()
          );

        if (print_separator_after)
        {
          std::printf ("\\midrule\n");
        }
      }
    };

  for (auto row_index {std::size_t {0}}; row_index < rows.size(); ++row_index)
  {
    print_size_row (rows[row_index], row_index + 1 == ipt_row_count);
  }

  std::printf ("\\bottomrule\n");
  std::printf ("\\end{tabular}\\\\\\\\\n");
  std::printf ("\\begin{tabular}{lrrrrrrrr}\n");
  std::printf ("\\toprule\n");
  std::printf
    ( "Throughput & \\multicolumn{2}{c}{\\texttt{pos\\_random}}"
      " & \\multicolumn{2}{c}{\\texttt{pos\\_all}}"
      " & \\multicolumn{2}{c}{\\texttt{at\\_random}}"
      " & \\multicolumn{2}{c}{\\texttt{at\\_all}} \\\\\n"
    );
  std::printf
    ( "       & \\multicolumn{2}{c}{Mpos/s}"
      " & \\multicolumn{2}{c}{Mpos/s}"
      " & \\multicolumn{2}{c}{Mat/s}"
      " & \\multicolumn{2}{c}{Mat/s} \\\\\n"
    );
  std::printf
    ( "\\cmidrule(lr){2-3}"
      "\\cmidrule(lr){4-5}"
      "\\cmidrule(lr){6-7}"
      "\\cmidrule(l){8-9}\n"
    );

  auto print_lower_row
    { [&]
      ( SummaryRow const& row
      , bool print_separator_after
      ) -> void
      {
        auto const pos_random {get_metric_value (row, "pos_random")};
        if (!std::isfinite (pos_random))
        {
          return;
        }

        auto const pos_all {get_metric_value (row, "pos_all")};
        auto const at_random {get_metric_value (row, "at_random")};
        auto const at_all {get_metric_value (row, "at_all")};
        auto const pos_random_high {has_high_spread (row, "pos_random")};
        auto const pos_all_high {has_high_spread (row, "pos_all")};
        auto const at_random_high {has_high_spread (row, "at_random")};
        auto const at_all_high {has_high_spread (row, "at_all")};

        auto const pos_random_text
          { with_dagger
            ( bold_if_best
              ( fmt_rate (pos_random)
              , equal_with_tolerance (pos_random, best_pos_random)
              )
            , pos_random_high
            )
          };
        auto const pos_all_text
          { with_dagger
            ( bold_if_best
              ( fmt_rate (pos_all)
              , equal_with_tolerance (pos_all, best_pos_all)
              )
            , pos_all_high
            )
          };
        auto const at_random_text
          { with_dagger
            ( bold_if_best
              ( fmt_rate (at_random)
              , equal_with_tolerance (at_random, best_at_random)
              )
            , at_random_high
            )
          };
        auto const at_all_text
          { with_dagger
            ( bold_if_best
              ( fmt_rate (at_all)
              , equal_with_tolerance (at_all, best_at_all)
              )
            , at_all_high
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
          { !row.label.starts_with ("\\textsc{IPT}")
            && !equal_with_tolerance (pos_random, best_pos_random)
            && !equal_with_tolerance (pos_all, best_pos_all)
            && !equal_with_tolerance (at_random, best_at_random)
            && !equal_with_tolerance (at_all, best_at_all)
            && !is_strictly_larger (pos_random, ipt_pos_random)
            && !is_strictly_larger (pos_all, ipt_pos_all)
            && !is_strictly_larger (at_random, ipt_at_random)
            && !is_strictly_larger (at_all, ipt_at_all)
          };
        auto const row_label_text {grey_if_dominated (row.label, dominated)};
        auto const row_pos_random_text
          {grey_if_dominated (pos_random_text, dominated)};
        auto const row_pos_random_ratio_text
          {grey_if_dominated (pos_random_ratio_text, dominated)};
        auto const row_pos_all_text
          {grey_if_dominated (pos_all_text, dominated)};
        auto const row_pos_all_ratio_text
          {grey_if_dominated (pos_all_ratio_text, dominated)};
        auto const row_at_random_text
          {grey_if_dominated (at_random_text, dominated)};
        auto const row_at_random_ratio_text
          {grey_if_dominated (at_random_ratio_text, dominated)};
        auto const row_at_all_text
          {grey_if_dominated (at_all_text, dominated)};
        auto const row_at_all_ratio_text
          {grey_if_dominated (at_all_ratio_text, dominated)};

        std::printf
          ( "%s & %s & %s & %s & %s & %s & %s & %s & %s \\\\\n"
          , row_label_text.c_str()
          , row_pos_random_text.c_str()
          , row_pos_random_ratio_text.c_str()
          , row_pos_all_text.c_str()
          , row_pos_all_ratio_text.c_str()
          , row_at_random_text.c_str()
          , row_at_random_ratio_text.c_str()
          , row_at_all_text.c_str()
          , row_at_all_ratio_text.c_str()
          );

        if (print_separator_after)
        {
          std::printf ("\\midrule\n");
        }
      }
    };

  for (auto row_index {std::size_t {0}}; row_index < rows.size(); ++row_index)
  {
    print_lower_row (rows[row_index], row_index + 1 == ipt_row_count);
  }

  std::printf ("\\bottomrule\n");
  std::printf ("\\end{tabular}\n");
  std::printf ("\\end{tabular}}\n");

  return 0;
}
