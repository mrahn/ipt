#include <cstring>
#include <format>
#include <fstream>
#include <ios>
#include <memory>
#include <stdexcept>
#include <type_traits>

namespace ipt::storage
{
  template<std::size_t D, Storage<D> Storage>
    auto write
      ( std::filesystem::path const& path
      , IPT<D, Storage> const& ipt
      , std::string_view description
      ) -> void
  {
    static_assert
      ( std::is_trivially_copyable_v<Entry<D>>
      , "Entry<D> must be trivially copyable to be written verbatim"
      );

    if (! (description.size() <= description_max))
    {
      throw std::length_error
        { std::format
            ( "ipt::storage::write: description does not fit ({} > {})"
            , description.size()
            , description_max
            )
        };
    }

    auto const entries {ipt.entries_view()};

    auto header
      { Header
        { .magic = magic_value
        , .version = format_version
        , .dimension = static_cast<std::uint32_t> (D)
        , .entry_size = static_cast<std::uint32_t> (sizeof (Entry<D>))
        , ._pad0 = 0u
        , .entry_count = static_cast<std::uint64_t> (entries.size())
        , .cache_cuboid_size = cache_cuboid_size_value
        , .cache_ruler_size = cache_ruler_size_value
        , .cache_entry_end = cache_entry_end_value
        , .cache_entry_lub = cache_entry_lub_value
        , .reserved = {}
        , .description = {}
        }
      };

    if (!description.empty())
    {
      std::memcpy
        (header.description.data(), description.data(), description.size());
    }

    auto out
      { std::ofstream
          {path, std::ios::binary | std::ios::trunc | std::ios::out}
      };

    if (!out)
    {
      throw std::runtime_error
        { std::format ("ipt::storage::write: cannot open '{}'", path.string())
        };
    }

    out.exceptions (std::ios::failbit | std::ios::badbit);

    out.write
      ( reinterpret_cast<char const*> (std::addressof (header))
      , static_cast<std::streamsize> (sizeof (Header))
      );

    if (!entries.empty())
    {
      out.write
        ( reinterpret_cast<char const*> (entries.data())
        , static_cast<std::streamsize> (entries.size_bytes())
        );
    }
  }
}
