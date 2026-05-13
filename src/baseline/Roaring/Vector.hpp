#pragma once

#include <baseline/Grid/Header.hpp>
#include <baseline/Roaring/Concepts.hpp>
#include <cstddef>
#include <ipt/Index.hpp>

#include <third_party/CRoaring/roaring.hh>

namespace ipt::baseline::roaring::storage
{
  template<std::size_t D>
    struct Vector
  {
    Vector() noexcept = default;

    [[nodiscard]] explicit Vector
      ( ipt::baseline::grid::ReservedLayout<D>
      )
      ;

    [[nodiscard]] auto bitmap
      (
      ) const noexcept -> ::roaring::Roaring64Map const&
      ;

    [[nodiscard]] auto bitmap_mutable
      (
      ) noexcept -> ::roaring::Roaring64Map&
      ;

    [[nodiscard]] auto point_count() const noexcept -> ipt::Index;

    auto bump_point_count() noexcept -> void;

    [[nodiscard]] auto layout
      (
      ) const noexcept -> ipt::baseline::grid::ReservedLayout<D> const&
      ;

  private:
    ipt::baseline::grid::ReservedLayout<D> _layout{};
    ::roaring::Roaring64Map _bitmap;
    ipt::Index _point_count {0};
  };
}

#include "detail/Vector.ipp"
