#pragma once

#ifndef IPT_BENCHMARK_CACHE_RULER_SIZE
#define IPT_BENCHMARK_CACHE_RULER_SIZE 0
#endif

#include <compare>
#include <ipt/Coordinate.hpp>
#include <ipt/Index.hpp>
#include <optional>

namespace ipt
{
  template<std::size_t D>
    struct Cuboid;

  struct Ruler
  {
    // Pre: begin < end
    // Pre: stride > 0
    // Pre: begin >= 0 || end <= limit<Coordinate>::max() + begin
    // Pre: (end - begin) % stride == 0
    //
    [[nodiscard]] constexpr Ruler
      ( Coordinate begin
      , Coordinate stride
      , Coordinate end
      )
      ;

    struct Singleton
    {
      Coordinate coordinate;
    };
    [[nodiscard]] constexpr Ruler (Singleton) noexcept;

    [[nodiscard]] constexpr auto make_bounding_ruler
      ( Ruler const&
      ) const noexcept -> Ruler
      ;
    [[nodiscard]] constexpr auto pop_back
      (
      ) const noexcept -> Ruler
      ;

    [[nodiscard]] constexpr auto begin() const noexcept -> Coordinate;
    [[nodiscard]] constexpr auto contains (Coordinate) const noexcept -> bool;
    [[nodiscard]] constexpr auto end() const noexcept -> Coordinate;
    [[nodiscard]] constexpr auto intersect
      ( Ruler const&
      ) const noexcept -> std::optional<Ruler>
      ;
    [[nodiscard]] constexpr auto is_singleton() const noexcept -> bool;
    [[nodiscard]] constexpr auto lub() const noexcept -> Coordinate;
    [[nodiscard]] constexpr auto pos (Coordinate) const -> Index;
    [[nodiscard]] constexpr auto size() const noexcept -> Index;
    [[nodiscard]] constexpr auto stride() const noexcept -> Coordinate;

    [[nodiscard]] constexpr auto is_extended_by
      ( Ruler const&
      ) const noexcept -> bool
      ;
    constexpr auto extend_with (Ruler const&) noexcept -> void;

    [[nodiscard]] constexpr auto operator<=>
      ( Ruler const&
      ) const noexcept = default
      ;

  private:
    template<std::size_t>
      friend struct Cuboid;

    struct Checked
    {
      [[nodiscard]] constexpr Checked
        ( Coordinate begin
        , Coordinate stride
        , Coordinate end
        );

      Coordinate _begin;
      Coordinate _stride;
      Coordinate _end;
    };
    [[nodiscard]] constexpr Ruler (Checked) noexcept;

    struct UNCHECKED{};
    [[nodiscard]] constexpr Ruler
      ( UNCHECKED
      , Coordinate begin
      , Coordinate stride
      , Coordinate end
      ) noexcept
      ;

    [[nodiscard]] constexpr auto UNCHECKED_at
      ( Index
      ) const noexcept -> Coordinate
      ;

    [[nodiscard]] constexpr auto compute_size() const noexcept -> Index;

    Coordinate _begin;
    Coordinate _stride;
    Coordinate _end;
#if IPT_BENCHMARK_CACHE_RULER_SIZE
    Index _size;
#endif
  };
}

#include "detail/Ruler.ipp"
