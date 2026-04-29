#pragma once

#include <baseline/RowCSR/Concepts.hpp>
#include <baseline/RowCSR/Vector.hpp>
#include <concepts>
#include <cstddef>
#include <generator>
#include <ipt/Cuboid.hpp>
#include <ipt/Index.hpp>
#include <ipt/Point.hpp>
#include <optional>
#include <string_view>
#include <utility>

namespace ipt::baseline
{
  // CSR format: first K coordinates are the "row" key, remaining D-K are the
  // "column" values stored per row.  Row keys are sorted; column values
  // within each row are sorted.  This gives two-level binary search for
  // pos() and a single binary search for at().
  template< std::size_t D
          , std::size_t K
          , row_csr::Storage<D, K> S = row_csr::storage::Vector<D, K>
          >
    requires (K >= 1 && K < D)
  struct RowCSR
  {
    using RowKey = row_csr::RowKey<D, K>;
    using ColumnValue = row_csr::ColumnValue<D, K>;

    RowCSR() = default;

    template<typename... Args>
      requires std::constructible_from<S, Args&&...>
    [[nodiscard]] explicit RowCSR (std::in_place_t, Args&&...);

    static auto name() noexcept -> std::string_view;

    auto add (Point<D> const&) noexcept -> void
      requires std::same_as<S, row_csr::storage::Vector<D, K>>
      ;

    [[nodiscard]] auto build() && noexcept -> RowCSR
      requires std::same_as<S, row_csr::storage::Vector<D, K>>
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
    S _storage;
    std::optional<Point<D>> _last_added;
  };
}

#include "detail/RowCSR.ipp"
