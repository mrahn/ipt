#include <algorithm>
#include <array>
#include <baseline/detail/select.hpp>
#include <baseline/enumerate.hpp>
#include <bit>
#include <cassert>
#include <ipt/Coordinate.hpp>
#include <iterator>
#include <limits>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <utility>

namespace ipt::baseline
{
  template<std::size_t D, dense_bitset::Storage<D> S>
    DenseBitset<D, S>::DenseBitset (Grid<D> grid)
      requires std::same_as<S, dense_bitset::storage::Vector<D>>
        : _storage
          { make_layout (grid)
          , static_cast<std::size_t>
              (divru (grid.size(), static_cast<Index> (bits_per_word)))
          }
        , _grid {grid}
  {}

  template<std::size_t D, dense_bitset::Storage<D> S>
    template<typename... Args>
      requires
        ( std::constructible_from<S, Args&&...>
       && !std::same_as<S, dense_bitset::storage::Vector<D>>
        )
    DenseBitset<D, S>::DenseBitset (std::in_place_t, Args&&... args)
      : _storage {std::forward<Args> (args)...}
      , _grid {make_grid (_storage.layout())}
  {}

  template<std::size_t D, dense_bitset::Storage<D> S>
    constexpr auto DenseBitset<D, S>::name() noexcept
  {
    return "dense-bitset";
  }

  template<std::size_t D, dense_bitset::Storage<D> S>
    auto DenseBitset<D, S>::add (Point<D> const& point) noexcept -> void
      requires std::same_as<S, dense_bitset::storage::Vector<D>>
  {
    assert (_storage.point_count() < _grid.size());
    assert (!_last_added || *_last_added < point);

    auto const linear_index {_grid.UNCHECKED_pos (point)};

    auto const word_index {word (linear_index)};
    auto const bit_index {bit (linear_index)};
    auto const mask {Word {1} << bit_index};

    auto bits {_storage.bits_mutable()};
    assert (word_index < bits.size());
    assert ((bits[word_index] & mask) == 0);

    bits[word_index] |= mask;
    _last_added = point;
    _storage.bump_point_count();
  }

  template<std::size_t D, dense_bitset::Storage<D> S>
    auto DenseBitset<D, S>::build() && noexcept -> DenseBitset
  {
    return std::move (*this);
  }

  template<std::size_t D, dense_bitset::Storage<D> S>
    auto DenseBitset<D, S>::size() const noexcept -> Index
  {
    return _storage.point_count();
  }

  template<std::size_t D, dense_bitset::Storage<D> S>
    auto DenseBitset<D, S>::bytes() const noexcept -> std::size_t
  {
    return sizeof (*this) + _storage.bits().size_bytes();
  }

  template<std::size_t D, dense_bitset::Storage<D> S>
    auto DenseBitset<D, S>::at (Index index) const -> Point<D>
  {
    auto const bits {_storage.bits()};

    if (!(index < size()))
    {
      throw std::out_of_range
        {"DenseBitset::at: index out of range"};
    }

    auto remaining {index};
    auto const word_it
      { std::ranges::find_if
        ( bits
        , [&remaining] (Word w) noexcept
          {
            auto const count {static_cast<Index> (std::popcount (w))};

            if (remaining < count)
            {
              return true;
            }

            remaining -= count;
            return false;
          }
        )
      };

    assert (word_it != std::ranges::end (bits));

    auto w {*word_it};

    std::ranges::for_each
      ( std::views::iota (Index {0}, remaining)
      , [&w] (auto) noexcept
        {
          w &= w - 1;
        }
      );

    auto const linear_index
      { static_cast<Index>
        ( std::ranges::distance (std::ranges::begin (bits), word_it)
        ) * static_cast<Index> (bits_per_word)
        + static_cast<Index> (std::countr_zero (w))
      };

    auto const origin {_grid.origin()};
    auto const extents {_grid.extents()};
    auto const strides {_grid.strides()};
    auto coordinates {std::array<Coordinate, D>{}};

    std::ranges::transform
      ( extents | std::views::reverse
      , std::rbegin (coordinates)
      , [&, remaining_index = linear_index, d = D]
          (Coordinate extent) mutable noexcept
        {
          auto const current_d {--d};
          auto const coordinate_index
            { static_cast<Coordinate>
              (remaining_index % static_cast<Index> (extent))
            };

          remaining_index /= static_cast<Index> (extent);

          return origin[current_d]
            + coordinate_index * strides[current_d]
            ;
        }
      );

    return Point<D> {coordinates};
  }

  template<std::size_t D, dense_bitset::Storage<D> S>
    auto DenseBitset<D, S>::pos (Point<D> point) const -> Index
  {
    auto const result {try_pos (point)};

    if (!result)
    {
      throw std::out_of_range
        {"DenseBitset::pos: point out of range"};
    }

    return *result;
  }

  template<std::size_t D, dense_bitset::Storage<D> S>
    auto DenseBitset<D, S>::try_pos
      ( Point<D> point
      ) const noexcept -> std::optional<Index>
  {
    auto const bits {_storage.bits()};
    auto const linear_opt {_grid.try_pos (point)};

    if (!linear_opt)
    {
      return std::nullopt;
    }

    auto const linear_index {*linear_opt};
    auto const word_index {word (linear_index)};
    auto const bit_index {bit (linear_index)};
    auto const mask {Word {1} << bit_index};

    if (word_index >= bits.size() || (bits[word_index] & mask) == 0)
    {
      return std::nullopt;
    }

    auto index {Index {0}};

    std::ranges::for_each
      ( std::views::counted (std::ranges::begin (bits), word_index)
      , [&] (Word w) noexcept
        {
          index += static_cast<Index> (std::popcount (w));
        }
      );

    auto const lower_bits
      { bit_index == 0
          ? Word {0}
          : (Word {1} << bit_index) - 1
      };

    index += static_cast<Index>
      (std::popcount (bits[word_index] & lower_bits));

    return index;
  }

