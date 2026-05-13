#pragma once

#include <array>
#include <concepts>
#include <cstddef>
#include <ipt/Coordinate.hpp>
#include <ipt/Index.hpp>

namespace ipt::baseline
{
  template<std::size_t D>
    struct Extents
  {
    [[nodiscard]] explicit constexpr Extents (std::array<Coordinate, D>);

    template<typename... Coordinates>
      requires (  sizeof... (Coordinates) == D
               && (std::convertible_to<Coordinate, Coordinates> && ...)
               )
    [[nodiscard]] explicit constexpr Extents (Coordinates...);

    // Pre: index < D
    [[nodiscard]] constexpr auto operator[]
      ( Index
      ) const noexcept -> Coordinate
      ;

    [[nodiscard]] constexpr auto size() const noexcept -> Coordinate;

    [[nodiscard]] constexpr auto begin() const noexcept;
    [[nodiscard]] constexpr auto end() const noexcept;
    [[nodiscard]] constexpr auto rbegin() const noexcept;
    [[nodiscard]] constexpr auto rend() const noexcept;

  private:
    std::array<Coordinate, D> _extents;
  };
}

#include "detail/Extents.ipp"
