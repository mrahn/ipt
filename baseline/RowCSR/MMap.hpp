#pragma once

#include <baseline/RowCSR/Concepts.hpp>
#include <baseline/storage/MappedFile.hpp>
#include <cstddef>
#include <filesystem>
#include <ipt/Index.hpp>
#include <span>
#include <string_view>

namespace ipt::baseline::row_csr::storage
{
  template<std::size_t D, std::size_t K>
    struct MMap
  {
    [[nodiscard]] explicit MMap (std::filesystem::path const&);

    MMap (MMap const&) = delete;
    MMap (MMap&&) = delete;
    auto operator= (MMap const&) -> MMap& = delete;
    auto operator= (MMap&&) -> MMap& = delete;
    ~MMap() = default;

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

    [[nodiscard]] auto description
      (
      ) const noexcept -> std::string_view
      ;

  private:
    ipt::baseline::storage::MappedFile _file;
    std::span<RowKey<D, K> const> _row_keys{};
    std::span<ipt::Index const> _row_offsets{};
    std::span<ColumnValue<D, K> const> _column_values{};
  };
}

#include "detail/MMap.ipp"
