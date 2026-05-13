#pragma once

#include <array>
#include <baseline/Grid/Grid.hpp>
#include <baseline/enumerate.hpp>
#include <cstddef>
#include <generator>
#include <optional>
#include <utility>

namespace ipt::baseline::detail
{
  template<std::size_t D>
    struct QueryShapeStatistics
    {
      Cuboid<D> intersection;
      Index point_count;
      Index interval_count;
      Index points_per_interval;
    };

  template<std::size_t D>
    auto query_shape_statistics
      ( Grid<D> const& grid
      , Cuboid<D> const& query
      ) -> std::optional<QueryShapeStatistics<D>>
  {
    auto intersection {to_ipt_cuboid (grid).intersect (query)};

    if (!intersection)
    {
      return std::nullopt;
    }

    auto const point_count {intersection->size()};
    auto const points_per_interval {intersection->ruler (D - 1).size()};
    auto const interval_count
      { [&] () noexcept
        {
          if constexpr (D == 1)
          {
            return Index {1};
          }

          return point_count / points_per_interval;
        }()
      };

    return QueryShapeStatistics<D>
      { .intersection = std::move (*intersection)
      , .point_count = point_count
      , .interval_count = interval_count
      , .points_per_interval = points_per_interval
      };
  }

  template<std::size_t D>
    auto query_linear_intervals
      ( Grid<D> const& grid
      , Cuboid<D> const& query
      ) -> std::generator<std::pair<Index, Index>>
  {
    auto const intersection {to_ipt_cuboid (grid).intersect (query)};

    if (!intersection)
    {
      co_return;
    }

    if constexpr (D == 1)
    {
      co_yield
        { grid.UNCHECKED_pos (intersection->glb())
        , grid.UNCHECKED_pos (intersection->lub())
        };
      co_return;
    }

    auto prefix_rulers
      { std::invoke
        ( [&]<std::size_t... Dimensions>
            (std::index_sequence<Dimensions...>)
          {
            return std::array<Ruler, D>
              { intersection->ruler (Dimensions)...
              , Ruler
                  { typename Ruler::Singleton
                    {intersection->ruler (D - 1).begin()}
                  }
              };
          }
        , std::make_index_sequence<D - 1> {}
        )
      };
    auto const prefix_cuboid {Cuboid<D> {std::move (prefix_rulers)}};

    for (auto const& prefix : enumerate (prefix_cuboid))
    {
      auto first_coordinates {std::array<Coordinate, D> {}};

      std::ranges::transform
        ( std::views::iota (std::size_t {0}, D)
        , std::ranges::begin (first_coordinates)
        , [&] (auto d) noexcept
          {
            return prefix[d];
          }
        );

      auto last_coordinates {first_coordinates};
      last_coordinates[D - 1] = intersection->ruler (D - 1).lub();

      co_yield
        { grid.UNCHECKED_pos (Point<D> {first_coordinates})
        , grid.UNCHECKED_pos (Point<D> {last_coordinates})
        };
    }
  }
}