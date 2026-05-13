#include <cstring>
#include <format>
#include <fstream>
#include <ios>
#include <memory>
#include <stdexcept>
#include <type_traits>

namespace ipt::baseline::storage
{
  inline auto make_description
    ( std::string_view description
    ) -> std::array<char, description_capacity>
  {
    if (! (description.size() <= description_max))
    {
      throw std::length_error
        { std::format
            ( "ipt::baseline::storage::make_description: "
              "description does not fit ({} > {})"
            , description.size(), description_max
            )
        };
    }

    auto array {std::array<char, description_capacity>{}};
    std::memcpy (array.data(), description.data(), description.size());
    return array;
  }

  template<typename T>
    auto make_reserved
      ( T const& value
      ) -> std::array<std::byte, header_reserved_size>
  {
    static_assert (std::is_trivially_copyable_v<T>);
    static_assert (sizeof (T) <= header_reserved_size);

    auto array {std::array<std::byte, header_reserved_size>{}};
    std::memcpy (array.data(), std::addressof (value), sizeof (T));
    return array;
  }

  inline auto make_header
    ( HeaderInit init
    ) -> Header
  {
    return Header
      { .magic = magic_value
      , .version = format_version
      , .kind = static_cast<std::uint32_t> (init.kind)
      , .dimension = init.dimension
      , .sub_dimension = init.sub_dimension
      , .payload_size = init.payload_size
      , .element_count = init.element_count
      , .cache_cuboid_size = cache_cuboid_size_value
      , .cache_ruler_size = cache_ruler_size_value
      , .cache_entry_end = cache_entry_end_value
      , .cache_entry_lub = cache_entry_lub_value
      , .section_bytes = init.section_bytes
      , .reserved = init.reserved
      , .description = make_description (init.description)
      };
  }

  inline auto write_with_sections
    ( std::filesystem::path const& path
    , Header const& header
    , std::span<std::span<std::byte const> const> sections
    ) -> void
  {
    auto out
      { std::ofstream
          {path, std::ios::binary | std::ios::trunc | std::ios::out}
      };

    if (!out)
    {
      throw std::runtime_error
        { std::format
            ("ipt::baseline::storage::write: cannot open '{}'", path.string())
        };
    }

    out.exceptions (std::ios::failbit | std::ios::badbit);

    out.write
      ( reinterpret_cast<char const*> (std::addressof (header))
      , static_cast<std::streamsize> (sizeof (Header))
      );

    for (auto const& section : sections)
    {
      if (!section.empty())
      {
        out.write
          ( reinterpret_cast<char const*> (section.data())
          , static_cast<std::streamsize> (section.size())
          );
      }
    }
  }
}
