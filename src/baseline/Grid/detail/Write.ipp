#include <array>
#include <cstdint>
#include <span>

namespace ipt::baseline::grid
{
  template<std::size_t D, typename G>
    auto write
      ( std::filesystem::path const& path
      , G const& grid
      , std::string_view description
      ) -> void
  {
    static_assert
      ( sizeof (ReservedLayout<D>)
        <= ipt::baseline::storage::header_reserved_size
      , "Grid reserved layout does not fit in header reserved area"
      );

    auto const header
      { ipt::baseline::storage::make_header
          ( { .kind          = ipt::baseline::storage::Kind::Grid
            , .dimension     = static_cast<std::uint32_t> (D)
            , .payload_size  = 0u
            , .element_count = 0u
            , .section_bytes = {}
            , .description   = description
            , .reserved
                = ipt::baseline::storage::make_reserved
                    (make_reserved_layout<D> (grid))
            }
          )
      };

    auto const sections
      { std::array<std::span<std::byte const>, 0> {}
      };
    ipt::baseline::storage::write_with_sections (path, header, sections);
  }
}
