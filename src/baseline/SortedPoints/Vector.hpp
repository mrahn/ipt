#pragma once

#include <cstddef>
#include <ipt/Point.hpp>
#include <span>
#include <vector>

namespace ipt::baseline::sorted_points::storage
{
  // In-memory storage: owns a std::vector<Point<D>>. Default
  // backend used by the live benchmark.
  template<std::size_t D>
    struct Vector
  {
    Vector() noexcept = default;

    [[nodiscard]] explicit Vector (std::vector<ipt::Point<D>>) noexcept;

    [[nodiscard]] auto points
      (
      ) const noexcept -> std::span<ipt::Point<D> const>
      ;

    auto push_back (ipt::Point<D> const&) -> void;

    [[nodiscard]] auto last() const noexcept -> ipt::Point<D> const&;

    [[nodiscard]] auto empty() const noexcept -> bool;

  private:
    std::vector<ipt::Point<D>> _points;
  };
}

#include "detail/Vector.ipp"
