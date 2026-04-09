#pragma once

#include <array>
#include <baseline/Grid/Grid.hpp>
#include <baseline/enumerate.hpp>
#include <concepts>
#include <cstddef>
#include <optional>
#include <ranges>
#include <string_view>
#include <utility>

namespace ipt::baseline::detail
{
  template<std::size_t D, typename Builder, std::ranges::input_range R>
    requires std::same_as<std::ranges::range_value_t<R>, Cuboid<D>>
  auto build_restricted_points
    ( Builder builder
    , R&& cuboids
    , std::string_view structure_name
    ) -> std::optional<decltype (std::move (builder).build())>
  {
    std::ignore = structure_name;

    auto any_selected {false};

    for (auto const& cuboid : cuboids)
    {
      any_selected = true;

      for (auto const& point : enumerate (cuboid))
      {
        builder.add (point);
      }
    }

    if (!any_selected)
    {
      return std::nullopt;
    }

    return std::move (builder).build();
  }

  template<std::size_t D>
    auto grid_from_cuboid (Cuboid<D> const& cuboid) -> Grid<D>
  {
    auto origin {std::array<Coordinate, D> {}};
    auto extents {std::array<Coordinate, D> {}};
    auto strides {std::array<Coordinate, D> {}};

    std::ranges::transform
      ( std::views::iota (std::size_t {0}, D)
      , std::ranges::begin (origin)
      , [&] (auto d) noexcept
        {
          return cuboid.ruler (d).begin();
        }
      );

    std::ranges::transform
      ( std::views::iota (std::size_t {0}, D)
      , std::ranges::begin (extents)
      , [&] (auto d) noexcept
        {
          auto const& ruler {cuboid.ruler (d)};

          return static_cast<Coordinate>
            ((ruler.end() - ruler.begin()) / ruler.stride());
        }
      );

    std::ranges::transform
      ( std::views::iota (std::size_t {0}, D)
      , std::ranges::begin (strides)
      , [&] (auto d) noexcept
        {
          return cuboid.ruler (d).stride();
        }
      );

    return Grid<D>
      { Origin<D> {origin}
      , Extents<D> {extents}
      , Strides<D> {strides}
      };
  }
}