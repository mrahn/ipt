#include <cstdint>
#include <format>
#include <stdexcept>

namespace ipt::baseline::row_csr::storage
{
  template<std::size_t D, std::size_t K>
    MMap<D, K>::MMap (std::filesystem::path const& path)
      : _file {path}
  {
    ipt::baseline::storage::validate_kind_and_dimension
      ( _file, ipt::baseline::storage::Kind::RowCSR
      , static_cast<std::uint32_t> (D)
      , static_cast<std::uint32_t> (K)
      );

    auto const& header {_file.header()};
    auto const key_bytes    {header.section_bytes[0]};
    auto const offset_bytes {header.section_bytes[1]};
    auto const value_bytes  {header.section_bytes[2]};

    if (key_bytes % sizeof (RowKey<D, K>) != 0
     || offset_bytes % sizeof (ipt::Index) != 0
     || value_bytes % sizeof (ColumnValue<D, K>) != 0
       )
    {
      throw std::runtime_error
        { std::format
            ( "ipt::baseline::row_csr::MMap: '{}': section sizes not aligned"
              " ({}/{}/{}, expected multiples of {}/{}/{})"
            , path.string()
            , key_bytes, offset_bytes, value_bytes
            , sizeof (RowKey<D, K>), sizeof (ipt::Index)
            , sizeof (ColumnValue<D, K>)
            )
        };
    }

    if (header.payload_size < key_bytes + offset_bytes + value_bytes)
    {
      throw std::runtime_error
        { std::format
            ( "ipt::baseline::row_csr::MMap: '{}': payload truncated"
            , path.string()
            )
        };
    }

    auto const* base {_file.payload().data()};
    auto const* keys_ptr
      {reinterpret_cast<RowKey<D, K> const*> (base)};
    auto const* offsets_ptr
      { reinterpret_cast<ipt::Index const*>
          (base + key_bytes)
      };
    auto const* values_ptr
      { reinterpret_cast<ColumnValue<D, K> const*>
          (base + key_bytes + offset_bytes)
      };

    _row_keys = std::span<RowKey<D, K> const>
      {keys_ptr, key_bytes / sizeof (RowKey<D, K>)};
    _row_offsets = std::span<ipt::Index const>
      {offsets_ptr, offset_bytes / sizeof (ipt::Index)};
    _column_values = std::span<ColumnValue<D, K> const>
      {values_ptr, value_bytes / sizeof (ColumnValue<D, K>)};
  }

  template<std::size_t D, std::size_t K>
    auto MMap<D, K>::row_keys
      (
      ) const noexcept -> std::span<RowKey<D, K> const>
  {
    return _row_keys;
  }

  template<std::size_t D, std::size_t K>
    auto MMap<D, K>::row_offsets
      (
      ) const noexcept -> std::span<ipt::Index const>
  {
    return _row_offsets;
  }

  template<std::size_t D, std::size_t K>
    auto MMap<D, K>::column_values
      (
      ) const noexcept -> std::span<ColumnValue<D, K> const>
  {
    return _column_values;
  }

  template<std::size_t D, std::size_t K>
    auto MMap<D, K>::description() const noexcept -> std::string_view
  {
    return _file.description();
  }
}
