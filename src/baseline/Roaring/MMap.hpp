#pragma once

#include <baseline/Grid/Header.hpp>
#include <baseline/Roaring/Concepts.hpp>
#include <baseline/storage/MappedFile.hpp>
#include <cstddef>
#include <filesystem>
#include <ipt/Index.hpp>
#include <string_view>

#include <third_party/CRoaring/roaring.hh>

namespace ipt::baseline::roaring::storage
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

    [[nodiscard]] auto bitmap
      (
      ) const noexcept -> ::roaring::Roaring64Map const&
      ;

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
    ::roaring::Roaring64Map _bitmap;
    ipt::Index _point_count {0};
  };
}

#include "detail/MMap.ipp"
