#pragma once

#include <baseline/Grid/Header.hpp>
#include <concepts>
#include <cstddef>
#include <ipt/Index.hpp>

#include <third_party/CRoaring/roaring.hh>

namespace ipt::baseline::roaring
{
  template<typename S, std::size_t D>
    concept Storage
      = requires (S const& s)
        {
          { s.bitmap() } -> std::same_as<::roaring::Roaring64Map const&>;
          { s.point_count() } -> std::same_as<ipt::Index>;
          { s.layout() } -> std::same_as
                              <ipt::baseline::grid::ReservedLayout<D> const&>;
        };
}
