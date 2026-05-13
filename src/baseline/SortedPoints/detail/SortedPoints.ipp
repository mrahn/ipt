#include <algorithm>
#include <cassert>
#include <ipt/Index.hpp>
#include <iterator>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <utility>

namespace ipt::baseline
{
  template<std::size_t D, sorted_points::Storage<D> S>
    template<typename... Args>
      requires std::constructible_from<S, Args&&...>
    SortedPoints<D, S>::SortedPoints (std::in_place_t, Args&&... args)
      : _storage {std::forward<Args> (args)...}
  {}

  template<std::size_t D, sorted_points::Storage<D> S>
    auto SortedPoints<D, S>::add (Point<D> const& point) -> void
      requires std::same_as<S, sorted_points::storage::Vector<D>>
  {
    assert (_storage.empty() || _storage.last() < point);

    _storage.push_back (point);
  }

  template<std::size_t D, sorted_points::Storage<D> S>
    auto SortedPoints<D, S>::build() && noexcept -> SortedPoints
  {
    return std::move (*this);
  }

  template<std::size_t D, sorted_points::Storage<D> S>
    constexpr auto SortedPoints<D, S>::name() noexcept
  {
    return "sorted-points";
  }

  template<std::size_t D, sorted_points::Storage<D> S>
    auto SortedPoints<D, S>::size() const noexcept -> Index
  {
    return _storage.points().size();
  }

  template<std::size_t D, sorted_points::Storage<D> S>
    auto SortedPoints<D, S>::bytes() const noexcept -> std::size_t
  {
    return _storage.points().size_bytes();
  }

  template<std::size_t D, sorted_points::Storage<D> S>
    auto SortedPoints<D, S>::at (Index index) const -> Point<D>
  {
    auto const points {_storage.points()};

    if (!(index < static_cast<Index> (points.size())))
    {
      throw std::out_of_range
        {"SortedPoints::at: index out of range"};
    }

    return points[index];
  }

  template<std::size_t D, sorted_points::Storage<D> S>
    auto SortedPoints<D, S>::pos (Point<D> point) const -> Index
  {
    auto const points {_storage.points()};
    auto const lb {std::ranges::lower_bound (points, point)};

    if (lb == std::ranges::end (points) || *lb != point)
    {
      throw std::out_of_range
        {"SortedPoints::pos: point out of range"};
    }

    return std::ranges::distance (std::ranges::begin (points), lb);
  }

  template<std::size_t D, sorted_points::Storage<D> S>
    auto SortedPoints<D, S>::try_pos
      ( Point<D> point
      ) const noexcept -> std::optional<Index>
  {
    auto const points {_storage.points()};
    auto const lb {std::ranges::lower_bound (points, point)};

    if (lb == std::ranges::end (points) || *lb != point)
    {
      return std::nullopt;
    }

    return std::ranges::distance (std::ranges::begin (points), lb);
  }

  template<std::size_t D, sorted_points::Storage<D> S>
    auto SortedPoints<D, S>::restrict
      ( Cuboid<D> query
      ) const -> std::optional<SortedPoints<D>>
  {
    auto const points {_storage.points()};
    auto const lo {std::ranges::lower_bound (points, query.glb())};
    auto const hi {std::ranges::upper_bound (points, query.lub())};
    auto restricted_points {std::vector<Point<D>> {}};
    restricted_points.reserve
      (static_cast<std::size_t> (std::ranges::distance (lo, hi)));

    for (auto point {lo}; point != hi; ++point)
    {
      if (query.contains (*point))
      {
        restricted_points.push_back (*point);
      }
    }

    if (restricted_points.empty())
    {
      return std::nullopt;
    }

    return SortedPoints<D> {std::in_place, std::move (restricted_points)};
  }

  template<std::size_t D, sorted_points::Storage<D> S>
    auto SortedPoints<D, S>::storage() const noexcept -> S const&
  {
    return _storage;
  }
}
