// Shared utilities for the IPT result-extraction

#pragma once

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ipt_plot
{
  struct ScenarioInfo
  {
    std::string_view name;
    std::string_view label;
  };

  struct DistributionSummary
  {
    double center{};
    double p10{};
    double p90{};
  };

  struct Platform
  {
    std::string id;
    std::filesystem::path result_dir;
    std::string os_raw;
    std::string os_label;
    std::string kernel_label;
    std::string cpu_raw;
    std::string cpu_label;
    std::string compiler_raw;
    std::string compiler_family;
    std::string compiler_version;
    std::string compiler_label;
    std::string platform_label;
  };

  struct FileMetadata
  {
    std::string cache_kind;
    std::string config;
    std::string reference_config;
  };

  class MappedFile
  {
  public:
    explicit MappedFile (std::filesystem::path const&);
    ~MappedFile() noexcept;
    MappedFile (MappedFile const&) = delete;
    MappedFile (MappedFile&& other) noexcept = delete;
    auto operator= (MappedFile const&) -> MappedFile& = delete;
    auto operator= (MappedFile&& other) noexcept -> MappedFile& = delete;

    [[nodiscard]] auto data() const noexcept -> std::string_view;

  private:
    struct OpenFD
    {
      int fd;
      explicit OpenFD (int fd_) noexcept : fd {fd_} {}
      ~OpenFD() noexcept;
      OpenFD (OpenFD const&) = delete;
      OpenFD (OpenFD&& other) noexcept = delete;
      auto operator= (OpenFD const&) -> OpenFD& = delete;
      auto operator= (OpenFD&& other) noexcept = delete;
    };

    OpenFD _fd {-1};
    std::size_t _size {0};
    char* _data {nullptr};
  };

  constexpr auto default_ipt_config() noexcept -> std::string_view
  {
    return "c0r1e1l0";
  }

  constexpr auto reference_cache_config() noexcept -> std::string_view
  {
    return "c0r0e0l0";
  }

  auto structured_scenario_infos() noexcept -> std::span<ScenarioInfo const>;
  auto structured_scenarios() -> std::vector<std::string>;
  auto structured_scenario_label (std::string_view scenario) -> std::string;
  auto grid_order() noexcept -> std::span<std::string_view const>;
  auto density_order() noexcept -> std::span<int const>;

  auto mean (std::vector<double> const&) -> double;
  auto geomean (std::vector<double> const&) -> double;
  auto percentile (double p, std::vector<double>) -> double;

  auto label_os (std::string const& os_raw) -> std::string;
  auto label_cpu (std::string const& cpu_raw) -> std::string;
  auto parse_platform (std::filesystem::path const&) -> Platform;
  auto load_platforms() -> std::vector<Platform>;
  auto reference_platform_id (std::vector<Platform> const&) -> std::string;
  auto parse_result_file
    ( std::filesystem::path const&
    ) -> std::optional<FileMetadata>
    ;

  struct Row
  {
    explicit Row (std::string_view line);
    auto value (std::string_view key) const noexcept -> std::string_view;
    auto measured_value() const -> double;

  private:
    std::unordered_map<std::string_view, std::string_view> _map;
  };

  auto is_multiple_survey (std::string_view scenario) -> bool;
  auto parse_random_density (std::string_view scenario) -> std::optional<int>;

  template<typename Range>
    requires (std::ranges::forward_range<Range>)
    auto string_view_from (Range&& range) -> std::string_view
  {
    if (std::ranges::empty (range))
    {
      return {};
    }
    auto const size {static_cast<std::size_t> (std::ranges::distance (range))};
    auto const* data {std::addressof (*std::ranges::begin (range))};
    return {data, size};
  }

  template<typename Assoc, typename Key>
    auto value_ptr
      ( Assoc const& assoc
      , Key const& key
      ) -> typename Assoc::mapped_type const*
  {
    auto const kv {assoc.find (key)};
    if (kv == std::cend (assoc))
    {
      return nullptr;
    }

    return std::addressof (kv->second);
  }

  template<typename Center>
    auto summarize
      ( std::vector<double> const& values
      , Center center
      ) -> DistributionSummary
  {
    if (values.empty())
    {
      return {};
    }

    return DistributionSummary
      { center (values)
      , percentile (0.10, values)
      , percentile (0.90, values)
      };
  }

  template<typename Center>
    auto summarize_or_default
      ( std::vector<double> const* values
      , Center center
      ) -> DistributionSummary
  {
    if (values == nullptr)
    {
      return {};
    }

    return summarize (*values, center);
  }

  template<typename F>
    auto for_each_line (std::string_view text, F&& f) -> void
  {
    std::ranges::for_each
      ( std::views::split (text, '\n')
      , [&] (auto&& line_range)
        {
          auto const line {string_view_from (line_range)};
          if (!line.empty())
          {
            std::invoke (f, line);
          }
        }
      );
  }

  template<typename Callback>
    auto visit_rows
      ( std::vector<Platform> const& platforms
      , Callback&& callback
      ) -> void
  {
    std::ranges::for_each
      ( platforms
      , [&] (auto const& platform)
          {
            auto files {std::vector<std::filesystem::path>{}};

            std::ranges::for_each
              ( std::filesystem::directory_iterator {platform.result_dir}
              , [&] (auto const& entry)
                {
                  if ( entry.is_regular_file()
                     && entry.path().extension() == ".txt"
                     )
                  {
                    files.push_back (entry.path());
                  }
                }
              );

            std::ranges::sort (files);

            std::ranges::for_each
              ( files
              , [&] (auto const& file)
                {
                  if (auto const meta {parse_result_file (file)})
                  {
                    auto const mapped {MappedFile {file}};
                    for_each_line
                      ( mapped.data()
                      , [&] (auto line)
                        {
                          callback (platform, *meta, Row {line});
                        }
                      );
                  }
                }
              );
          }
      );
  }

  template<typename Callback>
    auto visit_rows (Callback&& callback) -> void
  {
    visit_rows (load_platforms(), std::forward<Callback> (callback));
  }
}
