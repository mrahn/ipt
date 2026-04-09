#pragma once

#include <compare>
#include <cstddef>
#include <ipt/Cuboid.hpp>
#include <ipt/Entry.hpp>
#include <ipt/Index.hpp>
#include <ipt/Point.hpp>
#include <ipt/SelectView.hpp>
#include <ipt/storage/Concepts.hpp>
#include <ipt/storage/Vector.hpp>
#include <optional>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

namespace ipt
{
  template<std::size_t D, Storage<D> Storage = storage::Vector<D>>
    struct IPT
  {

    // Construct an IPT from an explicit Storage instance.
    //
    [[nodiscard]] constexpr explicit IPT (Storage);

    // In-place constructor: forwards arguments to the Storage's
    // constructor. Required for storage backends that are neither
    // copyable nor movable (e.g. storage::MMap), so that an
    // IPT<D, MMap<D>> can still be created directly from a path.
    //
    template<typename... Args>
      requires std::constructible_from<Storage, Args&&...>
    [[nodiscard]] constexpr explicit IPT (std::in_place_t, Args&&...);

    // Convenience overload: build an IPT<D, storage::Vector<D>> from a
    // ready-made entry vector. Only available when the storage is the
    // default owning storage::Vector<D>.
    //
    [[nodiscard]] constexpr IPT (std::vector<Entry<D>>)
      requires std::same_as<Storage, storage::Vector<D>>
      ;

    // Build an IPT directly from a sequence of strictly lex-ascending,
    // non-overlapping cuboids. The cuboids are stored as-is, with running
    // index prefix sums computed during construction. No greedy merging
    // is performed; pass the result through GreedyPlusMerge if a canonical
    // decomposition is required. Only meaningful for the owning
    // storage::Vector<D> backend.
    //
    template<std::ranges::input_range R>
      requires std::same_as<Storage, storage::Vector<D>>
            && std::same_as<std::ranges::range_value_t<R>, Cuboid<D>>
    [[nodiscard]] constexpr IPT (std::from_range_t, R&&);

    [[nodiscard]] constexpr auto at (Index) const -> Point<D>;
    [[nodiscard]] constexpr auto pos (Point<D>) const -> Index;
    [[nodiscard]] constexpr auto try_pos
      ( Point<D>
      ) const -> std::optional<Index>
      ;
    [[nodiscard]] constexpr auto size() const noexcept -> Index;

    // Lazy selection view: yields the per-entry intersection cuboids of
    // *this with the query, in IPT lex order. The result is an
    // input_range of Cuboid<D>.
    //
    [[nodiscard]] constexpr auto select
      ( Cuboid<D>
      ) const noexcept -> SelectView<D>
      ;

    [[nodiscard]] constexpr auto operator==
      ( IPT const&
      ) const noexcept -> bool
      ;

    [[nodiscard]] constexpr auto bytes() const noexcept -> std::size_t;
    [[nodiscard]] constexpr auto number_of_entries
      (
      ) const noexcept -> std::size_t
      ;

    // Read-only access to the underlying entry array. Useful for
    // serialization (ipt::storage::write) and for tests; the returned
    // span is tied to the lifetime of *this.
    //
    [[nodiscard]] constexpr auto entries_view
      (
      ) const noexcept -> std::span<Entry<D> const>
      ;

  private:
    Storage _storage;
  };
}

#include "detail/IPT.ipp"
