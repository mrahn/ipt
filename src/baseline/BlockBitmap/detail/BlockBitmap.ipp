#include <algorithm>
#include <array>
#include <baseline/detail/select.hpp>
#include <baseline/enumerate.hpp>
#include <bit>
#include <cassert>
#include <functional>
#include <ipt/Coordinate.hpp>
#include <iterator>
#include <limits>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <utility>

namespace ipt::baseline
{
  template<std::size_t D, block_bitmap::Storage<D> S>
    BlockBitmap<D, S>::BlockBitmap (Grid<D> grid)
      requires std::same_as<S, block_bitmap::storage::Vector<D>>
        : _storage
          { make_layout (grid)
          , static_cast<std::size_t>
              ( std::ranges::fold_left
                  ( compute_tile_counts (grid)
                  , Index {1}, std::multiplies<Index>{}
                  )
              )
          }
        , _grid {grid}
        , _tile_counts {compute_tile_counts (grid)}
  {}

  template<std::size_t D, block_bitmap::Storage<D> S>
    template<typename... Args>
      requires
        ( std::constructible_from<S, Args&&...>
       && !std::same_as<S, block_bitmap::storage::Vector<D>>
        )
    BlockBitmap<D, S>::BlockBitmap (std::in_place_t, Args&&... args)
      : _storage {std::forward<Args> (args)...}
      , _grid {make_grid (_storage.layout())}
      , _tile_counts {compute_tile_counts (_grid)}
  {}

  template<std::size_t D, block_bitmap::Storage<D> S>
    constexpr auto BlockBitmap<D, S>::name() noexcept
  {
    return "block-bitmap";
  }

  template<std::size_t D, block_bitmap::Storage<D> S>
    auto BlockBitmap<D, S>::add (Point<D> const& point) noexcept -> void
      requires std::same_as<S, block_bitmap::storage::Vector<D>>
  {
    assert (_storage.point_count() < _grid.size());
    assert (!_last_added || *_last_added < point);

    auto tile_coords {std::array<Index, D>{}};
    auto local_coords {std::array<Index, D>{}};

    std::ranges::for_each
      ( std::views::iota (std::size_t {0}, D)
      , [&] (auto d) noexcept
        {
          auto const coord {point[d]};
          auto const origin {_grid.origin()[d]};
          auto const stride {_grid.strides()[d]};

          assert (coord >= origin);
          assert ((coord - origin) % stride == 0);

          auto const grid_coordinate
            { static_cast<Index> ((coord - origin) / stride)
            };

          assert (grid_coordinate
                  < static_cast<Index> (_grid.extents()[d]));

          tile_coords[d] = grid_coordinate / tile_extent;
          local_coords[d] = grid_coordinate % tile_extent;
        }
      );

    auto const tile_index
      { std::ranges::fold_left
        ( std::views::iota (std::size_t {0}, D)
        , Index {0}
        , [&] (auto index, auto d) noexcept
          {
            return index * _tile_counts[d] + tile_coords[d];
          }
        )
      };

    auto const bit_index
      { std::ranges::fold_left
        ( std::views::iota (std::size_t {0}, D)
        , Index {0}
        , [&] (auto index, auto d) noexcept
          {
            return index * tile_extent + local_coords[d];
          }
        )
      };

    auto tiles {_storage.tiles_mutable()};
    assert (tile_index < static_cast<Index> (tiles.size()));
    assert (bit_index < tile_volume);
    assert ((tiles[static_cast<std::size_t> (tile_index)]
             & (Word {1} << static_cast<unsigned> (bit_index))) == 0);

    tiles[static_cast<std::size_t> (tile_index)]
      |= Word {1} << static_cast<unsigned> (bit_index);
    _last_added = point;
    _storage.bump_point_count();
  }

  template<std::size_t D, block_bitmap::Storage<D> S>
    auto BlockBitmap<D, S>::build() && noexcept -> BlockBitmap
  {
    return std::move (*this);
  }

  template<std::size_t D, block_bitmap::Storage<D> S>
    auto BlockBitmap<D, S>::size() const noexcept -> Index
  {
    return _storage.point_count();
  }

  template<std::size_t D, block_bitmap::Storage<D> S>
    auto BlockBitmap<D, S>::bytes() const noexcept -> std::size_t
  {
    return sizeof (*this) + _storage.tiles().size_bytes();
  }

