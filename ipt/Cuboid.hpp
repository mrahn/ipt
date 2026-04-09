#pragma once

#ifndef IPT_BENCHMARK_CACHE_CUBOID_SIZE
#define IPT_BENCHMARK_CACHE_CUBOID_SIZE 0
#endif

#include <array>
#include <compare>
#include <cstddef>
#include <ipt/Index.hpp>
#include <ipt/Point.hpp>
#include <ipt/Ruler.hpp>
#include <optional>

namespace ipt
{
  template<std::size_t D>
    struct Entry;

  template<std::size_t D>
    struct Cuboid
  {
    [[nodiscard]] constexpr Cuboid (std::array<Ruler, D>) noexcept;

    struct Singleton
    {
      Point<D> _point;
    };
    [[nodiscard]] constexpr Cuboid (Singleton) noexcept;

    [[nodiscard]] constexpr auto merge_if_mergeable
      ( Cuboid<D> const&
      ) noexcept -> bool
      ;

    [[nodiscard]] constexpr auto contains
      ( Point<D> const&
      ) const noexcept -> bool
      ;
    [[nodiscard]] constexpr auto at (Index) const -> Point<D>;
    [[nodiscard]] constexpr auto glb() const noexcept -> Point<D>;
    [[nodiscard]] constexpr auto intersect
      ( Cuboid<D> const&
      ) const noexcept -> std::optional<Cuboid<D>>
      ;
    [[nodiscard]] constexpr auto lub() const noexcept -> Point<D>;
    [[nodiscard]] constexpr auto pos (Point<D>) const -> Index;
    [[nodiscard]] constexpr auto try_pos
      ( Point<D>
      ) const noexcept -> std::optional<Index>
      ;
    [[nodiscard]] constexpr auto ruler
      ( std::size_t
      ) const noexcept -> Ruler const&
      ;
    [[nodiscard]] constexpr auto size() const noexcept -> Index;

    [[nodiscard]] constexpr auto operator<=>
      ( Cuboid const&
      ) const noexcept = default
      ;

  private:
    template<std::size_t>
      friend struct Entry;

    [[nodiscard]] constexpr auto UNCHECKED_at
      ( Index
      ) const noexcept -> Point<D>;

    std::array<Ruler, D> _rulers;

#if IPT_BENCHMARK_CACHE_CUBOID_SIZE
    Index _size;
#endif

    [[nodiscard]] constexpr auto compute_size() const noexcept -> Index;
  };
}

#include "detail/Cuboid.ipp"
