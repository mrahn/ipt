#pragma once

#include <array>
#include <baseline/Extents.hpp>
#include <baseline/Grid/Grid.hpp>
#include <baseline/Origin.hpp>
#include <baseline/Roaring/Concepts.hpp>
#include <baseline/Roaring/Vector.hpp>
#include <baseline/Strides.hpp>
#include <baseline/detail/restrict.hpp>
#include <baseline/detail/select.hpp>
#include <baseline/enumerate.hpp>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <ipt/Cuboid.hpp>
#include <ipt/Index.hpp>
#include <ipt/Point.hpp>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>

namespace ipt::baseline
{
  // External baseline: Roaring bitmap (vendored CRoaring v4.6.1, see
  // src/third_party/CRoaring/). Linearises points through the bounding Grid
  // exactly as DenseBitset and BlockBitmap do, and stores the resulting
  // 64-bit ids in a Roaring64Map. Selected as the mature open-source
  // compressed-bitmap baseline; uses the 64-bit map so that bounding
  // grids larger than 2^32 are supported.
  template< std::size_t D
          , ::ipt::baseline::roaring::Storage<D> S
              = ::ipt::baseline::roaring::storage::Vector<D>
          >
    struct Roaring
  {
    [[nodiscard]] explicit Roaring (Grid<D>) noexcept
      requires std::same_as<S, ::ipt::baseline::roaring::storage::Vector<D>>
      ;

    template<typename... Args>
      requires
        ( std::constructible_from<S, Args&&...>
       && !std::same_as<S, ::ipt::baseline::roaring::storage::Vector<D>>
        )
    [[nodiscard]] explicit Roaring (std::in_place_t, Args&&...);

    [[nodiscard]] static constexpr auto name() noexcept;

    auto add (Point<D> const&) noexcept -> void
      requires std::same_as<S, ::ipt::baseline::roaring::storage::Vector<D>>
      ;

    [[nodiscard]] auto build() && noexcept -> Roaring
      requires std::same_as<S, ::ipt::baseline::roaring::storage::Vector<D>>
      ;

    [[nodiscard]] auto size() const noexcept -> Index;
    [[nodiscard]] auto bytes() const noexcept -> std::size_t;

    [[nodiscard]] auto at (Index) const -> Point<D>;
    [[nodiscard]] auto pos (Point<D>) const -> Index;
    [[nodiscard]] auto try_pos
      ( Point<D>
      ) const noexcept -> std::optional<Index>
      ;

    [[nodiscard]] auto restrict
      ( Cuboid<D>
      ) const -> std::optional<Roaring<D>>
      ;

    [[nodiscard]] auto storage() const noexcept -> S const&;

  private:
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
    std::optional<Point<D>> _last_added;
  };
}

namespace ipt::baseline
{
  template<std::size_t D, ::ipt::baseline::roaring::Storage<D> S>
    Roaring<D, S>::Roaring (Grid<D> grid) noexcept
      requires std::same_as<S, ::ipt::baseline::roaring::storage::Vector<D>>
        : _storage {make_layout (grid)}
        , _grid {grid}
  {}

  template<std::size_t D, ::ipt::baseline::roaring::Storage<D> S>
    template<typename... Args>
      requires
        ( std::constructible_from<S, Args&&...>
       && !std::same_as<S, ::ipt::baseline::roaring::storage::Vector<D>>
        )
    Roaring<D, S>::Roaring (std::in_place_t, Args&&... args)
      : _storage {std::forward<Args> (args)...}
      , _grid {make_grid (_storage.layout())}
  {}

  template<std::size_t D, ::ipt::baseline::roaring::Storage<D> S>
    constexpr auto Roaring<D, S>::name() noexcept
  {
    return "roaring";
  }

  template<std::size_t D, ::ipt::baseline::roaring::Storage<D> S>
    auto Roaring<D, S>::add (Point<D> const& point) noexcept -> void
      requires std::same_as<S, ::ipt::baseline::roaring::storage::Vector<D>>
  {
    assert (_storage.point_count() < _grid.size());
    assert (!_last_added || *_last_added < point);

    auto const linear_index {_grid.UNCHECKED_pos (point)};

    _storage.bitmap_mutable().add
      (static_cast<std::uint64_t> (linear_index));
    _last_added = point;
    _storage.bump_point_count();
  }

  template<std::size_t D, ::ipt::baseline::roaring::Storage<D> S>
    auto Roaring<D, S>::build() && noexcept -> Roaring
      requires std::same_as<S, ::ipt::baseline::roaring::storage::Vector<D>>
  {
    _storage.bitmap_mutable().runOptimize();
    _storage.bitmap_mutable().shrinkToFit();
    return std::move (*this);
  }

  template<std::size_t D, ::ipt::baseline::roaring::Storage<D> S>
    auto Roaring<D, S>::size() const noexcept -> Index
  {
    return _storage.point_count();
  }

  template<std::size_t D, ::ipt::baseline::roaring::Storage<D> S>
    auto Roaring<D, S>::bytes() const noexcept -> std::size_t
  {
    return sizeof (*this) + _storage.bitmap().getSizeInBytes (true);
  }

  template<std::size_t D, ::ipt::baseline::roaring::Storage<D> S>
    auto Roaring<D, S>::at (Index index) const -> Point<D>
  {
    if (!(index < size()))
    {
      throw std::out_of_range {"Roaring::at: index out of range"};
    }

    auto element {std::uint64_t {0}};

    if (!_storage.bitmap().select
          (static_cast<std::uint64_t> (index), std::addressof (element)))
    {
      throw std::out_of_range {"Roaring::at: index out of range"};
    }

    return _grid.at (static_cast<Index> (element));
  }

