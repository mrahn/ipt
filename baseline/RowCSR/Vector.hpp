#pragma once

#include <baseline/RowCSR/Concepts.hpp>
#include <cstddef>
#include <ipt/Index.hpp>
#include <span>
#include <vector>

namespace ipt::baseline::row_csr::storage
{
  template<std::size_t D, std::size_t K>
    struct Vector
  {
    Vector() noexcept = default;

    [[nodiscard]] auto row_keys
      (
      ) const noexcept -> std::span<RowKey<D, K> const>
      ;

    [[nodiscard]] auto row_offsets
      (
      ) const noexcept -> std::span<ipt::Index const>
      ;

    [[nodiscard]] auto column_values
      (
      ) const noexcept -> std::span<ColumnValue<D, K> const>
      ;

    auto push_row_key (RowKey<D, K> const&) -> void;
    auto push_row_offset (ipt::Index) -> void;
    auto push_column_value (ColumnValue<D, K> const&) -> void;

    [[nodiscard]] auto last_row_key
      (
      ) const noexcept -> RowKey<D, K> const&
      ;

    [[nodiscard]] auto row_keys_empty() const noexcept -> bool;

  private:
    std::vector<RowKey<D, K>> _row_keys;
    std::vector<ipt::Index> _row_offsets;
    std::vector<ColumnValue<D, K>> _column_values;
  };
}

#include "detail/Vector.ipp"