  template<std::size_t D, block_bitmap::Storage<D> S>
    auto BlockBitmap<D, S>::at (Index index) const -> Point<D>
  {
    auto const tiles {_storage.tiles()};

    if (!(index < size()))
    {
      throw std::out_of_range
        {"BlockBitmap::at: index out of range"};
    }

    auto remaining {index};
    auto const flat_tile_it
      { std::ranges::find_if
        ( tiles
        , [&remaining] (Word tile) noexcept
          {
            auto const count {static_cast<Index> (std::popcount (tile))};

            if (remaining < count)
            {
              return true;
            }

            remaining -= count;
            return false;
          }
        )
      };

    assert (flat_tile_it != std::ranges::end (tiles));

    auto const flat_tile
      { static_cast<std::size_t>
        (std::ranges::distance (std::ranges::begin (tiles), flat_tile_it))
      };

    // Find the remaining-th set bit within the tile.
    auto word {*flat_tile_it};

    std::ranges::for_each
      ( std::views::iota (Index {0}, remaining)
      , [&word] (auto) noexcept
        {
          word &= word - 1;
        }
      );

    auto const bit_index {static_cast<Index> (std::countr_zero (word))};

    // Recover tile coordinates (reverse row-major).
    auto tile_coords {std::array<Index, D>{}};

    std::ranges::transform
      ( _tile_counts | std::views::reverse
      , std::rbegin (tile_coords)
      , [remaining = static_cast<Index> (flat_tile)]
          ( auto tile_count
          ) mutable noexcept
        {
          auto const tile_coord {remaining % tile_count};
          remaining /= tile_count;
          return tile_coord;
        }
      );

    // Recover within-tile coordinates (reverse row-major).
    auto local_coords {std::array<Index, D>{}};

    std::ranges::transform
      ( std::views::iota (std::size_t {0}, D)
      , std::rbegin (local_coords)
      , [remaining = bit_index] (auto) mutable noexcept
        {
          auto const local_coord {remaining % tile_extent};
          remaining /= tile_extent;
          return local_coord;
        }
      );

    auto coordinates {std::array<Coordinate, D>{}};

    std::ranges::transform
      ( std::views::iota (std::size_t {0}, D)
      , std::ranges::begin (coordinates)
      , [&] (auto d) noexcept
        {
          auto const grid_coordinate
            {tile_coords[d] * tile_extent + local_coords[d]};

          return _grid.origin()[d]
            + static_cast<Coordinate> (grid_coordinate)
              * _grid.strides()[d]
            ;
        }
      );

    return Point<D> {coordinates};
  }

  template<std::size_t D, block_bitmap::Storage<D> S>
    auto BlockBitmap<D, S>::pos (Point<D> point) const -> Index
  {
    auto tile_coords {std::array<Index, D>{}};
    auto local_coords {std::array<Index, D>{}};

    std::ranges::for_each
      ( std::views::iota (std::size_t {0}, D)
      , [&] (auto d)
        {
          auto const coord {point[d]};
          auto const origin {_grid.origin()[d]};
          auto const stride {_grid.strides()[d]};

          if (coord < origin)
          {
            throw std::out_of_range {"BlockBitmap::pos: point out of range"};
          }

          if ((coord - origin) % stride != 0)
          {
            throw std::out_of_range {"BlockBitmap::pos: point out of range"};
          }

          auto const grid_coordinate
            {static_cast<Index> ((coord - origin) / stride)};

          if (!(grid_coordinate < static_cast<Index> (_grid.extents()[d])))
          {
            throw std::out_of_range {"BlockBitmap::pos: point out of range"};
          }

          tile_coords[d] = grid_coordinate / tile_extent;
          local_coords[d] = grid_coordinate % tile_extent;
        }
      );

    auto const tile_index
      { std::ranges::fold_left
        ( std::views::iota (std::size_t {0}, D)
        , Index {0}
        , [&] (auto index, auto d) noexcept
          {
            return index * _tile_counts[d] + tile_coords[d];
          }
        )
      };

    auto const bit_index
      { std::ranges::fold_left
        ( std::views::iota (std::size_t {0}, D)
        , Index {0}
        , [&] (auto index, auto d) noexcept
          {
            return index * tile_extent + local_coords[d];
          }
        )
      };

    auto const tiles {_storage.tiles()};
    auto const tile_i {static_cast<std::size_t> (tile_index)};

    if (!(tile_i < tiles.size())
        || ( tiles[tile_i]
           & (Word {1} << static_cast<unsigned> (bit_index))
           ) == 0)
    {
      throw std::out_of_range {"BlockBitmap::pos: point out of range"};
    }

    auto index {Index {0}};

    std::ranges::for_each
      ( std::views::counted (std::ranges::begin (tiles), tile_i)
      , [&] (Word tile) noexcept
        {
          index += static_cast<Index> (std::popcount (tile));
        }
      );

    auto const lower_mask
      { bit_index == 0
        ? Word {0}
        : (Word {1} << static_cast<unsigned> (bit_index)) - 1
      };
   index += static_cast<Index> (std::popcount (tiles[tile_i] & lower_mask));

    return index;
  }

