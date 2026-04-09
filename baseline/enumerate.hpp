#pragma once

#include <cstddef>
#include <ipt/Cuboid.hpp>
#include <ipt/Point.hpp>

namespace ipt::baseline
{
  // Lazily enumerate the points of a cuboid in IPT lex order.
  // The cuboid is taken by value and moved into the view so the
  // returned range stays valid even when called on a temporary.
  template<std::size_t D>
    [[nodiscard]] constexpr auto enumerate (Cuboid<D>) noexcept;

  template<std::size_t D>
    [[nodiscard]] auto singleton_cuboid
      ( Point<D> const&
      ) -> Cuboid<D>
      ;
}

#include "detail/enumerate.ipp"
