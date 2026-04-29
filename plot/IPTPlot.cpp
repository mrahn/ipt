// Shared utilities for the IPT result-extraction programs.

#include "IPTPlot.hpp"

#include <array>
#include <charconv>
#include <cctype>
#include <cmath>
#include <compare>
#include <cstring>
#include <format>
#include <fcntl.h>
#include <ranges>
#include <regex>
#include <stdexcept>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace ipt_plot
{
  namespace
  {
    auto scenario_info_storage() noexcept -> std::span<ScenarioInfo const>
    {
      static auto const infos
        { std::to_array<ScenarioInfo>
          ( { {"multiple-survey-2-l", "2-L"}
            , {"multiple-survey-3-steps", "3-steps"}
            , {"multiple-survey-4-overlap", "4-overlap"}
            , {"multiple-survey-5-mixed", "5-mixed"}
            , {"multiple-survey-6-bands", "6-bands"}
            , {"multiple-survey-7-large", "7-large"}
            , {"multiple-survey-8-threed", "12-3D"}
            , {"multiple-survey-10-sixteen", "16-3D"}
            , {"multiple-survey-11-thirtysix", "36-3D"}
            }
          )
        };

      return infos;
    }

    auto grid_storage() noexcept -> std::span<std::string_view const>
    {
      static auto const grids
        { std::to_array<std::string_view>
          ( { "10x10x10"
            , "100x10x10"
            , "100x100x10"
            , "100x100x100"
            }
          )
        };

      return grids;
    }

    auto density_storage() noexcept -> std::span<int const>
    {
      static auto const densities
        { std::to_array<int>
          ( {1, 2, 5, 10, 25, 50, 75, 90, 95, 98, 99, 100}
          )
        };

      return densities;
    }

    auto is_cache_config (std::string_view text) -> bool
    {
      return text.size() == 8
        && text[0] == 'c'
        && (text[1] == '0' || text[1] == '1')
        && text[2] == 'r'
        && (text[3] == '0' || text[3] == '1')
        && text[4] == 'e'
        && (text[5] == '0' || text[5] == '1')
        && text[6] == 'l'
        && (text[7] == '0' || text[7] == '1')
        ;
    }

    auto kernel_components (std::string_view kernel) -> std::vector<int>
    {
      auto components {std::vector<int>{}};
      auto index {std::size_t {0}};

      while (index < kernel.size())
      {
        while ( index < kernel.size()
              && !std::isdigit (static_cast<unsigned char> (kernel[index]))
              )
        {
          ++index;
        }

        if (index == kernel.size())
        {
          break;
        }

        auto end {index};
        while ( end < kernel.size()
              && std::isdigit (static_cast<unsigned char> (kernel[end]))
              )
        {
          ++end;
        }

        {
          auto value {0};
          auto const result
            { std::from_chars ( kernel.data() + index
                              , kernel.data() + end
                              , value
                              )
            };

          if (result.ec == std::errc{})
          {
            components.push_back (value);
          }
        }

        index = end;
      }

      return components;
    }

    auto compare_kernel_labels
      ( std::string_view lhs
      , std::string_view rhs
      ) -> std::strong_ordering
    {
      auto const lhs_components {kernel_components (lhs)};
      auto const rhs_components {kernel_components (rhs)};

      auto const count
        { lhs_components.size() > rhs_components.size()
        ? lhs_components.size()
        : rhs_components.size()
        };

      for (auto index {std::size_t{0}}; index < count; ++index)
      {
        auto const lhs_component
          { index < lhs_components.size() ? lhs_components[index] : 0
          };
        auto const rhs_component
          { index < rhs_components.size() ? rhs_components[index] : 0
          };

        if (auto const order {lhs_component <=> rhs_component}; order != 0)
        {
          return order;
        }
      }

      return lhs <=> rhs;
    }

    auto same_platform_variant
      ( Platform const& lhs
      , Platform const& rhs
      ) -> bool
    {
      return lhs.os_raw == rhs.os_raw
        && lhs.cpu_raw == rhs.cpu_raw
        && lhs.compiler_raw == rhs.compiler_raw
        ;
    }

    auto preferred_platforms
      ( std::vector<Platform> const& platforms
      ) -> std::vector<Platform>
    {
      // Keep one kernel snapshot per OS/CPU/compiler tuple so reruns
      // on newer kernels replace older snapshots in the plots and
      // paper tables.
      auto selected {std::vector<Platform>{}};
      selected.reserve (platforms.size());

      std::ranges::copy_if
        ( platforms
        , std::back_inserter (selected)
        , [&] (auto const& candidate)
          {
            return std::ranges::none_of
              ( platforms
              , [&] (auto const& other)
                {
                  return other.id != candidate.id
                    && same_platform_variant (other, candidate)
                    && compare_kernel_labels
                       ( other.kernel_label
                       , candidate.kernel_label
                       ) == std::strong_ordering::greater
                    ;
                }
              );
          }
        );

      return selected;
    }
  }

  auto structured_scenario_infos() noexcept -> std::span<ScenarioInfo const>
  {
    return scenario_info_storage();
  }

  auto structured_scenarios() -> std::vector<std::string>
  {
    auto names {std::vector<std::string>{}};
    auto const infos {structured_scenario_infos()};
    names.reserve (infos.size());

    std::ranges::transform
      ( infos
      , std::back_inserter (names)
      , [] (auto const& info)
        {
          return std::string {info.name};
        }
      );

    return names;
  }

  auto structured_scenario_label (std::string_view scenario) -> std::string
  {
    auto const infos {structured_scenario_infos()};
    auto const info
      { std::ranges::find_if
        ( infos
        , [&] (auto const& i)
          {
            return i.name == scenario;
          }
        )
      };

    return info == std::cend (infos)
      ? std::string {scenario}
      : std::string {info->label}
    ;
  }

  auto grid_order() noexcept -> std::span<std::string_view const>
  {
    return grid_storage();
  }

  auto density_order() noexcept -> std::span<int const>
  {
    return density_storage();
  }

  auto mean (std::vector<double> const& values) -> double
  {
    if (values.empty())
    {
      return 0.0;
    }

    auto const sum {std::ranges::fold_left (values, 0.0, std::plus{})};

    return sum / static_cast<double> (values.size());
  }

  auto geomean (std::vector<double> const& values) -> double
  {
    if (values.empty())
    {
      return 0.0;
    }

    if (!std::ranges::all_of (values, [] (auto value) { return value > 0.0; }))
    {
      return 0.0;
    }

    auto const log_sum
      { std::ranges::fold_left
        ( values
        , 0.0
        , [] (auto log_sum, auto v) { return log_sum + std::log (v); }
        )
      };

    return std::exp (log_sum / static_cast<double> (values.size()));
  }

  auto percentile (double p, std::vector<double> values) -> double
  {
    if (values.empty())
    {
      return 0.0;
    }

    std::ranges::sort (values);

    auto const rank {p * static_cast<double> (values.size() - 1)};
    auto const lo {static_cast<std::size_t> (rank)};
    auto const hi {std::min (lo + 1, values.size() - 1)};
    auto const frac {rank - static_cast<double> (lo)};

    return values[lo] * (1.0 - frac) + values[hi] * frac;
  }

  auto label_os (std::string const& os_raw) -> std::string
  {
    auto const os {std::string_view {os_raw}};
    if (os.starts_with ("rocky-"))
    {
      return std::format ("Rocky {}", os.substr (6));
    }

    if (os.starts_with ("rhel-"))
    {
      return std::format ("RHEL {}", os.substr (5));
    }

    return os_raw;
  }

  auto label_cpu (std::string const& cpu_raw) -> std::string
  {
    static auto const labels
      { std::to_array<std::pair<std::string_view, std::string_view>>
        ( { {"amd-epyc-7543-32-core-processor", "AMD EPYC 7543"}
          , {"amd-epyc-9454-48-core-processor", "AMD EPYC 9454"}
          , {"intel-r-xeon-r-gold-6240r-cpu-2.40ghz", "Intel Xeon Gold 6240R"}
          , {"intel-r-xeon-r-gold-6348-cpu-2.60ghz", "Intel Xeon Gold 6348"}
          , {"intel-r-xeon-r-gold-6442y", "Intel Xeon Gold 6442Y"}
          , {"13th-gen-intel-r-core-tm-i7-1370p", "Intel Core i7-1370P"}
          }
        )
      };

    auto const label
      { std::ranges::find_if
        ( labels
        , [&] (auto const& entry) { return entry.first == cpu_raw; }
        )
    };

    return label == std::cend (labels) ? cpu_raw : std::string {label->second};
  }

  auto parse_platform (std::filesystem::path const& dir) -> Platform
  {
    static auto const re
      { std::regex {"^os_(.*?)__kernel_(.*?)__cpu_(.*?)__compiler_(.*?)/?$"}
      };
    static auto const version_re {std::regex {"([0-9]+\\.[0-9]+\\.[0-9]+)"}};

    auto const id {dir.filename().string()};

    auto match {std::smatch{}};
    if (!std::regex_match (id, match, re))
    {
      throw std::runtime_error
        { std::format ("unrecognised result directory: {}", dir.string())
        };
    }

    auto const os_raw {match[1].str()};
    auto const os_label {label_os (os_raw)};
    auto const kernel_label {match[2].str()};
    auto const cpu_raw {match[3].str()};
    auto const cpu_label {label_cpu (cpu_raw)};
    auto const compiler_raw {match[4].str()};
    auto const compiler_family
      { std::string_view {compiler_raw}.starts_with ("clang-")
        ? std::string {"Clang"}
        : std::string {"GCC"}
      };
    auto version_match {std::smatch{}};
    auto const compiler_version
      { std::regex_search (compiler_raw, version_match, version_re)
        ? version_match[1].str()
        : compiler_raw
      };
    auto const compiler_label {std::format ("{} {}", compiler_family, compiler_version)};
    auto const platform_label
      { std::format ("{} / {} / {}", cpu_label, os_label, compiler_label)
      };

    return Platform
      { .id = id
      , .result_dir = dir
      , .os_raw = os_raw
      , .os_label = os_label
      , .kernel_label = kernel_label
      , .cpu_raw = cpu_raw
      , .cpu_label = cpu_label
      , .compiler_raw = compiler_raw
      , .compiler_family = compiler_family
      , .compiler_version = compiler_version
      , .compiler_label = compiler_label
      , .platform_label = platform_label
      };
  }

  auto load_platforms() -> std::vector<Platform>
  {
    auto dirs {std::vector<std::filesystem::path>{}};
    std::ranges::for_each
      ( std::filesystem::directory_iterator {"results"}
      , [&] (auto const& entry)
        {
          if (entry.is_directory())
          {
            dirs.push_back (entry.path());
          }
        }
      );
    std::ranges::sort (dirs);

    auto platforms {std::vector<Platform>{}};
    platforms.reserve (dirs.size());
    std::ranges::transform
      ( dirs
      , std::back_inserter (platforms)
      , [] (auto const& dir) { return parse_platform (dir); }
      );

    return preferred_platforms (platforms);
  }

  auto reference_platform_id
    ( std::vector<Platform> const& platforms
    ) -> std::string
  {
    auto const find_preferred
      { [&] ( std::string_view cpu
            , std::string_view compiler
            , std::string_view os
            ) -> Platform const*
        {
          auto const candidate
            { std::ranges::find_if
              ( platforms
              , [&] (auto const& platform)
                {
                  return platform.cpu_label == cpu
                      && platform.compiler_family == compiler
                      && platform.os_label == os
                      ;
                }
              )
            };

          return candidate == std::cend (platforms)
            ? nullptr
            : std::addressof (*candidate)
            ;
        }
      };

    if ( auto const* preferred
           { find_preferred ("AMD EPYC 9454", "GCC", "Rocky 10.1")
           }
       )
    {
      return preferred->id;
    }

    return platforms.empty() ? std::string {} : platforms.front().id;
  }

  auto parse_result_file
    ( std::filesystem::path const& file
    ) -> std::optional<FileMetadata>
  {
    auto constexpr cache_independent_prefix
      { std::string_view {"cache-independent-"}
      };
    auto const base {file.filename().string()};
    auto const name {std::string_view {base}};

    if (name.ends_with (".txt"))
    {
      auto const config {name.substr (0, name.size() - 4)};
      if (is_cache_config (config))
      {
        return FileMetadata
          { "cache-dependent"
          , std::string {config}
          , std::string {reference_cache_config()}
          };
      }
    }

    if (name.starts_with (cache_independent_prefix) && name.ends_with (".txt"))
    {
      auto const config
        { name.substr ( cache_independent_prefix.size()
                      , name.size() - cache_independent_prefix.size() - 4
                      )
        };
      if (is_cache_config (config))
      {
        return FileMetadata
          { "cache-independent"
          , std::string {config}
          , std::string {config}
          };
      }
    }

    if (name.ends_with ("-stress.txt"))
    {
      auto const config {name.substr (0, name.size() - 11)};
      if (is_cache_config (config))
      {
        return FileMetadata
          { "stress"
          , std::string {config}
          , std::string {reference_cache_config()}
          };
      }
    }

    auto constexpr storage_prefix {std::string_view {"storage-"}};
    if (name.starts_with (storage_prefix) && name.ends_with (".txt"))
    {
      auto const config
        { name.substr ( storage_prefix.size()
                      , name.size() - storage_prefix.size() - 4
                      )
        };
      if (is_cache_config (config))
      {
        return FileMetadata
          { "storage"
          , std::string {config}
          , std::string {config}
          };
      }
    }

    auto constexpr pos_mix_prefix {std::string_view {"pos-mix-"}};
    if (name.starts_with (pos_mix_prefix) && name.ends_with (".txt"))
    {
      auto const config
        { name.substr ( pos_mix_prefix.size()
                      , name.size() - pos_mix_prefix.size() - 4
                      )
        };
      if (is_cache_config (config))
      {
        return FileMetadata
          { "pos-mix"
          , std::string {config}
          , std::string {config}
          };
      }
    }

    return std::nullopt;
  }

  MappedFile::OpenFD::~OpenFD() noexcept
  {
    ::close (fd);
  }

  MappedFile::MappedFile (std::filesystem::path const& path)
    : _fd
      { std::invoke
        ( [&]() -> OpenFD
          {
            auto const fd {::open (path.c_str(), O_RDONLY)};
            if (fd < 0)
            {
              throw std::runtime_error
                { std::format ("open {}: {}", path.string(), std::strerror (errno))
                };
            }
            return OpenFD {fd};
          }
        )
      }
    , _size
      { std::invoke
        ( [&]() -> std::size_t
          {
            struct stat st {};
            if (::fstat (_fd.fd, std::addressof (st)) != 0)
            {
              auto const err {errno};
              throw std::runtime_error
                { std::format ("fstat {}: {}", path.string(), std::strerror (err))
                };
            }
            if (std::cmp_equal (st.st_size, 0))
            {
              throw std::runtime_error
                { std::format ("fstat {} has size zero", path.string())
                };
            }
            return st.st_size;
          }
        )
      }
    , _data
      { std::invoke
        ( [&]() -> char*
          {
            auto* mapped
              { ::mmap (nullptr, _size, PROT_READ, MAP_PRIVATE, _fd.fd, 0)
              };
            if (mapped == MAP_FAILED)
            {
              auto const err {errno};
              throw std::runtime_error
                { std::format ("mmap {}: {}", path.string(), std::strerror (err))
                };
            }
            ::madvise (mapped, _size, MADV_SEQUENTIAL);
            return static_cast<char*> (mapped);
          }
        )
      }
  {}

  MappedFile::~MappedFile() noexcept
  {
    ::munmap (_data, _size);
  }

  auto MappedFile::data() const noexcept -> std::string_view
  {
    return std::string_view {_data, _size};
  }

  namespace
  {
    auto to_double (std::string_view value) -> double
    {
      if (value.empty())
      {
        throw std::logic_error {"to_double from empty view"};
      }

      auto buffer {std::array<char, 64>{}};
      auto const n {std::min (value.size(), buffer.size() - 1)};
      std::memcpy (buffer.data(), value.data(), n);
      buffer[n] = '\0';

      auto* end {static_cast<char*> (nullptr)};
      auto const parsed {std::strtod (buffer.data(), std::addressof (end))};
      return end == buffer.data() ? 0.0 : parsed;
    }
  }

  Row::Row (std::string_view line)
  {
    auto pos {std::size_t {0}};
    while (pos < line.size())
    {
      while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t'))
      {
        ++pos;
      }

      auto const start {pos};
      while (pos < line.size() && line[pos] != ' ' && line[pos] != '\t')
      {
        ++pos;
      }

      if (start == pos)
      {
        break;
      }

      auto const token {line.substr (start, pos - start)};
      auto const eq {token.find ('=')};
      if (eq == std::string_view::npos)
      {
        continue;
      }

      _map.emplace (token.substr (0, eq), token.substr (eq + 1));
    }
  }

  auto Row::value (std::string_view key) const noexcept -> std::string_view
  {
    auto const match {_map.find (key)};
    return match == std::cend (_map) ? std::string_view {} : match->second;
  }

  auto Row::measured_value() const -> double
  {
    auto const metric {value ("metric")};
    if (metric == "construct")
    {
      return to_double (value ("ns_per_point"));
    }

    if (metric == "pos_random" || metric == "pos_all" || metric == "pos_mix")
    {
      return to_double (value ("ns_per_pos"));
    }

    if (metric == "at_random" || metric == "at_all")
    {
      return to_double (value ("ns_per_at"));
    }

    return to_double (value ("value"));
  }

  auto is_multiple_survey (std::string_view scenario) -> bool
  {
    return scenario.starts_with ("multiple-survey");
  }

  auto parse_random_density (std::string_view scenario) -> std::optional<int>
  {
    if (!scenario.starts_with ("random-") || !scenario.ends_with ("pct"))
    {
      return std::nullopt;
    }

    auto const digits {scenario.substr (7, scenario.size() - 10)};
    auto density {0};
    auto const result
      { std::from_chars ( digits.data()
                        , digits.data() + digits.size()
                        , density
                        )
      };

    if ( result.ec != std::errc {}
       || result.ptr != digits.data() + digits.size()
       )
    {
      return std::nullopt;
    }

    return density;
  }
}
