#pragma once

#include <array>
#include <compare>
#include <cstddef>
#include <ipt/Coordinate.hpp>
#include <ipt/Index.hpp>

namespace ipt
{
  template<std::size_t D>
    struct Point
  {
    [[nodiscard]] constexpr Point (std::array<Coordinate, D>) noexcept;

    // Pre: index < D
    //
    [[nodiscard]] constexpr auto operator[]
      ( Index
      ) const noexcept -> Coordinate
      ;

    [[nodiscard]] constexpr auto operator<=>
      ( Point const&
      ) const noexcept = default
      ;

  private:
    std::array<Coordinate, D> _coordinates;
  };
}

#include "detail/Point.ipp"