  template<std::size_t D, block_bitmap::Storage<D> S>
    auto BlockBitmap<D, S>::try_pos
      ( Point<D> point
      ) const noexcept -> std::optional<Index>
  {
    auto tile_coords {std::array<Index, D>{}};
    auto local_coords {std::array<Index, D>{}};

    for (auto d {std::size_t {0}}; d < D; ++d)
    {
      auto const coord {point[d]};
      auto const origin {_grid.origin()[d]};
      auto const stride {_grid.strides()[d]};

      if (coord < origin)
      {
        return std::nullopt;
      }

      if ((coord - origin) % stride != 0)
      {
        return std::nullopt;
      }

      auto const grid_coordinate
        {static_cast<Index> ((coord - origin) / stride)};

      if (!(grid_coordinate < static_cast<Index> (_grid.extents()[d])))
      {
        return std::nullopt;
      }

      tile_coords[d] = grid_coordinate / tile_extent;
      local_coords[d] = grid_coordinate % tile_extent;
    }

    auto const tile_index
      { std::ranges::fold_left
        ( std::views::iota (std::size_t {0}, D)
        , Index {0}
        , [&] (auto index, auto d) noexcept
          {
            return index * _tile_counts[d] + tile_coords[d];
          }
        )
      };

    auto const bit_index
      { std::ranges::fold_left
        ( std::views::iota (std::size_t {0}, D)
        , Index {0}
        , [&] (auto index, auto d) noexcept
          {
            return index * tile_extent + local_coords[d];
          }
        )
      };

    auto const tiles {_storage.tiles()};
    auto const tile_i {static_cast<std::size_t> (tile_index)};

    if (!(tile_i < tiles.size())
        || ( tiles[tile_i]
           & (Word {1} << static_cast<unsigned> (bit_index))
           ) == 0)
    {
      return std::nullopt;
    }

    auto index {Index {0}};

    std::ranges::for_each
      ( std::views::counted (std::ranges::begin (tiles), tile_i)
      , [&] (Word tile) noexcept
        {
          index += static_cast<Index> (std::popcount (tile));
        }
      );

    auto const lower_mask
      { bit_index == 0
        ? Word {0}
        : (Word {1} << static_cast<unsigned> (bit_index)) - 1
      };
    index += static_cast<Index>
      (std::popcount (tiles[tile_i] & lower_mask));

    return index;
  }

