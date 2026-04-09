#pragma once

#include <baseline/LexRun/Concepts.hpp>
#include <baseline/LexRun/Vector.hpp>
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
  // Run-length encoded baseline.  Points are stored as a vector of runs where
  // each run is a maximal arithmetic progression that agrees on the first D-1
  // coordinates and advances by a fixed stride in coordinate D-1 (the
  // fastest-varying dimension in lexicographic order).  Construction is O(n);
  // at and pos are O(log R) in the number of runs R.
  template< std::size_t D
          , lex_run::Storage<D> S = lex_run::storage::Vector<D>
          >
    struct LexRun
  {
    using Run = lex_run::Run<D>;

    LexRun() = default;

    template<typename... Args>
      requires std::constructible_from<S, Args&&...>
    [[nodiscard]] explicit LexRun (std::in_place_t, Args&&...);

    [[nodiscard]] static constexpr auto name() noexcept;

    auto add (Point<D> const&) noexcept -> void
      requires std::same_as<S, lex_run::storage::Vector<D>>
      ;

    [[nodiscard]] auto build() && noexcept -> LexRun;

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
    [[nodiscard]] static auto make_run_cuboid
      ( Run const&
      ) -> Cuboid<D>
      ;

    S _storage;
    std::optional<Point<D>> _last_added;
  };
}

#include "detail/LexRun.ipp"
