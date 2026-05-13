#pragma once

#include <baseline/Extents.hpp>
#include <baseline/Origin.hpp>
#include <baseline/Strides.hpp>
#include <cstddef>
#include <ipt/Cuboid.hpp>

namespace ipt::baseline
{
  template<std::size_t D>
    [[nodiscard]] constexpr auto make_cuboid
      ( Origin<D>
      , Extents<D>
      , Strides<D>
      ) -> Cuboid<D>
      ;
}

#include "detail/make_cuboid.ipp"
