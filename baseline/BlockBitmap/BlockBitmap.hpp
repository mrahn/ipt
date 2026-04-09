#pragma once

#include <baseline/BlockBitmap/Concepts.hpp>
#include <baseline/BlockBitmap/Vector.hpp>
#include <baseline/Grid/Grid.hpp>
#include <concepts>
#include <cstddef>
#include <generator>
#include <ipt/Cuboid.hpp>
#include <ipt/Index.hpp>
#include <ipt/Point.hpp>
#include <optional>
#include <utility>

namespace ipt::baseline
{
  // Tile-based bitmap baseline.  The bounding grid is divided into
  // axis-aligned tiles of tile_extent grid points per dimension, where
  // tile_extent is the largest power of two such that tile_extent^D ≤ 64.
  // Every tile is stored as one uint64_t word; bits within a tile are laid
  // out in row-major order.  Memory per point equals 1 bit, same as
  // DenseBitset, but the tile granularity gives better spatial locality for
  // D-dimensional access patterns.
  template< std::size_t D
          , block_bitmap::Storage<D> S = block_bitmap::storage::Vector<D>
          >
    struct BlockBitmap
  {
    using Word = block_bitmap::Word;

    // Largest power of 2 such that tile_extent^D fits in one 64-bit word.
    // D=1: 64, D=2: 8, D=3: 4, D=6: 2, D>6: 1.
    static constexpr Index tile_extent
      { []() constexpr noexcept -> Index
        {
          auto e {Index {64}};
          while (true)
          {
            auto vol {Index {1}};
            for (std::size_t k {0}; k < D; ++k)
            {
              vol *= e;
            }

            if (vol <= 64)
            {
              return e;
            }

            e /= 2;
          }
        }()
      };

    static constexpr Index tile_volume
      { []() constexpr noexcept -> Index
        {
          auto vol {Index {1}};
          for (std::size_t k {0}; k < D; ++k)
          {
            vol *= tile_extent;
          }
          return vol;
        }()
      };

    [[nodiscard]] explicit BlockBitmap (Grid<D>)
      requires std::same_as<S, block_bitmap::storage::Vector<D>>
      ;

    template<typename... Args>
      requires
        ( std::constructible_from<S, Args&&...>
       && !std::same_as<S, block_bitmap::storage::Vector<D>>
        )
    [[nodiscard]] explicit BlockBitmap (std::in_place_t, Args&&...);

    [[nodiscard]] static constexpr auto name() noexcept;

    auto add (Point<D> const&) noexcept -> void
      requires std::same_as<S, block_bitmap::storage::Vector<D>>
      ;

    [[nodiscard]] auto build() && noexcept -> BlockBitmap;

    [[nodiscard]] auto size() const noexcept -> Index;
    [[nodiscard]] auto bytes() const noexcept -> std::size_t;

    [[nodiscard]] auto at (Index) const -> Point<D>;
    [[nodiscard]] auto pos (Point<D>) const -> Index;
    [[nodiscard]] auto try_pos
      ( Point<D>
      ) const noexcept -> std::optional<Index>
      ;

    [[nodiscard]] auto select (Cuboid<D>) const -> std::generator<Cuboid<D>>;

    [[nodiscard]] auto storage() const noexcept -> S const&;

  private:
    [[nodiscard]] static auto compute_tile_counts
      ( Grid<D> const&
      ) noexcept -> std::array<Index, D>
      ;

    [[nodiscard]] static auto make_layout
      ( Grid<D> const&
      ) noexcept -> grid::ReservedLayout<D>
      ;

    [[nodiscard]] static auto make_grid
      ( grid::ReservedLayout<D> const&
      ) noexcept -> Grid<D>
      ;

    S _storage;
    Grid<D> _grid;
    std::array<Index, D> _tile_counts;
    std::optional<Point<D>> _last_added;
  };
}

#include "detail/BlockBitmap.ipp"
