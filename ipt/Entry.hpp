#pragma once

#ifndef IPT_BENCHMARK_CACHE_ENTRY_END
#define IPT_BENCHMARK_CACHE_ENTRY_END 0
#endif

#ifndef IPT_BENCHMARK_CACHE_ENTRY_LUB
#define IPT_BENCHMARK_CACHE_ENTRY_LUB 0
#endif

#include <compare>
#include <cstddef>
#include <ipt/Cuboid.hpp>
#include <ipt/Index.hpp>
#include <ipt/Point.hpp>

namespace ipt
{
  template<std::size_t D>
    struct IPT;

  template<std::size_t D>
    struct Entry
  {
    [[nodiscard]] constexpr Entry (Cuboid<D>, Index) noexcept;

    [[nodiscard]] constexpr auto merge_if_mergeable
      ( Entry const&
      ) noexcept -> bool
      ;

    [[nodiscard]] constexpr auto begin() const noexcept -> Index;
    [[nodiscard]] constexpr auto cuboid() const noexcept -> Cuboid<D> const&;
    [[nodiscard]] constexpr auto end() const noexcept -> Index;
    [[nodiscard]] constexpr auto lub() const noexcept -> Point<D>;
    [[nodiscard]] constexpr auto pos (Point<D>) const -> Index;
    [[nodiscard]] constexpr auto size() const noexcept -> std::size_t;

    [[nodiscard]] constexpr auto operator<=>
      ( Entry const&
      ) const noexcept = default
      ;

  private:
    template<std::size_t>
      friend struct IPT;

    [[nodiscard]] constexpr auto UNCHECKED_at
      ( Index
      ) const noexcept -> Point<D>
      ;

    Cuboid<D> _cuboid;
    Index _begin;
#if IPT_BENCHMARK_CACHE_ENTRY_END
    Index _end;
#endif
#if IPT_BENCHMARK_CACHE_ENTRY_LUB
    Point<D> _lub;
#endif

    [[nodiscard]] constexpr auto compute_end() const noexcept -> Index;
    [[nodiscard]] constexpr auto compute_lub() const noexcept -> Point<D>;
  };
}

#include "detail/Entry.ipp"
