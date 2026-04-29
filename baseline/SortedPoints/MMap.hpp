#pragma once

#include <baseline/storage/MappedFile.hpp>
#include <cstddef>
#include <filesystem>
#include <ipt/Point.hpp>
#include <span>
#include <string_view>

namespace ipt::baseline::sorted_points::storage
{
  // Read-only storage backed by a mmap. Spans point directly into
  // the mapped pages: zero-copy. Non-copyable, non-movable.
  template<std::size_t D>
    struct MMap
  {
    [[nodiscard]] explicit MMap (std::filesystem::path const&);

    MMap (MMap const&) = delete;
    MMap (MMap&&) = delete;
    auto operator= (MMap const&) -> MMap& = delete;
    auto operator= (MMap&&) -> MMap& = delete;
    ~MMap() = default;

    [[nodiscard]] auto points
      (
      ) const noexcept -> std::span<ipt::Point<D> const>
      ;

    [[nodiscard]] auto description
      (
      ) const noexcept -> std::string_view
      ;

  private:
    ipt::baseline::storage::MappedFile _file;
    std::span<ipt::Point<D> const> _points{};
  };
}

#include "detail/MMap.ipp"
