#pragma once

#include <compare>
#include <cstddef>
#include <ipt/Cuboid.hpp>
#include <ipt/Entry.hpp>
#include <ipt/Index.hpp>
#include <ipt/Point.hpp>
#include <ipt/SelectView.hpp>
#include <ranges>
#include <vector>

namespace ipt
{
  template<std::size_t D>
    struct IPT
  {
    [[nodiscard]] constexpr IPT (std::vector<Entry<D>>);

    // Build an IPT directly from a sequence of strictly lex-ascending,
    // non-overlapping cuboids. The cuboids are stored as-is, with running
    // index prefix sums computed during construction. No greedy merging is
    // performed; pass the result through GreedyPlusMerge if a canonical
    // decomposition is required.
    //
    template<std::ranges::input_range R>
      requires std::same_as<std::ranges::range_value_t<R>, Cuboid<D>>
    [[nodiscard]] constexpr IPT (std::from_range_t, R&&);

    [[nodiscard]] constexpr auto at (Index) const -> Point<D>;
    [[nodiscard]] constexpr auto pos (Point<D>) const -> Index;
    [[nodiscard]] constexpr auto size() const noexcept -> Index;

    // Lazy selection view: yields the per-entry intersection cuboids of
    // *this with the query, in IPT lex order. The result is an
    // input_range of Cuboid<D>.
    //
    [[nodiscard]] constexpr auto select
      ( Cuboid<D>
      ) const noexcept -> SelectView<D>
      ;

    [[nodiscard]] constexpr auto operator<=>
      ( IPT const&
      ) const noexcept = default
      ;

    [[nodiscard]] constexpr auto bytes() const noexcept -> std::size_t;
    [[nodiscard]] constexpr auto number_of_entries
      (
      ) const noexcept -> std::size_t
      ;

  private:
    std::vector<Entry<D>> _entries;
  };
}

#include "detail/IPT.ipp"
