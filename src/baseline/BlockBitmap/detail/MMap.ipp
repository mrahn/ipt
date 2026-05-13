#include <cstring>
#include <format>
#include <memory>
#include <stdexcept>

namespace ipt::baseline::block_bitmap::storage
{
  template<std::size_t D>
    MMap<D>::MMap (std::filesystem::path const& path)
      : _file {path}
  {
    ipt::baseline::storage::validate_kind_and_dimension
      (_file, ipt::baseline::storage::Kind::BlockBitmap, D);

    auto const& header {_file.header()};
    std::memcpy
      (std::addressof (_layout), header.reserved.data(), sizeof (_layout));

    auto const tile_bytes {header.section_bytes[0]};

    if (tile_bytes % sizeof (Word) != 0)
    {
      throw std::runtime_error
        { std::format
            ( "ipt::baseline::block_bitmap::MMap: '{}': section[0] size {}"
              " not a multiple of sizeof(Word)={}"
            , path.string(), tile_bytes, sizeof (Word)
            )
        };
    }

    if (header.payload_size < tile_bytes)
    {
      throw std::runtime_error
        { std::format
            ( "ipt::baseline::block_bitmap::MMap: '{}': payload truncated"
            , path.string()
            )
        };
    }

    auto const* const data
      { reinterpret_cast<Word const*> (_file.payload().data())
      };
    _tiles = std::span<Word const>
      {data, static_cast<std::size_t> (tile_bytes / sizeof (Word))};
    _point_count = static_cast<ipt::Index> (header.element_count);
  }

  template<std::size_t D>
    auto MMap<D>::tiles() const noexcept -> std::span<Word const>
  {
    return _tiles;
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
