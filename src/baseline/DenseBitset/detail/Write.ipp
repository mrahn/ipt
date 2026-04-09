#include <array>
#include <cstdint>
#include <span>

namespace ipt::baseline::dense_bitset
{
  template<std::size_t D, Storage<D> S>
    auto write
      ( std::filesystem::path const& path
      , S const& storage
      , std::string_view description
      ) -> void
  {
    auto const bits {storage.bits()};

    auto const header
      { ipt::baseline::storage::make_header
          ( { .kind = ipt::baseline::storage::Kind::DenseBitset
            , .dimension     = static_cast<std::uint32_t> (D)
            , .payload_size  = bits.size_bytes()
            , .element_count
                = static_cast<std::uint64_t> (storage.point_count())
            , .section_bytes = {bits.size_bytes()}
            , .description   = description
            , .reserved
                = ipt::baseline::storage::make_reserved (storage.layout())
            }
          )
      };

    auto const section
      { std::span<std::byte const>
          { reinterpret_cast<std::byte const*> (bits.data())
          , bits.size_bytes()
          }
      };
    auto const sections
      { std::array<std::span<std::byte const>, 1> {section}
      };
    ipt::baseline::storage::write_with_sections (path, header, sections);
  }
}
