#pragma once

#include <baseline/Grid/Grid.hpp>
#include <baseline/Roaring/Concepts.hpp>
#include <baseline/Roaring/Vector.hpp>
#include <concepts>
#include <cstddef>
#include <generator>
#include <ipt/Cuboid.hpp>
#include <ipt/Index.hpp>
#include <ipt/Point.hpp>
#include <optional>
#include <utility>

namespace ipt::baseline
{
  // External baseline: Roaring bitmap (vendored CRoaring v4.6.1, see
  // third_party/CRoaring/). Linearises points through the bounding Grid
  // exactly as DenseBitset and BlockBitmap do, and stores the resulting
  // 64-bit ids in a Roaring64Map. Selected as the mature open-source
  // compressed-bitmap baseline; uses the 64-bit map so that bounding
  // grids larger than 2^32 are supported.
  template< std::size_t D
          , ::ipt::baseline::roaring::Storage<D> S
              = ::ipt::baseline::roaring::storage::Vector<D>
          >
    struct Roaring
  {
    [[nodiscard]] explicit Roaring (Grid<D>) noexcept
      requires std::same_as<S, ::ipt::baseline::roaring::storage::Vector<D>>
      ;

    template<typename... Args>
      requires
        ( std::constructible_from<S, Args&&...>
       && !std::same_as<S, ::ipt::baseline::roaring::storage::Vector<D>>
        )
    [[nodiscard]] explicit Roaring (std::in_place_t, Args&&...);

    [[nodiscard]] static constexpr auto name() noexcept;

    auto add (Point<D> const&) noexcept -> void
      requires std::same_as<S, ::ipt::baseline::roaring::storage::Vector<D>>
      ;

    [[nodiscard]] auto build() && noexcept -> Roaring
      requires std::same_as<S, ::ipt::baseline::roaring::storage::Vector<D>>
      ;

    [[nodiscard]] auto size() const noexcept -> Index;
    [[nodiscard]] auto bytes() const noexcept -> std::size_t;

    [[nodiscard]] auto at (Index) const -> Point<D>;
    [[nodiscard]] auto pos (Point<D>) const -> Index;
    [[nodiscard]] auto try_pos
      ( Point<D>
      ) const noexcept -> std::optional<Index>
      ;

    [[nodiscard]] auto select
      ( Cuboid<D>
      ) const -> std::generator<Cuboid<D>>
      ;

    [[nodiscard]] auto storage() const noexcept -> S const&;

  private:
    [[nodiscard]] static auto make_layout
      ( Grid<D> const&
      ) noexcept -> grid::ReservedLayout<D>
      ;

    [[nodiscard]] static auto make_grid
      ( grid::ReservedLayout<D> const&
      ) noexcept -> Grid<D>
      ;

    S _storage;
    Grid<D> _grid;
    std::optional<Point<D>> _last_added;
  };
}

#include "detail/Roaring.ipp"
