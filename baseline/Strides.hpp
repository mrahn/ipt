#pragma once

#include <array>
#include <concepts>
#include <cstddef>
#include <ipt/Coordinate.hpp>
#include <ipt/Index.hpp>

namespace ipt::baseline
{
  template<std::size_t D>
    struct Strides
  {
    [[nodiscard]] explicit constexpr Strides (std::array<Coordinate, D>);

    template<typename... Coordinates>
      requires (  sizeof... (Coordinates) == D
               && (std::convertible_to<Coordinate, Coordinates> && ...)
               )
    [[nodiscard]] explicit constexpr Strides (Coordinates...);

    // Pre: index < D
    [[nodiscard]] constexpr auto operator[]
      ( Index
      ) const noexcept -> Coordinate
      ;

  private:
    std::array<Coordinate, D> _strides;
  };
}

#include "detail/Strides.ipp"
