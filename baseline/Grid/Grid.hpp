#pragma once

#include <baseline/Extents.hpp>
#include <baseline/Origin.hpp>
#include <baseline/Strides.hpp>
#include <cstddef>
#include <generator>
#include <ipt/Cuboid.hpp>
#include <ipt/Index.hpp>
#include <ipt/Point.hpp>
#include <optional>

namespace ipt::baseline
{
  template<std::size_t D>
    struct Grid
  {
    [[nodiscard]] constexpr Grid (Origin<D>, Extents<D>, Strides<D>);

    [[nodiscard]] static constexpr auto name() noexcept;

    auto add (Point<D> const&) -> void;

    [[nodiscard]] auto build() && noexcept -> Grid;

    [[nodiscard]] constexpr auto origin() const noexcept -> Origin<D>;
    [[nodiscard]] constexpr auto extents() const noexcept -> Extents<D>;
    [[nodiscard]] constexpr auto strides() const noexcept -> Strides<D>;

    [[nodiscard]] constexpr auto size() const noexcept -> Index;
    [[nodiscard]] constexpr auto bytes() const noexcept -> std::size_t;

    [[nodiscard]] auto at (Index) const -> Point<D>;
    [[nodiscard]] auto pos (Point<D>) const -> Index;
    [[nodiscard]] auto try_pos
      ( Point<D>
      ) const noexcept -> std::optional<Index>
      ;
    [[nodiscard]] auto UNCHECKED_pos (Point<D>) const noexcept -> Index;

    [[nodiscard]] auto select
      ( Cuboid<D>
      ) const -> std::generator<Cuboid<D>>
      ;

  private:
    Origin<D> _origin;
    Extents<D> _extents;
    Strides<D> _strides;
  };

  // Convert a baseline Grid into the equivalent ipt::Cuboid covering
  // the same point set.
  template<std::size_t D>
    [[nodiscard]] auto to_ipt_cuboid (Grid<D> const&) -> Cuboid<D>;

  // Selection generator for Grid: at most one cuboid (the
  // intersection of the grid's intrinsic cuboid with the query).
  template<std::size_t D>
    [[nodiscard]] auto grid_select_impl
      ( Cuboid<D>
      , Cuboid<D>
      ) -> std::generator<Cuboid<D>>
      ;
}

#include "detail/Grid.ipp"
