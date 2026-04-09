#include <array>
#include <cstdint>
#include <span>

namespace ipt::baseline::sorted_points
{
  template<std::size_t D, Storage<D> S>
    auto write
      ( std::filesystem::path const& path
      , S const& storage
      , std::string_view description
      ) -> void
  {
    auto const points {storage.points()};
    auto const header
      { ipt::baseline::storage::make_header
          ( { .kind = ipt::baseline::storage::Kind::SortedPoints
            , .dimension     = static_cast<std::uint32_t> (D)
            , .payload_size  = points.size_bytes()
            , .element_count = static_cast<std::uint64_t> (points.size())
            , .section_bytes = {points.size_bytes()}
            , .description   = description
            }
          )
      };

    auto const section
      { std::span<std::byte const>
          { reinterpret_cast<std::byte const*> (points.data())
          , points.size_bytes()
          }
      };
    auto const sections
      { std::array<std::span<std::byte const>, 1> {section}
      };
    ipt::baseline::storage::write_with_sections (path, header, sections);
  }
}
