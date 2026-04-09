#pragma once

#include <baseline/SortedPoints/Concepts.hpp>
#include <baseline/SortedPoints/Vector.hpp>
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
  template< std::size_t D
          , sorted_points::Storage<D> S = sorted_points::storage::Vector<D>
          >
    struct SortedPoints
  {
    SortedPoints() = default;

    template<typename... Args>
      requires std::constructible_from<S, Args&&...>
    [[nodiscard]] explicit SortedPoints (std::in_place_t, Args&&...);

    auto add (Point<D> const&) -> void
      requires std::same_as<S, sorted_points::storage::Vector<D>>
      ;

    [[nodiscard]] auto build() && noexcept -> SortedPoints;

    [[nodiscard]] static constexpr auto name() noexcept;

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
    S _storage;
  };
}

#include "detail/SortedPoints.ipp"
