#include <algorithm>
#include <array>
#include <baseline/enumerate.hpp>
#include <cassert>
#include <format>
#include <functional>
#include <ipt/Coordinate.hpp>
#include <iterator>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <utility>

namespace ipt::baseline
{
  template<std::size_t D, std::size_t K, row_csr::Storage<D, K> S>
    requires (K >= 1 && K < D)
  template<typename... Args>
    requires std::constructible_from<S, Args&&...>
  RowCSR<D, K, S>::RowCSR (std::in_place_t, Args&&... args)
    : _storage {std::forward<Args> (args)...}
  {}

  template<std::size_t D, std::size_t K, row_csr::Storage<D, K> S>
    requires (K >= 1 && K < D)
  auto RowCSR<D, K, S>::name() noexcept -> std::string_view
  {
    static auto const _name {std::format ("row-csr-k{}", K)};

    return _name;
  }

  template<std::size_t D, std::size_t K, row_csr::Storage<D, K> S>
    requires (K >= 1 && K < D)
  auto RowCSR<D, K, S>::add (Point<D> const& point) noexcept -> void
    requires std::same_as<S, row_csr::storage::Vector<D, K>>
  {
    assert (!_last_added || *_last_added < point);

    auto row_key {RowKey{}};

    std::ranges::transform
      ( std::views::iota (std::size_t {0}, K)
      , std::ranges::begin (row_key)
      , [&] (auto d) noexcept
        {
          return point[d];
        }
      );

    auto column_value {ColumnValue{}};

    std::ranges::transform
      ( std::views::iota (std::size_t {0}, D - K)
      , std::ranges::begin (column_value)
      , [&] (auto d) noexcept
        {
          return point[K + d];
        }
      );

    if (_storage.row_keys_empty() || _storage.last_row_key() != row_key)
    {
      _storage.push_row_offset
        (static_cast<Index> (_storage.column_values().size()));
      _storage.push_row_key (row_key);
    }

    _storage.push_column_value (column_value);
    _last_added = point;
  }

  template<std::size_t D, std::size_t K, row_csr::Storage<D, K> S>
    requires (K >= 1 && K < D)
  auto RowCSR<D, K, S>::build() && noexcept -> RowCSR
    requires std::same_as<S, row_csr::storage::Vector<D, K>>
  {
    _storage.push_row_offset
      (static_cast<Index> (_storage.column_values().size()));
    return std::move (*this);
  }

  template<std::size_t D, std::size_t K, row_csr::Storage<D, K> S>
    requires (K >= 1 && K < D)
  auto RowCSR<D, K, S>::size() const noexcept -> Index
  {
    return static_cast<Index> (_storage.column_values().size());
  }

  template<std::size_t D, std::size_t K, row_csr::Storage<D, K> S>
    requires (K >= 1 && K < D)
  auto RowCSR<D, K, S>::bytes() const noexcept -> std::size_t
  {
    return sizeof (*this)
      + _storage.row_keys().size_bytes()
      + _storage.row_offsets().size_bytes()
      + _storage.column_values().size_bytes();
  }

  template<std::size_t D, std::size_t K, row_csr::Storage<D, K> S>
    requires (K >= 1 && K < D)
  auto RowCSR<D, K, S>::at (Index index) const -> Point<D>
  {
    auto const row_keys {_storage.row_keys()};
    auto const row_offsets {_storage.row_offsets()};
    auto const column_values {_storage.column_values()};

    if (!(index < size()))
    {
      throw std::out_of_range {"RowCSR::at: index out of range"};
    }

    // Find last row_offsets entry that is <= index.
    auto const row_offset_upper_bound
      { std::ranges::upper_bound (row_offsets, index)
      };
    auto const row
      { static_cast<std::size_t>
          ( std::distance
              ( std::cbegin (row_offsets)
              , std::prev (row_offset_upper_bound)
              )
          )
      };

    auto coordinates {std::array<Coordinate, D>{}};

    std::ranges::copy (row_keys[row], std::ranges::begin (coordinates));
    std::ranges::copy
      ( column_values[index]
      , std::ranges::begin (coordinates) + K
      );

    return Point<D> {coordinates};
  }

