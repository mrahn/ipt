#pragma once

#include <array>
#include <concepts>
#include <cstddef>
#include <ipt/Coordinate.hpp>
#include <ipt/Index.hpp>

namespace ipt::baseline
{
  template<std::size_t D>
    struct Origin
  {
    [[nodiscard]] explicit constexpr Origin
      ( std::array<Coordinate, D>
      ) noexcept
      ;

    template<typename... Coordinates>
      requires (  sizeof... (Coordinates) == D
               && (std::convertible_to<Coordinate, Coordinates> && ...)
               )
    [[nodiscard]] explicit constexpr Origin (Coordinates...) noexcept;

    // Pre: index < D
    [[nodiscard]] constexpr auto operator[]
      ( Index
      ) const noexcept -> Coordinate
      ;

  private:
    std::array<Coordinate, D> _origin;
  };
}

#include "detail/Origin.ipp"
