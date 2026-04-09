#include <cstring>
#include <format>
#include <memory>
#include <stdexcept>

namespace ipt::baseline::roaring::storage
{
  template<std::size_t D>
    MMap<D>::MMap (std::filesystem::path const& path)
      : _file {path}
  {
    ipt::baseline::storage::validate_kind_and_dimension
      (_file, ipt::baseline::storage::Kind::Roaring, D);

    auto const& header {_file.header()};
    std::memcpy
      (std::addressof (_layout), header.reserved.data(), sizeof (_layout));

    auto const buf_bytes {header.section_bytes[0]};

    if (header.payload_size < buf_bytes)
    {
      throw std::runtime_error
        { std::format
            ( "ipt::baseline::roaring::MMap: '{}': payload truncated"
            , path.string()
            )
        };
    }

    auto const* const data
      { reinterpret_cast<char const*> (_file.payload().data())
      };

    _bitmap = ::roaring::Roaring64Map::readSafe
      (data, static_cast<std::size_t> (buf_bytes));
    _point_count = static_cast<ipt::Index> (header.element_count);
  }

  template<std::size_t D>
    auto MMap<D>::bitmap
      (
      ) const noexcept -> ::roaring::Roaring64Map const&
  {
    return _bitmap;
  }

  template<std::size_t D>
    auto MMap<D>::point_count() const noexcept -> ipt::Index
  {
    return _point_count;
  }

  template<std::size_t D>
    auto MMap<D>::layout
      (
      ) const noexcept -> ipt::baseline::grid::ReservedLayout<D> const&
  {
    return _layout;
  }

  template<std::size_t D>
    auto MMap<D>::description
      (
      ) const noexcept -> std::string_view
  {
    return _file.description();
  }
}
