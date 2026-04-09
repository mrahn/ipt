#include <algorithm>
#include <array>
#include <cassert>
#include <limits>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <utility>

namespace ipt::create
{
  namespace detail
  {
    template<std::size_t D>
      [[nodiscard]] auto make_ruler_for_dimension
        ( std::span<Point<D> const> points
        , std::size_t dimension
        ) -> std::optional<Ruler>
    {
      auto coordinates {std::vector<Coordinate> {}};
      coordinates.reserve (points.size());

      for (auto const& point : points)
      {
        coordinates.push_back (point[dimension]);
      }

      std::ranges::sort (coordinates);
      auto const unique_end {std::ranges::unique (coordinates).begin()};
      coordinates.erase (unique_end, std::ranges::end (coordinates));

      if (coordinates.size() == 1)
      {
        return Ruler {Ruler::Singleton {coordinates.front()}};
      }

      auto const stride {coordinates[1] - coordinates[0]};
      if (!(stride > 0))
      {
        return std::nullopt;
      }

      for ( auto index
          : std::views::iota (std::size_t {1}, coordinates.size())
          )
      {
        if (coordinates[index] - coordinates[index - 1] != stride)
        {
          return std::nullopt;
        }
      }

      return Ruler {coordinates.front(), stride, coordinates.back() + stride};
    }

    template<std::size_t D, std::size_t... Dimensions>
      [[nodiscard]] auto make_cuboid_from_rulers
        ( std::array<std::optional<Ruler>, D> const& rulers
        , std::index_sequence<Dimensions...>
        ) -> Cuboid<D>
    {
      return Cuboid<D> {std::array<Ruler, D> {*rulers[Dimensions]...}};
    }

    template<std::size_t D>
      [[nodiscard]] auto make_segment_cuboid
        ( std::span<Point<D> const> points
        ) -> std::optional<Cuboid<D>>
    {
      if (points.empty())
      {
        return std::nullopt;
      }

      auto rulers {std::array<std::optional<Ruler>, D> {}};
      for (auto dimension : std::views::iota (std::size_t {0}, D))
      {
        rulers[dimension] = make_ruler_for_dimension (points, dimension);
        if (!rulers[dimension])
        {
          return std::nullopt;
        }
      }

      auto cuboid
        { make_cuboid_from_rulers<D>
          (rulers, std::make_index_sequence<D> {})
        };

      if (cuboid.size() != static_cast<Index> (points.size()))
      {
        return std::nullopt;
      }

      auto const contains_all_points
        { std::ranges::all_of
          ( points
          , [&cuboid] (Point<D> const& point) noexcept
            {
              return cuboid.contains (point);
            }
          )
        };

      if (!contains_all_points)
      {
        return std::nullopt;
      }

      return cuboid;
    }
  }

  template<std::size_t D>
    constexpr auto DynamicProgramming<D>::name() noexcept
  {
    return "dynamic-programming";
  }

  template<std::size_t D>
    auto DynamicProgramming<D>::add (Point<D> const& point) -> void
  {
    _points.push_back (point);
  }

  template<std::size_t D>
    auto DynamicProgramming<D>::build() && -> IPT<D>
  {
    auto const point_count {_points.size()};
    auto constexpr unreachable {std::numeric_limits<std::size_t>::max()};

    auto costs {std::vector<std::size_t> (point_count + 1, unreachable)};
    auto predecessors {std::vector<std::size_t> (point_count + 1)};
    auto cuboids {std::vector<std::optional<Cuboid<D>>> (point_count + 1)};
    auto const points {std::span<Point<D> const> {_points}};

    costs[0] = 0;
    for (auto end : std::views::iota (std::size_t {1}, point_count + 1))
    {
      for (auto begin : std::views::iota (std::size_t {0}, end))
      {
        if (costs[begin] == unreachable)
        {
          continue;
        }

        auto maybe_cuboid
          {detail::make_segment_cuboid (points.subspan (begin, end - begin))};
        if (!maybe_cuboid)
        {
          continue;
        }

        auto const candidate {costs[begin] + 1};
        if (candidate < costs[end])
        {
          costs[end] = candidate;
          predecessors[end] = begin;
          cuboids[end] = std::move (*maybe_cuboid);
        }
      }
    }

    if (costs[point_count] == unreachable)
    {
      throw std::logic_error {"DynamicProgramming: no decomposition found"};
    }

    auto selected_cuboids {std::vector<Cuboid<D>> {}};
    selected_cuboids.reserve (costs[point_count]);

    for (auto end {point_count}; end != 0; end = predecessors[end])
    {
      assert (cuboids[end].has_value());
      selected_cuboids.push_back (std::move (*cuboids[end]));
    }

    std::ranges::reverse (selected_cuboids);
    return IPT<D> {std::from_range, std::move (selected_cuboids)};
  }
}