  template<std::size_t D, block_bitmap::Storage<D> S>
    auto BlockBitmap<D, S>::restrict
      ( Cuboid<D> query
      ) const -> std::optional<BlockBitmap<D>>
  {
    auto const tiles {_storage.tiles()};
    auto const origin {_grid.origin()};
    auto const strides {_grid.strides()};
    auto const shape_statistics {detail::query_shape_statistics (_grid, query)};

    if (!shape_statistics)
    {
      return std::nullopt;
    }

    auto const& statistics {*shape_statistics};
    auto restricted
      {BlockBitmap<D> {detail::grid_from_cuboid (statistics.intersection)}};
    auto const use_pointwise_probe
      { statistics.point_count <= Index {4} * tile_volume
     || statistics.points_per_interval < Index {8} * tile_extent
      };

    if (use_pointwise_probe)
    {
      for (auto const& point : enumerate (statistics.intersection))
      {
        auto tile_coordinates {std::array<Index, D> {}};
        auto local_coordinates {std::array<Index, D> {}};

        for (auto d {std::size_t {0}}; d < D; ++d)
        {
          auto const grid_coordinate
            { static_cast<Index>
              ((point[d] - origin[d]) / strides[d])
            };

          tile_coordinates[d] = grid_coordinate / tile_extent;
          local_coordinates[d] = grid_coordinate % tile_extent;
        }

        auto tile_index {Index {0}};

        for (auto d {std::size_t {0}}; d < D; ++d)
        {
          tile_index = tile_index * _tile_counts[d] + tile_coordinates[d];
        }

        auto bit_index {Index {0}};

        for (auto d {std::size_t {0}}; d < D; ++d)
        {
          bit_index = bit_index * tile_extent + local_coordinates[d];
        }

        auto const tile_offset {static_cast<std::size_t> (tile_index)};
        auto const mask {Word {1} << static_cast<unsigned> (bit_index)};

        if ((tiles[tile_offset] & mask) != 0)
        {
          restricted.add (point);
        }
      }
    }
    else
    {
      for ( auto const& [first, last]
          : detail::query_linear_intervals (_grid, statistics.intersection)
          )
      {
        auto const first_point {_grid.at (first)};
        auto const last_point {_grid.at (last)};
        auto tile_coords {std::array<Index, D> {}};
        auto local_coords {std::array<Index, D> {}};

        for (auto d {std::size_t {0}}; d < D; ++d)
        {
          auto const grid_coordinate
            { static_cast<Index>
              ((first_point[d] - origin[d]) / strides[d])
            };

          tile_coords[d] = grid_coordinate / tile_extent;
          local_coords[d] = grid_coordinate % tile_extent;
        }

        auto prefix_tile_index {Index {0}};

        for (auto d {std::size_t {0}}; d + 1 < D; ++d)
        {
          prefix_tile_index = prefix_tile_index * _tile_counts[d] + tile_coords[d];
        }

        auto prefix_bit_index {Index {0}};

        for (auto d {std::size_t {0}}; d + 1 < D; ++d)
        {
          prefix_bit_index = prefix_bit_index * tile_extent + local_coords[d];
        }

        auto const base_bit_index {prefix_bit_index * tile_extent};
        auto const first_grid_coordinate_last
          { static_cast<Index>
            ((first_point[D - 1] - origin[D - 1]) / strides[D - 1])
          };
        auto const last_grid_coordinate_last
          { static_cast<Index>
            ((last_point[D - 1] - origin[D - 1]) / strides[D - 1])
          };
        auto const first_tile_coordinate_last
          {first_grid_coordinate_last / tile_extent};
        auto const last_tile_coordinate_last
          {last_grid_coordinate_last / tile_extent};
        auto coordinates {std::array<Coordinate, D> {}};

        std::ranges::transform
          ( std::views::iota (std::size_t {0}, D)
          , std::ranges::begin (coordinates)
          , [&] (auto d) noexcept
            {
              return first_point[d];
            }
          );

        for ( auto tile_coordinate_last {first_tile_coordinate_last}
            ; tile_coordinate_last <= last_tile_coordinate_last
            ; ++tile_coordinate_last
            )
        {
          auto const tile_index
            {prefix_tile_index * _tile_counts[D - 1] + tile_coordinate_last};
          auto masked {tiles[static_cast<std::size_t> (tile_index)]};
          auto const first_local_last
            { tile_coordinate_last == first_tile_coordinate_last
                ? first_grid_coordinate_last % tile_extent
                : Index {0}
            };
          auto const last_local_last
            { tile_coordinate_last == last_tile_coordinate_last
                ? last_grid_coordinate_last % tile_extent
                : tile_extent - 1
            };
          auto const first_bit {base_bit_index + first_local_last};
          auto const last_bit {base_bit_index + last_local_last};
          auto const lower_mask
            { first_bit == 0
                ? Word {0}
                : (Word {1} << static_cast<unsigned> (first_bit)) - 1
            };
          auto const upper_mask
            { last_bit + 1 == tile_volume
                ? std::numeric_limits<Word>::max()
                : (Word {1} << static_cast<unsigned> (last_bit + 1)) - 1
            };

          masked &= ~lower_mask;
          masked &= upper_mask;

          while (masked != 0)
          {
            auto const bit_index
              {static_cast<Index> (std::countr_zero (masked))};
            auto const local_coordinate_last {bit_index - base_bit_index};

            coordinates[D - 1] = origin[D - 1]
              + static_cast<Coordinate>
                  (tile_coordinate_last * tile_extent + local_coordinate_last)
                * strides[D - 1]
              ;
            restricted.add (Point<D> {coordinates});
            masked &= masked - 1;
          }
        }
      }
    }

    if (restricted.size() == Index {0})
    {
      return std::nullopt;
    }

    return std::move (restricted).build();
  }

  template<std::size_t D, block_bitmap::Storage<D> S>
    auto BlockBitmap<D, S>::storage() const noexcept -> S const&
  {
    return _storage;
  }

  template<std::size_t D, block_bitmap::Storage<D> S>
    auto BlockBitmap<D, S>::compute_tile_counts
      ( Grid<D> const& g
      ) noexcept -> std::array<Index, D>
  {
    auto counts {std::array<Index, D>{}};

    std::ranges::transform
      ( std::views::iota (std::size_t {0}, D)
      , std::ranges::begin (counts)
      , [&g] (auto d) noexcept
        {
          return ( static_cast<Index> (g.extents()[d])
                   + tile_extent - 1
                 ) / tile_extent;
        }
      );

    return counts;
  }

  template<std::size_t D, block_bitmap::Storage<D> S>
    auto BlockBitmap<D, S>::make_layout
      ( Grid<D> const& g
      ) noexcept -> grid::ReservedLayout<D>
  {
    return grid::make_reserved_layout<D> (g);
  }

  template<std::size_t D, block_bitmap::Storage<D> S>
    auto BlockBitmap<D, S>::make_grid
      ( grid::ReservedLayout<D> const& layout
      ) noexcept -> Grid<D>
  {
    return Grid<D>
      { Origin<D>  {layout.origin}
      , Extents<D> {layout.extents}
      , Strides<D> {layout.strides}
      };
  }
}
