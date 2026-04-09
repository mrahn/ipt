#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <ipt/Coordinate.hpp>
#include <ipt/Ruler.hpp>
#include <iterator>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <utility>

namespace ipt::baseline
{
  template<std::size_t D>
    constexpr Grid<D>::Grid
      ( Origin<D> origin
      , Extents<D> extents
      , Strides<D> strides
      )
        : _origin {origin}
        , _extents {extents}
        , _strides {strides}
  {}

  template<std::size_t D>
    constexpr auto Grid<D>::name() noexcept
  {
    return "grid";
  }

  template<std::size_t D>
    auto Grid<D>::add (Point<D> const&) -> void
  {
    throw std::logic_error
      {"Grid::add: Grid must be constructed directly"};
  }

  template<std::size_t D>
    auto Grid<D>::build() && noexcept -> Grid
  {
    return std::move (*this);
  }

  template<std::size_t D>
    constexpr auto Grid<D>::origin() const noexcept -> Origin<D>
  {
    return _origin;
  }

  template<std::size_t D>
    constexpr auto Grid<D>::extents() const noexcept -> Extents<D>
  {
    return _extents;
  }

  template<std::size_t D>
    constexpr auto Grid<D>::strides() const noexcept -> Strides<D>
  {
    return _strides;
  }

  template<std::size_t D>
    constexpr auto Grid<D>::size() const noexcept -> Index
  {
    return static_cast<Index> (_extents.size());
  }

  template<std::size_t D>
    constexpr auto Grid<D>::bytes() const noexcept -> std::size_t
  {
    return sizeof (Grid);
  }

  template<std::size_t D>
    auto Grid<D>::at (Index index) const -> Point<D>
  {
    if (!(index < size()))
    {
      throw std::out_of_range {"Grid::at: index out of range"};
    }

    auto coordinates {std::array<Coordinate, D>{}};

    std::ranges::transform
      ( _extents | std::views::reverse
      , std::rbegin (coordinates)
      , [&, remaining = index, d = D] (auto extent) mutable
        {
          auto const current_d {--d};
          auto const i_extent {static_cast<Index> (extent)};
          auto const coordinate_index
            {static_cast<Coordinate> (remaining % i_extent)};

          remaining /= i_extent;

          return _origin[current_d]
            + coordinate_index * _strides[current_d]
            ;
        }
      );

    return Point<D> {coordinates};
  }

  template<std::size_t D>
    auto Grid<D>::pos (Point<D> point) const -> Index
  {
    std::ranges::for_each
      ( _extents
      , [&, d {std::size_t {0}}] (auto extent) mutable
        {
          auto const coordinate {point[d]};
          auto const origin {_origin[d]};
          auto const stride {_strides[d++]};

          if (coordinate < origin)
          {
            throw std::out_of_range {"Grid::pos: point out of range"};
          }

          if ((coordinate - origin) % stride != 0)
          {
            throw std::out_of_range {"Grid::pos: point out of range"};
          }

          auto const offset {(coordinate - origin) / stride};

          if (offset < 0 || offset >= extent)
          {
            throw std::out_of_range {"Grid::pos: point out of range"};
          }
        }
      );

    return UNCHECKED_pos (point);
  }

  template<std::size_t D>
    auto Grid<D>::try_pos
      ( Point<D> point
      ) const noexcept -> std::optional<Index>
  {
    auto in_range {true};

    std::ranges::for_each
      ( _extents
      , [&, d {std::size_t {0}}] (auto extent) mutable noexcept
        {
          if (!in_range)
          {
            ++d;
            return;
          }

          auto const coordinate {point[d]};
          auto const origin {_origin[d]};
          auto const stride {_strides[d++]};

          if (coordinate < origin)
          {
            in_range = false;
            return;
          }

          if ((coordinate - origin) % stride != 0)
          {
            in_range = false;
            return;
          }

          auto const offset {(coordinate - origin) / stride};

          if (offset < 0 || offset >= extent)
          {
            in_range = false;
          }
        }
      );

    if (!in_range)
    {
      return std::nullopt;
    }

    return UNCHECKED_pos (point);
  }

  template<std::size_t D>
    auto Grid<D>::UNCHECKED_pos
      ( Point<D> point
      ) const noexcept -> Index
  {
    return std::ranges::fold_left
      ( _extents
      , Index {0}
      , [&, d {std::size_t {0}}]
          ( Index index
          , Coordinate extent
          ) mutable noexcept
        {
          auto const coordinate {point[d]};
          auto const origin {_origin[d]};
          auto const stride {_strides[d++]};
          auto const offset {(coordinate - origin) / stride};

          return static_cast<Index> (offset)
            + index * static_cast<Index> (extent)
            ;
        }
      );
  }

  template<std::size_t D>
    auto to_ipt_cuboid
      ( Grid<D> const& grid
      ) -> Cuboid<D>
  {
    auto rulers
      { std::invoke
        ( [&]<std::size_t... Dimensions>
            ( std::index_sequence<Dimensions...>
            )
          {
            return std::array<Ruler, D>
              { Ruler
                { grid.origin()[Dimensions]
                , grid.strides()[Dimensions]
                , grid.origin()[Dimensions]
                  + static_cast<Coordinate> (grid.extents()[Dimensions])
                  * grid.strides()[Dimensions]
                }...
              };
          }
        , std::make_index_sequence<D>{}
        )
      };

    return Cuboid<D> {std::move (rulers)};
  }

  template<std::size_t D>
    auto grid_select_impl
      ( Cuboid<D> grid_cuboid
      , Cuboid<D> query
      ) -> std::generator<Cuboid<D>>
  {
    if (auto inter {grid_cuboid.intersect (query)})
    {
      co_yield std::move (*inter);
    }
  }

  template<std::size_t D>
    auto Grid<D>::select
      ( Cuboid<D> query
      ) const -> std::generator<Cuboid<D>>
  {
    return grid_select_impl<D>
      (to_ipt_cuboid (*this), std::move (query));
  }
}