  template<std::size_t D, dense_bitset::Storage<D> S>
    auto DenseBitset<D, S>::restrict
      ( Cuboid<D> query
      ) const -> std::optional<DenseBitset<D>>
  {
    auto const bits {_storage.bits()};
    auto const shape_statistics {detail::query_shape_statistics (_grid, query)};

    if (!shape_statistics)
    {
      return std::nullopt;
    }

    auto const& statistics {*shape_statistics};
    auto restricted
      {DenseBitset<D> {detail::grid_from_cuboid (statistics.intersection)}};
    auto const use_pointwise_probe
      { statistics.point_count <= Index {128}
     || statistics.points_per_interval
          < static_cast<Index> (bits_per_word / 2)
      };

    if (use_pointwise_probe)
    {
      for (auto const& point : enumerate (statistics.intersection))
      {
        auto const linear {_grid.UNCHECKED_pos (point)};
        auto const word_index {word (linear)};
        auto const bit_index {bit (linear)};

        if ((bits[word_index] & (Word {1} << bit_index)) != 0)
        {
          restricted.add (point);
        }
      }
    }
    else
    {
      auto const& last_ruler {statistics.intersection.ruler (D - 1)};
      auto const last_grid_stride {_grid.strides()[D - 1]};

      assert (Coordinate {0} < last_grid_stride);
      assert (last_ruler.stride() % last_grid_stride == Coordinate {0});

      auto const last_linear_step
        { last_ruler.size() == Index {1}
            ? Index {1}
            : static_cast<Index> (last_ruler.stride() / last_grid_stride)
        };

      for ( auto const& [first, last]
          : detail::query_linear_intervals (_grid, statistics.intersection)
          )
      {
        if (last_linear_step == Index {1})
        {
          auto const first_word {word (first)};
          auto const last_word {word (last)};

          for (auto word_index {first_word}; word_index <= last_word; ++word_index)
          {
            auto masked {bits[word_index]};

            if (word_index == first_word)
            {
              auto const first_bit {bit (first)};
              auto const lower_mask
                { first_bit == 0
                    ? Word {0}
                    : (Word {1} << first_bit) - 1
                };
              masked &= ~lower_mask;
            }

            if (word_index == last_word)
            {
              auto const last_bit {bit (last)};
              auto const upper_mask
                { last_bit + 1 == bits_per_word
                    ? std::numeric_limits<Word>::max()
                    : (Word {1} << (last_bit + 1)) - 1
                };
              masked &= upper_mask;
            }

            while (masked != 0)
            {
              auto const bit_offset
                {static_cast<Index> (std::countr_zero (masked))};
              auto const linear
                { static_cast<Index> (word_index)
                  * static_cast<Index> (bits_per_word)
                  + bit_offset
                };

              restricted.add (_grid.at (linear));
              masked &= masked - 1;
            }
          }

          continue;
        }

        for (auto position {Index {0}}; position < last_ruler.size(); ++position)
        {
          auto const linear {first + position * last_linear_step};
          auto const word_index {word (linear)};
          auto const bit_index {bit (linear)};

          if ((bits[word_index] & (Word {1} << bit_index)) != 0)
          {
            restricted.add (_grid.at (linear));
          }
        }
      }
    }

    if (restricted.size() == Index {0})
    {
      return std::nullopt;
    }

    return std::move (restricted).build();
  }

  template<std::size_t D, dense_bitset::Storage<D> S>
    auto DenseBitset<D, S>::storage() const noexcept -> S const&
  {
    return _storage;
  }

  template<std::size_t D, dense_bitset::Storage<D> S>
    constexpr auto DenseBitset<D, S>::divru
      ( Index a
      , Index b
      ) noexcept -> Index
  {
    return (a + b - 1) / b;
  }

  template<std::size_t D, dense_bitset::Storage<D> S>
    constexpr auto DenseBitset<D, S>::word
      ( Index index
      ) noexcept -> std::size_t
  {
    return static_cast<std::size_t>
      (index / static_cast<Index> (bits_per_word));
  }

  template<std::size_t D, dense_bitset::Storage<D> S>
    constexpr auto DenseBitset<D, S>::bit
      ( Index index
      ) noexcept -> unsigned
  {
    return static_cast<unsigned>
      (index % static_cast<Index> (bits_per_word));
  }

  template<std::size_t D, dense_bitset::Storage<D> S>
    auto DenseBitset<D, S>::make_layout
      ( Grid<D> const& g
      ) noexcept -> grid::ReservedLayout<D>
  {
    return grid::make_reserved_layout<D> (g);
  }

  template<std::size_t D, dense_bitset::Storage<D> S>
    auto DenseBitset<D, S>::make_grid
      ( grid::ReservedLayout<D> const& layout
      ) noexcept -> Grid<D>
  {
    return Grid<D>
      { Origin<D>  {layout.origin}
      , Extents<D> {layout.extents}
      , Strides<D> {layout.strides}
      };
  }
}
