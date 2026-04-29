#pragma once

#include <baseline/BlockBitmap/Concepts.hpp>
#include <baseline/Grid/Header.hpp>
#include <cstddef>
#include <ipt/Index.hpp>
#include <span>
#include <vector>

namespace ipt::baseline::block_bitmap::storage
{
  template<std::size_t D>
    struct Vector
  {
    Vector() noexcept = default;

    [[nodiscard]] explicit Vector
      ( ipt::baseline::grid::ReservedLayout<D>
      , std::size_t
      )
      ;

    [[nodiscard]] auto tiles() const noexcept -> std::span<Word const>;
    [[nodiscard]] auto tiles_mutable() noexcept -> std::span<Word>;
    [[nodiscard]] auto point_count() const noexcept -> ipt::Index;
    auto bump_point_count() noexcept -> void;

    [[nodiscard]] auto layout
      (
      ) const noexcept -> ipt::baseline::grid::ReservedLayout<D> const&
      ;

  private:
    ipt::baseline::grid::ReservedLayout<D> _layout{};
    std::vector<Word> _tiles;
    ipt::Index _point_count {0};
  };
}

#include "detail/Vector.ipp"