  template<std::size_t D, std::size_t K, row_csr::Storage<D, K> S>
    requires (K >= 1 && K < D)
  auto RowCSR<D, K, S>::pos (Point<D> point) const -> Index
  {
    auto const row_keys {_storage.row_keys()};
    auto const row_offsets {_storage.row_offsets()};
    auto const column_values {_storage.column_values()};

    auto row_key {RowKey{}};

    std::ranges::transform
      ( std::views::iota (std::size_t {0}, K)
      , std::ranges::begin (row_key)
      , [&] (auto d) noexcept
        {
          return point[d];
        }
      );

    auto column_value {ColumnValue{}};

    std::ranges::transform
      ( std::views::iota (std::size_t {0}, D - K)
      , std::ranges::begin (column_value)
      , [&] (auto d) noexcept
        {
          return point[K + d];
        }
      );

    // Locate row.
    auto const row_index
      { std::invoke
        ( [&]() -> std::size_t
          {
            auto const row_iterator
              {std::ranges::lower_bound (row_keys, row_key)};

            if ( row_iterator == std::cend (row_keys)
              || *row_iterator != row_key
               )
            {
              throw std::out_of_range
                {"RowCSR::pos: point out of range"};
            }

            return std::distance (std::cbegin (row_keys), row_iterator);
          }
        )
      };
    auto const row_begin {row_offsets[row_index]};
    auto const row_end {row_offsets[row_index + 1]};

    // Locate column within row.
    auto const row_column_values
      { std::ranges::subrange
        ( std::cbegin (column_values) + row_begin
        , std::cbegin (column_values) + row_end
        )
      };
    auto const column_iterator
      { std::ranges::lower_bound (row_column_values, column_value)
      };
    if (  column_iterator == std::end (row_column_values)
       || *column_iterator != column_value
       )
    {
      throw std::out_of_range {"RowCSR::pos: point out of range"};
    }

    return static_cast<Index>
      (std::distance (std::cbegin (column_values), column_iterator));
  }

  template<std::size_t D, std::size_t K, row_csr::Storage<D, K> S>
    requires (K >= 1 && K < D)
  auto RowCSR<D, K, S>::try_pos
    ( Point<D> point
    ) const noexcept -> std::optional<Index>
  {
    auto const row_keys {_storage.row_keys()};
    auto const row_offsets {_storage.row_offsets()};
    auto const column_values {_storage.column_values()};

    auto row_key {RowKey{}};

    std::ranges::transform
      ( std::views::iota (std::size_t {0}, K)
      , std::ranges::begin (row_key)
      , [&] (auto d) noexcept
        {
          return point[d];
        }
      );

    auto column_value {ColumnValue{}};

    std::ranges::transform
      ( std::views::iota (std::size_t {0}, D - K)
      , std::ranges::begin (column_value)
      , [&] (auto d) noexcept
        {
          return point[K + d];
        }
      );

    auto const row_iterator
      {std::ranges::lower_bound (row_keys, row_key)};

    if ( row_iterator == std::cend (row_keys)
      || *row_iterator != row_key
       )
    {
      return std::nullopt;
    }

    auto const row_index
      { static_cast<std::size_t>
          (std::distance (std::cbegin (row_keys), row_iterator))
      };
    auto const row_begin {row_offsets[row_index]};
    auto const row_end {row_offsets[row_index + 1]};

    auto const row_column_values
      { std::ranges::subrange
        ( std::cbegin (column_values) + row_begin
        , std::cbegin (column_values) + row_end
        )
      };
    auto const column_iterator
      { std::ranges::lower_bound (row_column_values, column_value)
      };

    if (  column_iterator == std::end (row_column_values)
       || *column_iterator != column_value
       )
    {
      return std::nullopt;
    }

    return static_cast<Index>
      (std::distance (std::cbegin (column_values), column_iterator));
  }

  template<std::size_t D, std::size_t K, row_csr::Storage<D, K> S>
    requires (K >= 1 && K < D)
  auto RowCSR<D, K, S>::select
    ( Cuboid<D> query
    ) const -> std::generator<Cuboid<D>>
  {
    auto const row_keys {_storage.row_keys()};
    auto const row_offsets {_storage.row_offsets()};
    auto const column_values {_storage.column_values()};

    // Walk all rows whose key lies in the query's row prefix; for each
    // row, scan its column entries and yield those falling in the
    // query's column suffix.
    for (auto row {std::size_t {0}}; row < row_keys.size(); ++row)
    {
      auto const& row_key {row_keys[row]};

      auto in_prefix {true};

      for (auto d {std::size_t {0}}; d < K && in_prefix; ++d)
      {
        if (!query.ruler (d).contains (row_key[d]))
        {
          in_prefix = false;
        }
      }

      if (!in_prefix)
      {
        continue;
      }

      auto const row_begin {row_offsets[row]};
      auto const row_end {row_offsets[row + 1]};

      for (auto i {row_begin}; i < row_end; ++i)
      {
        auto const& column_value
          { column_values[static_cast<std::size_t> (i)]
          };

        auto in_suffix {true};

        for (auto d {std::size_t {0}}; d < D - K && in_suffix; ++d)
        {
          if (!query.ruler (K + d).contains (column_value[d]))
          {
            in_suffix = false;
          }
        }

        if (!in_suffix)
        {
          continue;
        }

        auto coordinates {std::array<Coordinate, D>{}};

        for (auto d {std::size_t {0}}; d < K; ++d)
        {
          coordinates[d] = row_key[d];
        }

        for (auto d {std::size_t {0}}; d < D - K; ++d)
        {
          coordinates[K + d] = column_value[d];
        }

        co_yield singleton_cuboid (Point<D> {coordinates});
      }
    }
  }

  template<std::size_t D, std::size_t K, row_csr::Storage<D, K> S>
    requires (K >= 1 && K < D)
  auto RowCSR<D, K, S>::storage() const noexcept -> S const&
  {
    return _storage;
  }
}
