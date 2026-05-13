#pragma once

#include <baseline/BlockBitmap/Concepts.hpp>
#include <baseline/Grid/Header.hpp>
#include <baseline/storage/MappedFile.hpp>
#include <cstddef>
#include <filesystem>
#include <ipt/Index.hpp>
#include <span>
#include <string_view>

namespace ipt::baseline::block_bitmap::storage
{
  template<std::size_t D>
    struct MMap
  {
    [[nodiscard]] explicit MMap (std::filesystem::path const&);

    MMap (MMap const&) = delete;
    MMap (MMap&&) = delete;
    auto operator= (MMap const&) -> MMap& = delete;
    auto operator= (MMap&&) -> MMap& = delete;
    ~MMap() = default;

    [[nodiscard]] auto tiles() const noexcept -> std::span<Word const>;
    [[nodiscard]] auto point_count() const noexcept -> ipt::Index;

    [[nodiscard]] auto layout
      (
      ) const noexcept -> ipt::baseline::grid::ReservedLayout<D> const&
      ;

    [[nodiscard]] auto description
      (
      ) const noexcept -> std::string_view
      ;

  private:
    ipt::baseline::storage::MappedFile _file;
    ipt::baseline::grid::ReservedLayout<D> _layout{};
    std::span<Word const> _tiles{};
    ipt::Index _point_count {0};
  };
}

#include "detail/MMap.ipp"
