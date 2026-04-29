#include <array>
#include <cstdint>
#include <span>

namespace ipt::baseline::row_csr
{
  template<std::size_t D, std::size_t K, Storage<D, K> S>
    auto write
      ( std::filesystem::path const& path
      , S const& storage
      , std::string_view description
      ) -> void
  {
    auto const keys {storage.row_keys()};
    auto const offsets {storage.row_offsets()};
    auto const values {storage.column_values()};

    auto const header
      { ipt::baseline::storage::make_header
          ( { .kind = ipt::baseline::storage::Kind::RowCSR
            , .dimension     = static_cast<std::uint32_t> (D)
            , .payload_size
                = keys.size_bytes() + offsets.size_bytes()
                + values.size_bytes()
            , .element_count = static_cast<std::uint64_t> (values.size())
            , .section_bytes
                = { keys.size_bytes()
                  , offsets.size_bytes()
                  , values.size_bytes()
                  }
            , .description   = description
            , .sub_dimension = static_cast<std::uint32_t> (K)
            }
          )
      };

    auto const sec_keys
      { std::span<std::byte const>
          { reinterpret_cast<std::byte const*> (keys.data())
          , keys.size_bytes()
          }
      };
    auto const sec_offsets
      { std::span<std::byte const>
          { reinterpret_cast<std::byte const*> (offsets.data())
          , offsets.size_bytes()
          }
      };
    auto const sec_values
      { std::span<std::byte const>
          { reinterpret_cast<std::byte const*> (values.data())
          , values.size_bytes()
          }
      };
    auto const sections
      { std::array<std::span<std::byte const>, 3>
          {sec_keys, sec_offsets, sec_values}
      };
    ipt::baseline::storage::write_with_sections (path, header, sections);
  }
}
