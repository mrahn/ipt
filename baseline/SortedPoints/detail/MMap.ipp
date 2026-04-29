#include <cstring>
#include <format>
#include <stdexcept>

namespace ipt::baseline::sorted_points::storage
{
  template<std::size_t D>
    MMap<D>::MMap (std::filesystem::path const& path)
      : _file {path}
  {
    ipt::baseline::storage::validate_kind_and_dimension
      (_file, ipt::baseline::storage::Kind::SortedPoints, D);

    auto const& header {_file.header()};
    auto const expected_bytes
      { static_cast<std::size_t> (header.element_count)
        * sizeof (ipt::Point<D>)
      };

    if (header.section_bytes[0] != expected_bytes)
    {
      throw std::runtime_error
        { std::format
            ( "ipt::baseline::sorted_points::MMap: '{}': section[0] size"
              " mismatch (file={}, expected={})"
            , path.string()
            , header.section_bytes[0], expected_bytes
            )
        };
    }

    if (header.payload_size < expected_bytes)
    {
      throw std::runtime_error
        { std::format
            ( "ipt::baseline::sorted_points::MMap: '{}': payload truncated"
            , path.string()
            )
        };
    }

    auto const* const data
      { reinterpret_cast<ipt::Point<D> const*> (_file.payload().data())
      };
    _points = std::span<ipt::Point<D> const>
      {data, static_cast<std::size_t> (header.element_count)};
  }

  template<std::size_t D>
    auto MMap<D>::points
      (
      ) const noexcept -> std::span<ipt::Point<D> const>
  {
    return _points;
  }

  template<std::size_t D>
    auto MMap<D>::description
      (
      ) const noexcept -> std::string_view
  {
    return _file.description();
  }
}
