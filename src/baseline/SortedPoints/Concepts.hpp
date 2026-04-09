#pragma once

#include <concepts>
#include <cstddef>
#include <ipt/Point.hpp>
#include <span>

namespace ipt::baseline::sorted_points
{
  template<typename S, std::size_t D>
    concept Storage
      = requires (S const& s)
        {
          { s.points() } -> std::same_as<std::span<ipt::Point<D> const>>;
        };
}
