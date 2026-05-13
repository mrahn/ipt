#pragma once

#include <array>
#include <concepts>
#include <cstddef>
#include <ipt/Coordinate.hpp>
#include <ipt/Index.hpp>
#include <span>

namespace ipt::baseline::row_csr
{
  template<std::size_t D, std::size_t K>
    using RowKey = std::array<ipt::Coordinate, K>;

  template<std::size_t D, std::size_t K>
    using ColumnValue = std::array<ipt::Coordinate, D - K>;

  template<typename S, std::size_t D, std::size_t K>
    concept Storage
      = requires (S const& s)
        {
          { s.row_keys() } -> std::same_as<std::span<RowKey<D, K> const>>;
          { s.row_offsets() } -> std::same_as<std::span<ipt::Index const>>;
          { s.column_values() } -> std::same_as
                                     <std::span<ColumnValue<D, K> const>>;
        };
}
