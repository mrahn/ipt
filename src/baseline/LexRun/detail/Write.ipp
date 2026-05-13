#include <array>
#include <cstdint>
#include <span>

namespace ipt::baseline::lex_run
{
  template<std::size_t D, Storage<D> S>
    auto write
      ( std::filesystem::path const& path
      , S const& storage
      , std::string_view description
      ) -> void
  {
    auto const runs {storage.runs()};

    auto const header
      { ipt::baseline::storage::make_header
          ( { .kind = ipt::baseline::storage::Kind::LexRun
            , .dimension     = static_cast<std::uint32_t> (D)
            , .payload_size  = runs.size_bytes()
            , .element_count
                = static_cast<std::uint64_t> (storage.total_points())
            , .section_bytes = {runs.size_bytes()}
            , .description   = description
            }
          )
      };

    auto const section
      { std::span<std::byte const>
          { reinterpret_cast<std::byte const*> (runs.data())
          , runs.size_bytes()
          }
      };
    auto const sections
      { std::array<std::span<std::byte const>, 1> {section}
      };
    ipt::baseline::storage::write_with_sections (path, header, sections);
  }
}