  template<std::size_t D, ::ipt::baseline::roaring::Storage<D> S>
    auto Roaring<D, S>::pos (Point<D> point) const -> Index
  {
    auto const linear_index {_grid.pos (point)};
    auto const offset
      { _storage.bitmap().getIndex
          (static_cast<std::uint64_t> (linear_index))
      };

    if (offset < 0)
    {
      throw std::out_of_range {"Roaring::pos: point out of range"};
    }

    return static_cast<Index> (offset);
  }

  template<std::size_t D, ::ipt::baseline::roaring::Storage<D> S>
    auto Roaring<D, S>::try_pos
      ( Point<D> point
      ) const noexcept -> std::optional<Index>
  {
    auto const linear_opt {_grid.try_pos (point)};

    if (!linear_opt)
    {
      return std::nullopt;
    }

    auto const offset
      { _storage.bitmap().getIndex
          (static_cast<std::uint64_t> (*linear_opt))
      };

    if (offset < 0)
    {
      return std::nullopt;
    }

    return static_cast<Index> (offset);
  }

  template<std::size_t D, ::ipt::baseline::roaring::Storage<D> S>
    auto Roaring<D, S>::restrict
      ( Cuboid<D> query
      ) const -> std::optional<Roaring<D>>
  {
    auto const& bitmap {_storage.bitmap()};
    auto const shape_statistics {detail::query_shape_statistics (_grid, query)};

    if (!shape_statistics)
    {
      return std::nullopt;
    }

    auto const& statistics {*shape_statistics};
    auto restricted
      {Roaring<D> {detail::grid_from_cuboid (statistics.intersection)}};
    auto const use_pointwise_probe
      { statistics.point_count <= Index {128}
     || statistics.points_per_interval * size() < Index {4} * _grid.size()
      };

    if (use_pointwise_probe)
    {
      for (auto const& point : enumerate (statistics.intersection))
      {
        auto const linear {_grid.UNCHECKED_pos (point)};

        if (bitmap.contains (static_cast<std::uint64_t> (linear)))
        {
          restricted.add (point);
        }
      }
    }
    else
    {
      auto const& last_ruler {statistics.intersection.ruler (D - 1)};
      auto const last_grid_stride {_grid.strides()[D - 1]};

      assert (Coordinate {0} < last_grid_stride);
      assert (last_ruler.stride() % last_grid_stride == Coordinate {0});

      auto const last_linear_step
        { last_ruler.size() == Index {1}
            ? Index {1}
            : static_cast<Index> (last_ruler.stride() / last_grid_stride)
        };

      if (last_linear_step == Index {1})
      {
        auto iterator {bitmap.begin()};
        auto const end {bitmap.end()};

        for ( auto const& [first, last]
            : detail::query_linear_intervals (_grid, statistics.intersection)
            )
        {
          if (iterator == end)
          {
            break;
          }

          if (*iterator < static_cast<std::uint64_t> (first))
          {
            if (!iterator.move_equalorlarger
                  (static_cast<std::uint64_t> (first)))
            {
              break;
            }
          }

          while (iterator != end)
          {
            auto const linear {static_cast<Index> (*iterator)};

            if (last < linear)
            {
              break;
            }

            restricted.add (_grid.at (linear));
            ++iterator;
          }
        }
      }
      else
      {
        auto iterator {bitmap.begin()};
        auto const end {bitmap.end()};

        if (iterator != end)
        {
          auto const prefix_rulers
            { [&]<std::size_t... Dimensions>
                ( std::index_sequence<Dimensions...>
                ) noexcept
              {
                return std::array<Ruler, D>
                  { [&]() noexcept -> Ruler
                      {
                        if constexpr (Dimensions + 1 == D)
                        {
                          return Ruler
                            {typename Ruler::Singleton {last_ruler.begin()}};
                        }

                        return statistics.intersection.ruler (Dimensions);
                      }()...
                  };
              } (std::make_index_sequence<D>{})
            };

          auto const prefix_cuboid {Cuboid<D> {std::move (prefix_rulers)}};

          for (auto const& prefix : enumerate (prefix_cuboid))
          {
            if (iterator == end)
            {
              break;
            }

            auto last_coordinates {std::array<Coordinate, D> {}};

            for (auto d {std::size_t {0}}; d < D; ++d)
            {
              last_coordinates[d] = prefix[d];
            }

            auto const first {_grid.UNCHECKED_pos (prefix)};
            last_coordinates[D - 1] = last_ruler.lub();
            auto const last {_grid.UNCHECKED_pos (Point<D> {last_coordinates})};

            if (*iterator < static_cast<std::uint64_t> (first))
            {
              if (!iterator.move_equalorlarger
                    (static_cast<std::uint64_t> (first)))
              {
                break;
              }
            }

            while (iterator != end)
            {
              auto const linear {static_cast<Index> (*iterator)};

              if (last < linear)
              {
                break;
              }

              auto const offset {linear - first};

              if (offset % last_linear_step == 0)
              {
                restricted.add (_grid.at (linear));
              }

              ++iterator;
            }
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

  template<std::size_t D, ::ipt::baseline::roaring::Storage<D> S>
    auto Roaring<D, S>::storage() const noexcept -> S const&
  {
    return _storage;
  }

  template<std::size_t D, ::ipt::baseline::roaring::Storage<D> S>
    auto Roaring<D, S>::make_layout
      ( Grid<D> const& g
      ) noexcept -> grid::ReservedLayout<D>
  {
    return grid::make_reserved_layout<D> (g);
  }

  template<std::size_t D, ::ipt::baseline::roaring::Storage<D> S>
    auto Roaring<D, S>::make_grid
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
