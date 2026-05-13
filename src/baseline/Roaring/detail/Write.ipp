#include <array>
#include <cstdint>
#include <format>
#include <span>
#include <stdexcept>
#include <vector>

namespace ipt::baseline::roaring
{
  template<std::size_t D, Storage<D> S>
    auto write
      ( std::filesystem::path const& path
      , S const& storage
      , std::string_view description
      ) -> void
  {
    auto const& bm {storage.bitmap()};
    auto const buf_size {bm.getSizeInBytes (true)};

    auto buffer {std::vector<char> (buf_size)};
    auto const written {bm.write (buffer.data(), true)};

    if (written != buf_size)
    {
      throw std::runtime_error
        { std::format
            ( "ipt::baseline::roaring::write: '{}': size mismatch ({} != {})"
            , path.string(), written, buf_size
            )
        };
    }

    auto const header
      { ipt::baseline::storage::make_header
          ( { .kind = ipt::baseline::storage::Kind::Roaring
            , .dimension     = static_cast<std::uint32_t> (D)
            , .payload_size  = buf_size
            , .element_count
                = static_cast<std::uint64_t> (storage.point_count())
            , .section_bytes = {buf_size}
            , .description   = description
            , .reserved
                = ipt::baseline::storage::make_reserved (storage.layout())
            }
          )
      };

    auto const section
      { std::span<std::byte const>
          { reinterpret_cast<std::byte const*> (buffer.data())
          , buf_size
          }
      };
    auto const sections
      { std::array<std::span<std::byte const>, 1> {section}
      };
    ipt::baseline::storage::write_with_sections (path, header, sections);
  }
}
