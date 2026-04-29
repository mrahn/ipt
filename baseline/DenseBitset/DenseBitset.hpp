#pragma once

#include <baseline/DenseBitset/Concepts.hpp>
#include <baseline/DenseBitset/Vector.hpp>
#include <baseline/Grid/Grid.hpp>
#include <concepts>
#include <cstddef>
#include <generator>
#include <ipt/Cuboid.hpp>
#include <ipt/Index.hpp>
#include <ipt/Point.hpp>
#include <limits>
#include <optional>
#include <utility>

namespace ipt::baseline
{
  template< std::size_t D
          , dense_bitset::Storage<D> S = dense_bitset::storage::Vector<D>
          >
    struct DenseBitset
  {
    using Word = dense_bitset::Word;

    static constexpr auto bits_per_word
      {std::numeric_limits<Word>::digits};

    [[nodiscard]] explicit DenseBitset (Grid<D>)
      requires std::same_as<S, dense_bitset::storage::Vector<D>>
      ;

    template<typename... Args>
      requires
        ( std::constructible_from<S, Args&&...>
       && !std::same_as<S, dense_bitset::storage::Vector<D>>
        )
    [[nodiscard]] explicit DenseBitset (std::in_place_t, Args&&...);

    [[nodiscard]] static constexpr auto name() noexcept;

    auto add (Point<D> const&) noexcept -> void
      requires std::same_as<S, dense_bitset::storage::Vector<D>>
      ;

    [[nodiscard]] auto build() && noexcept -> DenseBitset;

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
    [[nodiscard]] static constexpr auto divru
      (Index, Index) noexcept -> Index;
    [[nodiscard]] static constexpr auto word (Index) noexcept -> std::size_t;
    [[nodiscard]] static constexpr auto bit (Index) noexcept -> unsigned;

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

#include "detail/DenseBitset.ipp"
