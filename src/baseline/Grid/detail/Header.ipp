#include <cstring>
#include <functional>
#include <memory>
#include <utility>

namespace ipt::baseline::grid
{
  template<std::size_t D, typename G>
    constexpr auto make_reserved_layout
      ( G const& grid
      ) -> ReservedLayout<D>
  {
    return std::invoke
      ( [&]<std::size_t... Ds>
          ( std::index_sequence<Ds...>
          ) -> ReservedLayout<D>
        {
          return ReservedLayout<D>
            { .origin  = {grid.origin()[Ds]...}
            , .extents
                = {static_cast<ipt::Coordinate> (grid.extents()[Ds])...}
            , .strides = {grid.strides()[Ds]...}
            };
        }
      , std::make_index_sequence<D>{}
      );
  }

  template<std::size_t D>
    auto read_layout
      ( std::filesystem::path const& path
      ) -> ReservedLayout<D>
  {
    auto file {ipt::baseline::storage::MappedFile {path}};
    ipt::baseline::storage::validate_kind_and_dimension
      (file, ipt::baseline::storage::Kind::Grid, D);

    auto layout {ReservedLayout<D>{}};
    std::memcpy
      ( std::addressof (layout)
      , file.header().reserved.data()
      , sizeof (layout)
      );
    return layout;
  }
}
