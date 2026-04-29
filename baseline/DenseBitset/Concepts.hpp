#pragma once

#include <baseline/Grid/Header.hpp>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <ipt/Index.hpp>
#include <span>

namespace ipt::baseline::dense_bitset
{
  using Word = std::uint64_t;

  template<typename S, std::size_t D>
    concept Storage
      = requires (S const& s)
        {
          { s.bits() } -> std::same_as<std::span<Word const>>;
          { s.point_count() } -> std::same_as<ipt::Index>;
          { s.layout() } -> std::same_as
                              <ipt::baseline::grid::ReservedLayout<D> const&>;
        };
}
