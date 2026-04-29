#include <baseline/Extents.hpp>
#include <baseline/Origin.hpp>
#include <baseline/Strides.hpp>
#include <baseline/enumerate.hpp>
#include <cassert>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>

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
    auto Roaring<D, S>::select
      ( Cuboid<D> query
      ) const -> std::generator<Cuboid<D>>
  {
    auto const grid_cuboid {to_ipt_cuboid (_grid)};
    auto const intersection {grid_cuboid.intersect (query)};

    if (!intersection)
    {
      co_return;
    }

    for (auto const& point : enumerate (*intersection))
    {
      auto const linear {_grid.UNCHECKED_pos (point)};

      if (_storage.bitmap().contains
            (static_cast<std::uint64_t> (linear)))
      {
        co_yield singleton_cuboid (point);
      }
    }
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
