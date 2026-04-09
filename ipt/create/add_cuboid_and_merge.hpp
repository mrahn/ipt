#pragma once

#include <cstddef>
#include <ipt/Cuboid.hpp>
#include <ipt/Entry.hpp>
#include <vector>

namespace ipt::create
{
  template<std::size_t D>
    auto add_cuboid_and_merge
      ( std::vector<Entry<D>>&
      , Cuboid<D> const&
      ) -> void
      ;
}

#include "detail/add_cuboid_and_merge.ipp"
