#pragma once

#include <cstddef>
#include <ipt/Cuboid.hpp>

namespace ipt::baseline
{
  // Lazily enumerate the points of a cuboid in IPT lex order.
  // The cuboid is taken by value and moved into the view so the
  // returned range stays valid even when called on a temporary.
  template<std::size_t D>
    [[nodiscard]] constexpr auto enumerate (Cuboid<D>) noexcept;
}

#include "detail/enumerate.ipp"
