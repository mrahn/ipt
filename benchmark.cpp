
#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <format>
#include <functional>
#include <generator>
#include <ipt/Coordinate.hpp>
#include <ipt/Cuboid.hpp>
#include <ipt/Entry.hpp>
#include <ipt/IPT.hpp>
#include <ipt/Index.hpp>
#include <ipt/Point.hpp>
#include <ipt/Ruler.hpp>
#include <ipt/create/GreedyPlusCombine.hpp>
#include <ipt/create/GreedyPlusMerge.hpp>
#include <ipt/create/Regular.hpp>
#include <iterator>
#include <numeric>
#include <optional>
#include <print>
#include <random>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string_view>
#include <third_party/CRoaring/roaring.hh>
#include <tuple>
#include <utility>
#include <vector>

namespace
{
  using namespace ipt;

  // Lazily enumerate the points of a cuboid in IPT lex order.
  // The cuboid is taken by value and moved into the view so the
  // returned range stays valid even when called on a temporary.
  template<std::size_t D>
    [[nodiscard]] constexpr auto enumerate (Cuboid<D> cuboid) noexcept
  {
    auto const total {cuboid.size()};

    return std::views::iota (Index {0}, total)
         | std::views::transform
           ( [cuboid {std::move (cuboid)}] (auto index)
             {
               return cuboid.at (index);
             }
           );
  }

  template<typename T, std::size_t D>
    [[nodiscard]] constexpr auto zeros() noexcept -> std::array<T, D>
  {
    auto zeros {std::array<T, D>{}};
    std::ranges::fill (zeros, T {0});
    return zeros;
  }
  template<typename T, std::size_t D>
    [[nodiscard]] constexpr auto ones() noexcept -> std::array<T, D>
  {
    auto ones {std::array<T, D>{}};
    std::ranges::fill (ones, T {1});
    return ones;
  }

  struct BenchmarkSelection
  {
    std::vector<std::string_view> scenarios;
    std::vector<std::string_view> algorithms;
    std::optional<std::size_t> seed_count;
    std::optional<std::size_t> all_query_cap;
  };

  [[nodiscard]] constexpr auto trim_ascii_whitespace
    ( std::string_view text
    ) noexcept -> std::string_view
  {
    auto const first {text.find_first_not_of (" \f\n\r\t\v")};

    if (first == std::string_view::npos)
    {
      return {};
    }

    auto const last {text.find_last_not_of (" \f\n\r\t\v")};

    return text.substr (first, last - first + 1);
  }

  [[nodiscard]] auto split_environment_filter
    ( char const* text
    ) -> std::vector<std::string_view>
  {
    if (text == nullptr || *text == '\0')
    {
      return {};
    }

    auto remaining {std::string_view {text}};
    auto values {std::vector<std::string_view> {}};

    while (true)
    {
      auto const comma {remaining.find (',')};
      auto const token {trim_ascii_whitespace (remaining.substr (0, comma))};
      if (!token.empty())
      {
        values.push_back (token);
      }

      if (comma == std::string_view::npos)
      {
        break;
      }

      remaining.remove_prefix (comma + 1);
    }

    return values;
  }

  [[nodiscard]] auto parse_environment_seed_count
    ( char const* text
    ) -> std::optional<std::size_t>
  {
    if (text == nullptr || *text == '\0')
    {
      return std::nullopt;
    }

    char* end {nullptr};
    auto const value {std::strtoull (text, &end, 10)};
    if (end == text || *end != '\0' || value == 0)
    {
      throw std::invalid_argument
        {"IPT_BENCHMARK_SEED_COUNT must be a positive integer"};
    }

    return static_cast<std::size_t> (value);
  }

  [[nodiscard]] auto parse_environment_all_query_cap
    ( char const* text
    ) -> std::optional<std::size_t>
  {
    if (text == nullptr || *text == '\0')
    {
      return std::nullopt;
    }

    char* end {nullptr};
    auto const value {std::strtoull (text, &end, 10)};
    if (end == text || *end != '\0')
    {
      throw std::invalid_argument
        {"IPT_BENCHMARK_ALL_QUERY_CAP must be a non-negative integer"};
    }

    return static_cast<std::size_t> (value);
  }

  [[nodiscard]] auto benchmark_selection() -> BenchmarkSelection const&
  {
    static auto const selection
      { BenchmarkSelection
        { .scenarios
            { split_environment_filter
                (std::getenv ("IPT_BENCHMARK_ONLY_SCENARIOS"))
            }
        , .algorithms
            { split_environment_filter
                (std::getenv ("IPT_BENCHMARK_ONLY_ALGORITHMS"))
            }
        , .seed_count
            { parse_environment_seed_count
                (std::getenv ("IPT_BENCHMARK_SEED_COUNT"))
            }
        , .all_query_cap
            { parse_environment_all_query_cap
                (std::getenv ("IPT_BENCHMARK_ALL_QUERY_CAP"))
            }
        }
      };

    return selection;
  }

  [[nodiscard]] constexpr auto default_all_query_cap() noexcept -> std::size_t
  {
    return std::size_t {100'000};
  }

  [[nodiscard]] auto benchmark_all_query_cap() noexcept -> std::size_t
  {
    auto const configured_cap
      { benchmark_selection().all_query_cap.value_or (default_all_query_cap())
      };

    if (configured_cap == 0)
    {
      return std::numeric_limits<std::size_t>::max();
    }

    return configured_cap;
  }

  [[nodiscard]] auto selection_matches
    ( std::span<std::string_view const> selected_values
    , std::string_view candidate
    ) noexcept -> bool
  {
    return selected_values.empty()
        || std::ranges::find (selected_values, candidate) != std::end (selected_values)
        ;
  }

  [[nodiscard]] auto selected_scenario
    ( std::string_view candidate
    ) noexcept -> bool
  {
    return selection_matches (benchmark_selection().scenarios, candidate);
  }

  [[nodiscard]] auto selected_algorithm
    ( std::string_view candidate
    ) noexcept -> bool
  {
    return selection_matches (benchmark_selection().algorithms, candidate);
  }

  template<typename T>
    [[nodiscard]] auto evenly_spaced_sample
      ( std::span<T const> items
      , std::size_t max_count
      ) -> std::vector<T>
  {
    auto const sample_count {std::min (items.size(), max_count)};
    if (sample_count == 0)
    {
      return {};
    }

    if (sample_count == items.size())
    {
      return std::vector<T> {std::ranges::begin (items), std::ranges::end (items)};
    }

    auto result {std::vector<T> {}};
    result.reserve (sample_count);

    std::generate_n
      ( std::back_inserter (result)
      , sample_count
      , [&, sample_index = std::size_t {0}]() mutable
        {
          auto const item_index
            { sample_count == 1
                ? std::size_t {0}
                : sample_index * (items.size() - 1) / (sample_count - 1)
            };
          sample_index += 1;
          return items[item_index];
        }
      );

    return result;
  }

  [[nodiscard]] auto evenly_spaced_index_sample
    ( std::size_t size
    , std::size_t max_count
    ) -> std::vector<Index>
  {
    auto const sample_count {std::min (size, max_count)};
    if (sample_count == 0)
    {
      return {};
    }

    auto result {std::vector<Index> {}};
    result.reserve (sample_count);

    std::generate_n
      ( std::back_inserter (result)
      , sample_count
      , [&, sample_index = std::size_t {0}] () mutable noexcept
        {
          auto const item_index
            { sample_count == 1
                ? std::size_t {0}
                : sample_index * (size - 1) / (sample_count - 1)
            };
          sample_index += 1;
          return static_cast<Index> (item_index);
        }
      );

    return result;
  }

  template<std::size_t D>
    struct Extents
  {
    [[nodiscard]] explicit constexpr Extents
      ( std::array<Coordinate, D> extents
      )
        : _extents {extents}
    {
      assert (std::ranges::all_of (_extents, [] (auto n) { return n > 0; }));
    }

    template<typename... Coordinates>
      requires (  sizeof... (Coordinates) == D
               && (std::convertible_to<Coordinate, Coordinates> && ...)
               )
    [[nodiscard]] explicit constexpr Extents
      ( Coordinates... ns
      )
        : Extents {std::array<Coordinate, D> {ns...}}
    {}

    [[nodiscard]] constexpr auto operator[] (Index index) const noexcept -> Coordinate
    {
      assert (index < D);

      return _extents[index];
    }

    [[nodiscard]] constexpr auto size() const noexcept -> Coordinate
    {
      return std::ranges::fold_left
        ( _extents
        , Coordinate {1}
        , std::multiplies<Coordinate>{}
        );
    }

    [[nodiscard]] constexpr auto begin() const noexcept
    {
      return std::begin (_extents);
    }
    [[nodiscard]] constexpr auto end() const noexcept
    {
      return std::end (_extents);
    }
    [[nodiscard]] constexpr auto rbegin() const noexcept
    {
      return std::rbegin (_extents);
    }
    [[nodiscard]] constexpr auto rend() const noexcept
    {
      return std::rend (_extents);
    }

  private:
    std::array<Coordinate, D> _extents;
  };

  template<std::size_t D>
    struct Strides
  {
    [[nodiscard]] explicit constexpr Strides
      ( std::array<Coordinate, D> strides
      )
        : _strides {strides}
    {
      assert (std::ranges::all_of (_strides, [] (auto s) { return s > 0; }));
    }

    template<typename... Coordinates>
      requires (  sizeof... (Coordinates) == D
               && (std::convertible_to<Coordinate, Coordinates> && ...)
               )
    [[nodiscard]] explicit constexpr Strides
      ( Coordinates... ss
      )
        : Strides {std::array<Coordinate, D> {ss...}}
    {}

    [[nodiscard]] constexpr auto operator[] (Index index) const noexcept -> Coordinate
    {
      assert (index < D);

      return _strides[index];
    }

  private:
    std::array<Coordinate, D> _strides;
  };

  template<std::size_t D>
    struct Origin
  {
    [[nodiscard]] explicit constexpr Origin
      ( std::array<Coordinate, D> origin
      ) noexcept
        : _origin {origin}
    {}

    template<typename... Coordinates>
      requires (  sizeof... (Coordinates) == D
               && (std::convertible_to<Coordinate, Coordinates> && ...)
               )
    [[nodiscard]] explicit constexpr Origin
      ( Coordinates... os
      ) noexcept
        : Origin {std::array<Coordinate, D> {os...}}
    {}

    [[nodiscard]] constexpr auto operator[] (Index index) const noexcept -> Coordinate
    {
      assert (index < D);

      return _origin[index];
    }

  private:
    std::array<Coordinate, D> _origin;
  };

  template<std::size_t D>
    [[nodiscard]] constexpr auto make_cuboid
      ( Origin<D> origin
      , Extents<D> extents
      , Strides<D> strides
      ) -> Cuboid<D>
  {
    return Cuboid<D>
      { std::invoke
        ( [&]<std::size_t... Dimensions>
            ( std::index_sequence<Dimensions...>
            )
          {
            return std::array<Ruler, D>
              { Ruler
                { origin[Dimensions]
                , strides[Dimensions]
                , extents[Dimensions]
                }...
              }
              ;
          }
        , std::make_index_sequence<D> {}
        )
      };
  }
}

namespace
{
  using namespace ipt;

  template<std::size_t D>
    struct Grid
  {
    [[nodiscard]] constexpr Grid
      ( Origin<D> origin
      , Extents<D> extents
      , Strides<D> strides
      )
        : _origin {origin}
        , _extents {extents}
        , _strides {strides}
    {}

    [[nodiscard]] static constexpr auto name() noexcept
    {
      return "grid";
    }

    auto add (Point<D> const&) -> void
    {
      throw std::logic_error {"Grid::add: Grid must be constructed directly"};
    }

    [[nodiscard]] auto build() && noexcept -> Grid
    {
      return std::move (*this);
    }

    [[nodiscard]] constexpr auto origin() const noexcept -> Origin<D>
    {
      return _origin;
    }
    [[nodiscard]] constexpr auto extents() const noexcept -> Extents<D>
    {
      return _extents;
    }
    [[nodiscard]] constexpr auto strides() const noexcept -> Strides<D>
    {
      return _strides;
    }

    [[nodiscard]] constexpr auto size() const noexcept -> Index
    {
      return static_cast<Index> (_extents.size());
    }

    [[nodiscard]] constexpr auto bytes() const noexcept -> std::size_t
    {
      return sizeof (Grid);
    }

    [[nodiscard]] auto at (Index index) const -> Point<D>
    {
      if (!(index < size()))
      {
        throw std::out_of_range {"Grid::at: index out of range"};
      }

      auto coordinates {std::array<Coordinate, D>{}};

      std::ranges::transform
        ( _extents | std::views::reverse
        , std::rbegin (coordinates)
        , [&, remaining = index, d = D] (auto extent) mutable
          {
            auto const current_d {--d};
            auto const i_extent {static_cast<Index> (extent)};
            auto const coordinate_index
              {static_cast<Coordinate> (remaining % i_extent)};

            remaining /= i_extent;

            return _origin[current_d]
              + coordinate_index * _strides[current_d]
              ;
          }
        );

      return Point<D> {coordinates};
    }

    [[nodiscard]] auto pos (Point<D> point) const -> Index
    {
      std::ranges::for_each
        ( _extents
        , [&, d {std::size_t {0}}] (auto extent) mutable
          {
            auto const coordinate {point[d]};
            auto const origin {_origin[d]};
            auto const stride {_strides[d++]};

            if (coordinate < origin)
            {
              throw std::out_of_range {"Grid::pos: point out of range"};
            }

            if ((coordinate - origin) % stride != 0)
            {
              throw std::out_of_range {"Grid::pos: point out of range"};
            }

            auto const offset {(coordinate - origin) / stride};

            if (offset < 0 || offset >= extent)
            {
              throw std::out_of_range {"Grid::pos: point out of range"};
            }
          }
        );

      return UNCHECKED_pos (point);
    }

    [[nodiscard]] auto UNCHECKED_pos (Point<D> point) const noexcept -> Index
    {
      return std::ranges::fold_left
        ( _extents
        , Index {0}
        , [&, d {std::size_t {0}}]
            ( Index index
            , Coordinate extent
            ) mutable noexcept
          {
            auto const coordinate {point[d]};
            auto const origin {_origin[d]};
            auto const stride {_strides[d++]};
            auto const offset {(coordinate - origin) / stride};

            return static_cast<Index> (offset)
              + index * static_cast<Index> (extent)
              ;
          }
        );
    }

    [[nodiscard]] auto select
      ( ipt::Cuboid<D> query
      ) const -> std::generator<ipt::Cuboid<D>>;

  private:
    Origin<D> _origin;
    Extents<D> _extents;
    Strides<D> _strides;
  };

  // Convert a benchmark Grid into the equivalent ipt::Cuboid covering the
  // same point set.
  template<std::size_t D>
    [[nodiscard]] auto to_ipt_cuboid (Grid<D> const& grid) -> ipt::Cuboid<D>
  {
    auto rulers
      { std::invoke
        ( [&]<std::size_t... Dimensions>
            ( std::index_sequence<Dimensions...>
            )
          {
            return std::array<ipt::Ruler, D>
              { ipt::Ruler
                { grid.origin()[Dimensions]
                , grid.strides()[Dimensions]
                , grid.origin()[Dimensions]
                  + static_cast<Coordinate> (grid.extents()[Dimensions])
                  * grid.strides()[Dimensions]
                }...
              };
          }
        , std::make_index_sequence<D> {}
        )
      };

    return ipt::Cuboid<D> {std::move (rulers)};
  }

  // Selection generator for Grid: at most one cuboid (the intersection of
  // the grid's intrinsic cuboid with the query).
  template<std::size_t D>
    [[nodiscard]] auto grid_select_impl
      ( ipt::Cuboid<D> grid_cuboid
      , ipt::Cuboid<D> query
      ) -> std::generator<ipt::Cuboid<D>>
  {
    if (auto inter {grid_cuboid.intersect (query)})
    {
      co_yield std::move (*inter);
    }
  }

  template<std::size_t D>
    auto Grid<D>::select
      ( ipt::Cuboid<D> query
      ) const -> std::generator<ipt::Cuboid<D>>
  {
    return grid_select_impl<D> (to_ipt_cuboid (*this), std::move (query));
  }

  template<std::size_t D>
    [[nodiscard]] auto singleton_cuboid
      ( Point<D> const& point
      ) -> ipt::Cuboid<D>
  {
    return ipt::Cuboid<D> {typename ipt::Cuboid<D>::Singleton {point}};
  }

  template<std::size_t Columns, std::size_t Rows>
    [[nodiscard]] constexpr auto make_rectangular_three_d_survey_grids
      ( std::array<Coordinate, Columns> const& x_origins
      , std::array<Coordinate, Rows> const& y_origins
      , std::array<Coordinate, Columns> const& x_extents
      , std::array<Coordinate, Rows> const& y_extents
      , std::array<Coordinate, Columns> const& z_extents
      , std::array<Coordinate, Columns> const& x_strides
      , std::array<Coordinate, Rows> const& y_strides
      , std::array<Coordinate, Rows> const& z_strides
      ) noexcept -> std::array<Grid<3>, Columns * Rows>
  {
    return std::invoke
      ( [&]<std::size_t... Indices>
          ( std::index_sequence<Indices...>
          ) noexcept
        {
          return std::array<Grid<3>, Columns * Rows>
            { Grid<3>
              { Origin<3>
                { x_origins[Indices % Columns]
                , y_origins[Indices / Columns]
                , Coordinate {0}
                }
              , Extents<3>
                { x_extents[Indices % Columns]
                , y_extents[Indices / Columns]
                , z_extents[Indices % Columns]
                }
              , Strides<3>
                { x_strides[Indices % Columns]
                , y_strides[Indices / Columns]
                , z_strides[Indices / Columns]
                }
              }...
            };
        }
      , std::make_index_sequence<Columns * Rows> {}
      );
  }
}

namespace
{
  using namespace ipt;

  template<std::size_t D>
    struct SortedPoints
  {
    auto add (Point<D> const& point) -> void
    {
      assert (_points.empty() || _points.back() < point);

      _points.push_back (point);
    }

    [[nodiscard]] auto build() && noexcept -> SortedPoints
    {
      return std::move (*this);
    }

    [[nodiscard]] static constexpr auto name() noexcept
    {
      return "sorted-points";
    }

    [[nodiscard]] auto size() const noexcept -> Index
    {
      return _points.size();
    }

    [[nodiscard]] auto bytes() const noexcept -> std::size_t
    {
      return _points.size() * sizeof (Point<D>);
    }

    [[nodiscard]] auto at (Index index) const -> Point<D>
    {
      if (!(index < size()))
      {
        throw std::out_of_range {"SortedPoints::at: index out of range"};
      }

      return _points[index];
    }

    [[nodiscard]] auto pos (Point<D> point) const -> Index
    {
      auto const lb {std::ranges::lower_bound (_points, point)};

      if (lb == std::ranges::end (_points) || *lb != point)
      {
        throw std::out_of_range {"SortedPoints::pos: point out of range"};
      }

      return std::ranges::distance (std::ranges::begin (_points), lb);
    }

    [[nodiscard]] auto select
      ( ipt::Cuboid<D> query
      ) const -> std::generator<ipt::Cuboid<D>>
    {
      // Cuboid::lub() is the inclusive lex-maximum (last contained
      // point), so the iteration range is [lower_bound(glb),
      // upper_bound(lub)).
      auto const lo {std::ranges::lower_bound (_points, query.glb())};
      auto const hi {std::ranges::upper_bound (_points, query.lub())};

      for (auto point {lo}; point != hi; ++point)
      {
        if (query.contains (*point))
        {
          co_yield singleton_cuboid (*point);
        }
      }
    }

  private:
    std::vector<Point<D>> _points;
  };
}

namespace
{
  using namespace ipt;

  template<std::size_t D>
    struct DenseBitset
  {
    [[nodiscard]] explicit DenseBitset (Grid<D> grid)
      : _grid {grid}
      , _bits
        ( static_cast<std::size_t>
            (divru (grid.size(), static_cast<Index> (bits_per_word)))
        , Word {0}
        )
    {}

    [[nodiscard]] static constexpr auto name() noexcept
    {
      return "dense-bitset";
    }

    auto add (Point<D> const& point) noexcept -> void
    {
      assert (_point_count < _grid.size());
      assert (!_last_added || *_last_added < point);

      auto const linear_index {_grid.UNCHECKED_pos (point)};

      auto const word_index {word (linear_index)};
      auto const bit_index {bit (linear_index)};
      auto const mask {Word {1} << bit_index};

      assert (word_index < _bits.size());
      assert ((_bits[word_index] & mask) == 0);

      _bits[word_index] |= mask;
      _last_added = point;
      _point_count += 1;
    }

    [[nodiscard]] auto build() && noexcept -> DenseBitset
    {
      return std::move (*this);
    }

    [[nodiscard]] auto size() const noexcept -> Index
    {
      return _point_count;
    }

    [[nodiscard]] auto bytes() const noexcept -> std::size_t
    {
      return sizeof (*this) + _bits.size() * sizeof (Word);
    }

    [[nodiscard]] auto at (Index index) const -> Point<D>
    {
      if (!(index < size()))
      {
        throw std::out_of_range {"DenseBitset::at: index out of range"};
      }

      auto remaining {index};
      auto const word_it
        { std::ranges::find_if
          ( _bits
          , [&remaining] (Word word) noexcept
            {
              auto const count {static_cast<Index> (std::popcount (word))};

              if (remaining < count)
              {
                return true;
              }

              remaining -= count;
              return false;
            }
          )
        };

      assert (word_it != std::ranges::end (_bits));

      auto word {*word_it};

      std::ranges::for_each
        ( std::views::iota (Index {0}, remaining)
        , [&word] (auto) noexcept
          {
            word &= word - 1;
          }
        );

      auto const linear_index
        { static_cast<Index>
          ( std::ranges::distance (std::ranges::begin (_bits), word_it)
          ) * static_cast<Index> (bits_per_word)
          + static_cast<Index> (std::countr_zero (word))
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

    [[nodiscard]] auto pos (Point<D> point) const -> Index
    {
      auto const linear_index {_grid.pos (point)};

      auto const word_index {word (linear_index)};
      auto const bit_index {bit (linear_index)};
      auto const mask {Word {1} << bit_index};

      if (word_index >= _bits.size() || (_bits[word_index] & mask) == 0)
      {
        throw std::out_of_range {"DenseBitset::pos: point out of range"};
      }

      auto index {Index {0}};

      std::ranges::for_each
        ( std::views::counted (std::ranges::begin (_bits), word_index)
        , [&] (Word word) noexcept
          {
            index += static_cast<Index> (std::popcount (word));
          }
        );

      auto const lower_bits
        { bit_index == 0
            ? Word {0}
            : (Word {1} << bit_index) - 1
        };

      index += static_cast<Index> (std::popcount (_bits[word_index] & lower_bits));

      return index;
    }

    [[nodiscard]] auto select
      ( ipt::Cuboid<D> query
      ) const -> std::generator<ipt::Cuboid<D>>
    {
      auto const grid_cuboid {to_ipt_cuboid (_grid)};
      auto const intersection {grid_cuboid.intersect (query)};

      if (!intersection)
      {
        co_return;
      }

      for (auto const& point : enumerate (*intersection))
      {
        auto const linear {_grid.UNCHECKED_pos (point)};
        auto const word_i {word (linear)};
        auto const bit_i {bit (linear)};

        if ((_bits[word_i] & (Word {1} << bit_i)) != 0)
        {
          co_yield singleton_cuboid (point);
        }
      }
    }

  private:
    using Word = std::uint64_t;

    static constexpr auto bits_per_word {std::numeric_limits<Word>::digits};

    [[nodiscard]] static constexpr auto divru
      ( Index a
      , Index b
      ) noexcept -> Index
    {
      return (a + b - 1) / b;
    }

    [[nodiscard]] static constexpr auto word (Index index) noexcept -> std::size_t
    {
      return static_cast<std::size_t>
        (index / static_cast<Index> (bits_per_word));
    }

    [[nodiscard]] static constexpr auto bit (Index index) noexcept -> unsigned
    {
      return static_cast<unsigned>
        (index % static_cast<Index> (bits_per_word));
    }

    Grid<D> _grid;
    std::vector<Word> _bits;
    Index _point_count {0};
    std::optional<Point<D>> _last_added;
  };
}

namespace
{
  using namespace ipt;

  // Tile-based bitmap baseline.  The bounding grid is divided into
  // axis-aligned tiles of tile_extent grid points per dimension, where
  // tile_extent is the largest power of two such that tile_extent^D ≤ 64.
  // Every tile is stored as one uint64_t word; bits within a tile are laid
  // out in row-major order.  Memory per point equals 1 bit, same as
  // DenseBitset, but the tile granularity gives better spatial locality for
  // D-dimensional access patterns.
  template<std::size_t D>
    struct BlockBitmap
  {
    // Largest power of 2 such that tile_extent^D fits in one 64-bit word.
    // D=1: 64, D=2: 8, D=3: 4, D=6: 2, D>6: 1.
    static constexpr Index tile_extent
      { [] () constexpr noexcept -> Index
        {
          auto e {Index {64}};
          while (true)
          {
            auto vol {Index {1}};
            for (std::size_t k {0}; k < D; ++k)
            {
              vol *= e;
            }

            if (vol <= 64)
            {
              return e;
            }

            e /= 2;
          }
        } ()
      };

    static constexpr Index tile_volume
      { [] () constexpr noexcept -> Index
        {
          auto vol {Index {1}};
          for (std::size_t k {0}; k < D; ++k)
          {
            vol *= tile_extent;
          }
          return vol;
        } ()
      };

    [[nodiscard]] explicit BlockBitmap (Grid<D> grid)
      : _grid {grid}
      , _tile_counts
        { std::invoke
          ( [&grid] () noexcept
            {
              auto counts {std::array<Index, D>{}};

              std::ranges::transform
                ( std::views::iota (std::size_t {0}, D)
                , std::ranges::begin (counts)
                , [&grid] (auto d) noexcept
                  {
                    return ( static_cast<Index> (grid.extents()[d])
                             + tile_extent
                             - 1
                           ) / tile_extent;
                  }
                );

              return counts;
            }
          )
        }
      , _tiles
        ( static_cast<std::size_t>
          ( std::ranges::fold_left
            ( _tile_counts
            , Index {1}
            , std::multiplies<Index> {}
            )
          )
        , Word {0}
        )
    {}

    [[nodiscard]] static constexpr auto name() noexcept
    {
      return "block-bitmap";
    }

    auto add (Point<D> const& point) noexcept -> void
    {
      assert (_point_count < _grid.size());
      assert (!_last_added || *_last_added < point);

      auto tile_coords {std::array<Index, D>{}};
      auto local_coords {std::array<Index, D>{}};

      std::ranges::for_each
        ( std::views::iota (std::size_t {0}, D)
        , [&] (auto d) noexcept
          {
            auto const coord {point[d]};
            auto const origin {_grid.origin()[d]};
            auto const stride {_grid.strides()[d]};

            assert (coord >= origin);
            assert ((coord - origin) % stride == 0);

            auto const grid_coordinate
              { static_cast<Index> ((coord - origin) / stride)
              };

            assert (grid_coordinate < static_cast<Index> (_grid.extents()[d]));

            tile_coords[d] = grid_coordinate / tile_extent;
            local_coords[d] = grid_coordinate % tile_extent;
          }
        );

      auto const tile_index
        { std::ranges::fold_left
          ( std::views::iota (std::size_t {0}, D)
          , Index {0}
          , [&] (auto index, auto d) noexcept
            {
              return index * _tile_counts[d] + tile_coords[d];
            }
          )
        };

      auto const bit_index
        { std::ranges::fold_left
          ( std::views::iota (std::size_t {0}, D)
          , Index {0}
          , [&] (auto index, auto d) noexcept
            {
              return index * tile_extent + local_coords[d];
            }
          )
        };

      assert (tile_index < static_cast<Index> (_tiles.size()));
      assert (bit_index < tile_volume);
      assert ((_tiles[static_cast<std::size_t> (tile_index)]
               & (Word {1} << static_cast<unsigned> (bit_index))) == 0);

      _tiles[static_cast<std::size_t> (tile_index)]
        |= Word {1} << static_cast<unsigned> (bit_index);
      _last_added = point;
      _point_count += 1;
    }

    [[nodiscard]] auto build() && noexcept -> BlockBitmap
    {
      return std::move (*this);
    }

    [[nodiscard]] auto size() const noexcept -> Index
    {
      return _point_count;
    }

    [[nodiscard]] auto bytes() const noexcept -> std::size_t
    {
      return sizeof (*this) + _tiles.size() * sizeof (Word);
    }

    [[nodiscard]] auto at (Index index) const -> Point<D>
    {
      if (!(index < size()))
      {
        throw std::out_of_range {"BlockBitmap::at: index out of range"};
      }

      auto remaining {index};
      auto const flat_tile_it
        { std::ranges::find_if
          ( _tiles
          , [&remaining] (Word tile) noexcept
            {
              auto const count {static_cast<Index> (std::popcount (tile))};

              if (remaining < count)
              {
                return true;
              }

              remaining -= count;
              return false;
            }
          )
        };

      assert (flat_tile_it != std::ranges::end (_tiles));

      auto const flat_tile
        { static_cast<std::size_t>
          (std::ranges::distance (std::ranges::begin (_tiles), flat_tile_it))
        };

      // Find the remaining-th set bit within the tile.
      auto word {*flat_tile_it};

      std::ranges::for_each
        ( std::views::iota (Index {0}, remaining)
        , [&word] (auto) noexcept
          {
            word &= word - 1;
          }
        );

      auto const bit_index
        {static_cast<Index> (std::countr_zero (word))};

      // Recover tile coordinates (reverse row-major).
      auto tile_coords {std::array<Index, D>{}};

      std::ranges::transform
        ( _tile_counts | std::views::reverse
        , std::rbegin (tile_coords)
        , [remaining = static_cast<Index> (flat_tile)]
            ( auto tile_count
            ) mutable noexcept
          {
            auto const tile_coord {remaining % tile_count};
            remaining /= tile_count;
            return tile_coord;
          }
        );

      // Recover within-tile coordinates (reverse row-major).
      auto local_coords {std::array<Index, D>{}};

      std::ranges::transform
        ( std::views::iota (std::size_t {0}, D)
        , std::rbegin (local_coords)
        , [remaining = bit_index] (auto) mutable noexcept
          {
            auto const local_coord {remaining % tile_extent};
            remaining /= tile_extent;
            return local_coord;
          }
        );

      auto coordinates {std::array<Coordinate, D>{}};

      std::ranges::transform
        ( std::views::iota (std::size_t {0}, D)
        , std::ranges::begin (coordinates)
        , [&] (auto d) noexcept
          {
            auto const grid_coordinate
              {tile_coords[d] * tile_extent + local_coords[d]};

            return _grid.origin()[d]
              + static_cast<Coordinate> (grid_coordinate) * _grid.strides()[d]
              ;
          }
        );

      return Point<D> {coordinates};
    }

    [[nodiscard]] auto pos (Point<D> point) const -> Index
    {
      auto tile_coords {std::array<Index, D>{}};
      auto local_coords {std::array<Index, D>{}};

      std::ranges::for_each
        ( std::views::iota (std::size_t {0}, D)
        , [&] (auto d)
          {
            auto const coord {point[d]};
            auto const origin {_grid.origin()[d]};
            auto const stride {_grid.strides()[d]};

            if (coord < origin)
            {
              throw std::out_of_range {"BlockBitmap::pos: point out of range"};
            }

            if ((coord - origin) % stride != 0)
            {
              throw std::out_of_range {"BlockBitmap::pos: point out of range"};
            }

            auto const grid_coordinate
              {static_cast<Index> ((coord - origin) / stride)};

            if (!(grid_coordinate < static_cast<Index> (_grid.extents()[d])))
            {
              throw std::out_of_range {"BlockBitmap::pos: point out of range"};
            }

            tile_coords[d] = grid_coordinate / tile_extent;
            local_coords[d] = grid_coordinate % tile_extent;
          }
        );

      auto const tile_index
        { std::ranges::fold_left
          ( std::views::iota (std::size_t {0}, D)
          , Index {0}
          , [&] (auto index, auto d) noexcept
            {
              return index * _tile_counts[d] + tile_coords[d];
            }
          )
        };

      auto const bit_index
        { std::ranges::fold_left
          ( std::views::iota (std::size_t {0}, D)
          , Index {0}
          , [&] (auto index, auto d) noexcept
            {
              return index * tile_extent + local_coords[d];
            }
          )
        };

      auto const tile_i {static_cast<std::size_t> (tile_index)};

      if (!(tile_i < _tiles.size())
          || (_tiles[tile_i] & (Word {1} << static_cast<unsigned> (bit_index))) == 0)
      {
        throw std::out_of_range {"BlockBitmap::pos: point out of range"};
      }

      auto index {Index {0}};

      std::ranges::for_each
        ( std::views::counted (std::ranges::begin (_tiles), tile_i)
        , [&] (Word tile) noexcept
          {
            index += static_cast<Index> (std::popcount (tile));
          }
        );

      auto const lower_mask
        { bit_index == 0
            ? Word {0}
            : (Word {1} << static_cast<unsigned> (bit_index)) - 1
        };
      index += static_cast<Index>
        (std::popcount (_tiles[tile_i] & lower_mask));

      return index;
    }

    [[nodiscard]] auto select
      ( ipt::Cuboid<D> query
      ) const -> std::generator<ipt::Cuboid<D>>
    {
      auto const grid_cuboid {to_ipt_cuboid (_grid)};
      auto const intersection {grid_cuboid.intersect (query)};

      if (!intersection)
      {
        co_return;
      }

      for (auto const& point : enumerate (*intersection))
      {
        // The point is guaranteed in-grid because it came from
        // intersecting with the grid's own cuboid.
        auto tile_coords {std::array<Index, D>{}};
        auto local_coords {std::array<Index, D>{}};

        for (auto d {std::size_t {0}}; d < D; ++d)
        {
          auto const grid_coordinate
            { static_cast<Index>
              ((point[d] - _grid.origin()[d]) / _grid.strides()[d])
            };

          tile_coords[d] = grid_coordinate / tile_extent;
          local_coords[d] = grid_coordinate % tile_extent;
        }

        auto tile_index {Index {0}};

        for (auto d {std::size_t {0}}; d < D; ++d)
        {
          tile_index = tile_index * _tile_counts[d] + tile_coords[d];
        }

        auto bit_index {Index {0}};

        for (auto d {std::size_t {0}}; d < D; ++d)
        {
          bit_index = bit_index * tile_extent + local_coords[d];
        }

        auto const tile_i {static_cast<std::size_t> (tile_index)};
        auto const mask {Word {1} << static_cast<unsigned> (bit_index)};

        if ((_tiles[tile_i] & mask) != 0)
        {
          co_yield singleton_cuboid (point);
        }
      }
    }

  private:
    using Word = std::uint64_t;

    Grid<D> _grid;
    std::array<Index, D> _tile_counts;
    std::vector<Word> _tiles;
    Index _point_count {0};
    std::optional<Point<D>> _last_added;
  };
}

namespace
{
  using namespace ipt;

  // Run-length encoded baseline.  Points are stored as a vector of runs where
  // each run is a maximal arithmetic progression that agrees on the first D-1
  // coordinates and advances by a fixed stride in coordinate D-1 (the
  // fastest-varying dimension in lexicographic order).  Construction is O(n);
  // at and pos are O(log R) in the number of runs R.
  template<std::size_t D>
    struct LexRun
  {
    [[nodiscard]] static constexpr auto name() noexcept
    {
      return "lex-run";
    }

    auto add (Point<D> const& point) noexcept -> void
    {
      assert (!_last_added || *_last_added < point);

      if (!_runs.empty())
      {
        auto& back {_runs.back()};

        // Check whether the prefix (all but the last coordinate) matches.
        auto const prefix_matches
          { std::ranges::all_of
            ( std::views::iota (std::size_t {0}, D - 1)
            , [&] (auto d) noexcept
              {
                return back.start[d] == point[d];
              }
            )
          };

        if (prefix_matches)
        {
          if (back.length == 1)
          {
            // Determine stride from the first two points.
            back.stride = point[D - 1] - back.start[D - 1];
            back.length = 2;
            _last_added = point;
            _total_points += 1;
            return;
          }

          // Extend an existing run if the point continues the progression.
          if ( point[D - 1]
             == back.start[D - 1]
                + static_cast<Coordinate> (back.length) * back.stride)
          {
            back.length += 1;
            _last_added = point;
            _total_points += 1;
            return;
          }
        }
      }

      // Start a new run.
      _runs.push_back (Run {point, Coordinate {0}, Index {1}, _total_points});
      _last_added = point;
      _total_points += 1;
    }

    [[nodiscard]] auto build() && noexcept -> LexRun
    {
      return std::move (*this);
    }

    [[nodiscard]] auto size() const noexcept -> Index
    {
      return _total_points;
    }

    [[nodiscard]] auto bytes() const noexcept -> std::size_t
    {
      return sizeof (*this) + _runs.size() * sizeof (Run);
    }

    [[nodiscard]] auto at (Index index) const -> Point<D>
    {
      if (!(index < _total_points))
      {
        throw std::out_of_range {"LexRun::at: index out of range"};
      }

      // Find the last run whose begin index is <= index.
      auto const run_upper_bound
        { std::ranges::upper_bound (_runs, index, {}, &Run::begin)
        };

      auto const& run {*std::prev (run_upper_bound)};
      auto const offset {index - run.begin};

      auto coordinates {std::array<Coordinate, D>{}};

      std::ranges::transform
        ( std::views::iota (std::size_t {0}, D - 1)
        , std::ranges::begin (coordinates)
        , [&] (auto d) noexcept
          {
            return run.start[d];
          }
        );

      coordinates[D - 1] = run.start[D - 1]
        + static_cast<Coordinate> (offset) * run.stride;

      return Point<D> {coordinates};
    }

    [[nodiscard]] auto pos (Point<D> point) const -> Index
    {
      // Find the last run whose start is <= point (lex order).
      auto const run_upper_bound
        { std::ranges::upper_bound (_runs, point, {}, &Run::start)
        };

      if (run_upper_bound == std::cbegin (_runs))
      {
        throw std::out_of_range {"LexRun::pos: point out of range"};
      }

      auto const& run {*std::prev (run_upper_bound)};

      if ( ! std::ranges::all_of
             ( std::views::iota (std::size_t {0}, D - 1)
             , [&] (auto d) noexcept
               {
                 return run.start[d] == point[d];
               }
             )
         )
      {
        throw std::out_of_range {"LexRun::pos: point out of range"};
      }

      auto const last_diff {point[D - 1] - run.start[D - 1]};

      if (last_diff < 0)
      {
        throw std::out_of_range {"LexRun::pos: point out of range"};
      }

      if (run.length == 1)
      {
        if (last_diff != 0)
        {
          throw std::out_of_range {"LexRun::pos: point out of range"};
        }

        return run.begin;
      }

      if (last_diff % run.stride != 0)
      {
        throw std::out_of_range {"LexRun::pos: point out of range"};
      }

      auto const offset {static_cast<Index> (last_diff / run.stride)};

      if (!(offset < run.length))
      {
        throw std::out_of_range {"LexRun::pos: point out of range"};
      }

      return run.begin + offset;
    }

    [[nodiscard]] auto select
      ( ipt::Cuboid<D> query
      ) const -> std::generator<ipt::Cuboid<D>>
    {
      for (auto const& run : _runs)
      {
        auto const run_cuboid {make_run_cuboid (run)};

        if (auto inter {run_cuboid.intersect (query)})
        {
          co_yield std::move (*inter);
        }
      }
    }

  private:
    struct Run
    {
      Point<D> start;
      Coordinate stride; // step in coord D-1; 0 if length == 1
      Index length;
      Index begin; // cumulative point count preceding this run
    };

    [[nodiscard]] static auto make_run_cuboid
      ( Run const& run
      ) -> ipt::Cuboid<D>
    {
      auto last_ruler
        { run.length == 1
            ? ipt::Ruler {typename ipt::Ruler::Singleton {run.start[D - 1]}}
            : ipt::Ruler
              { run.start[D - 1]
              , run.stride
              , run.start[D - 1]
                + static_cast<Coordinate> (run.length) * run.stride
              }
        };

      auto rulers
        { std::invoke
          ( [&]<std::size_t... Dimensions>
              ( std::index_sequence<Dimensions...>
              )
            {
              return std::array<ipt::Ruler, D>
                { ipt::Ruler
                    {typename ipt::Ruler::Singleton {run.start[Dimensions]}}...
                , std::move (last_ruler)
                };
            }
          , std::make_index_sequence<D - 1> {}
          )
        };

      return ipt::Cuboid<D> {std::move (rulers)};
    }

    std::vector<Run> _runs;
    Index _total_points {0};
    std::optional<Point<D>> _last_added;
  };
}

namespace
{
  using namespace ipt;

  // CSR format: first K coordinates are the "row" key, remaining D-K are the
  // "column" values stored per row.  Row keys are sorted; column values within
  // each row are sorted.  This gives two-level binary search for pos() and a
  // single binary search for at().
  template<std::size_t D, std::size_t K>
    requires (K >= 1 && K < D)
  struct RowCSR
  {
    static auto name() noexcept -> std::string_view
    {
      static auto const _name {std::format ("row-csr-k{}", K)};

      return _name;
    }

    auto add (Point<D> const& point) noexcept -> void
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

      if (_row_keys.empty() || _row_keys.back() != row_key)
      {
        _row_offsets.push_back (static_cast<Index> (_column_values.size()));
        _row_keys.push_back (row_key);
      }

      _column_values.push_back (column_value);
      _last_added = point;
    }

    [[nodiscard]] auto build() && noexcept -> RowCSR
    {
      _row_offsets.push_back (static_cast<Index> (_column_values.size()));
      return std::move (*this);
    }

    [[nodiscard]] auto size() const noexcept -> Index
    {
      return static_cast<Index> (_column_values.size());
    }

    [[nodiscard]] auto bytes() const noexcept -> std::size_t
    {
      return sizeof (*this)
        + _row_keys.size() * sizeof (RowKey)
        + _row_offsets.size() * sizeof (Index)
        + _column_values.size() * sizeof (ColumnValue);
    }

    [[nodiscard]] auto at (Index index) const -> Point<D>
    {
      if (!(index < size()))
      {
        throw std::out_of_range {"RowCSR::at: index out of range"};
      }

      // Find last row_offsets entry that is <= index.
      auto const row_offset_upper_bound
        { std::ranges::upper_bound (_row_offsets, index)
        };
      auto const row
        { static_cast<std::size_t>
            (std::distance (std::cbegin (_row_offsets), std::prev (row_offset_upper_bound)))
        };

      auto coordinates {std::array<Coordinate, D> {}};

      std::ranges::copy (_row_keys[row], std::ranges::begin (coordinates));
      std::ranges::copy
        ( _column_values[index]
        , std::ranges::begin (coordinates) + K
        );

      return Point<D> {coordinates};
    }

    [[nodiscard]] auto pos (Point<D> point) const -> Index
    {
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
              auto const row_iterator {std::ranges::lower_bound (_row_keys, row_key)};

              if (row_iterator == std::cend (_row_keys) || *row_iterator != row_key)
              {
                throw std::out_of_range {"RowCSR::pos: point out of range"};
              }

              return std::distance (std::cbegin (_row_keys), row_iterator);
            }
          )
        };
      auto const row_begin {_row_offsets[row_index]};
      auto const row_end {_row_offsets[row_index + 1]};

      // Locate column within row.
      auto const row_column_values
        { std::ranges::subrange
          ( std::cbegin (_column_values) + row_begin
          , std::cbegin (_column_values) + row_end
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
        (std::distance (std::cbegin (_column_values), column_iterator));
    }

    [[nodiscard]] auto select
      ( ipt::Cuboid<D> query
      ) const -> std::generator<ipt::Cuboid<D>>
    {
      // Walk all rows whose key lies in the query's row prefix; for each
      // row, scan its column entries and yield those falling in the
      // query's column suffix.
      for (auto row {std::size_t {0}}; row < _row_keys.size(); ++row)
      {
        auto const& row_key {_row_keys[row]};

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

        auto const row_begin {_row_offsets[row]};
        auto const row_end {_row_offsets[row + 1]};

        for (auto i {row_begin}; i < row_end; ++i)
        {
          auto const& column_value
            { _column_values[static_cast<std::size_t> (i)]
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

          auto coordinates {std::array<Coordinate, D> {}};

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

  private:
    using RowKey = std::array<Coordinate, K>;
    using ColumnValue = std::array<Coordinate, D - K>;

    std::vector<RowKey> _row_keys;
    std::vector<Index> _row_offsets;
    std::vector<ColumnValue> _column_values;
    std::optional<Point<D>> _last_added;
  };
}

namespace
{
  using namespace ipt;

  // External baseline: Roaring bitmap (vendored CRoaring v4.6.1, see
  // third_party/CRoaring/). Linearises points through the bounding Grid
  // exactly as DenseBitset and BlockBitmap do, and stores the resulting
  // 64-bit ids in a Roaring64Map. Selected as the mature open-source
  // compressed-bitmap baseline; uses the 64-bit map so that bounding
  // grids larger than 2^32 are supported.
  template<std::size_t D>
    struct Roaring
  {
    [[nodiscard]] explicit Roaring (Grid<D> grid) noexcept
      : _grid {grid}
    {}

    [[nodiscard]] static constexpr auto name() noexcept
    {
      return "roaring";
    }

    auto add (Point<D> const& point) noexcept -> void
    {
      assert (_point_count < _grid.size());
      assert (!_last_added || *_last_added < point);

      auto const linear_index {_grid.UNCHECKED_pos (point)};

      _bitmap.add (static_cast<std::uint64_t> (linear_index));
      _last_added = point;
      _point_count += 1;
    }

    [[nodiscard]] auto build() && noexcept -> Roaring
    {
      _bitmap.runOptimize();
      _bitmap.shrinkToFit();
      return std::move (*this);
    }

    [[nodiscard]] auto size() const noexcept -> Index
    {
      return _point_count;
    }

    [[nodiscard]] auto bytes() const noexcept -> std::size_t
    {
      return sizeof (*this) + _bitmap.getSizeInBytes (true);
    }

    [[nodiscard]] auto at (Index index) const -> Point<D>
    {
      if (!(index < size()))
      {
        throw std::out_of_range {"Roaring::at: index out of range"};
      }

      auto element {std::uint64_t {0}};

      if (!_bitmap.select (static_cast<std::uint64_t> (index), &element))
      {
        throw std::out_of_range {"Roaring::at: index out of range"};
      }

      return _grid.at (static_cast<Index> (element));
    }

    [[nodiscard]] auto pos (Point<D> point) const -> Index
    {
      auto const linear_index {_grid.pos (point)};
      auto const offset
        { _bitmap.getIndex (static_cast<std::uint64_t> (linear_index))
        };

      if (offset < 0)
      {
        throw std::out_of_range {"Roaring::pos: point out of range"};
      }

      return static_cast<Index> (offset);
    }

    [[nodiscard]] auto select
      ( ipt::Cuboid<D> query
      ) const -> std::generator<ipt::Cuboid<D>>
    {
      auto const grid_cuboid {to_ipt_cuboid (_grid)};
      auto const intersection {grid_cuboid.intersect (query)};

      if (!intersection)
      {
        co_return;
      }

      for (auto const& point : enumerate (*intersection))
      {
        auto const linear {_grid.UNCHECKED_pos (point)};

        if (_bitmap.contains (static_cast<std::uint64_t> (linear)))
        {
          co_yield singleton_cuboid (point);
        }
      }
    }

  private:
    Grid<D> _grid;
    roaring::Roaring64Map _bitmap;
    Index _point_count {0};
    std::optional<Point<D>> _last_added;
  };
}

namespace
{
  using Clock = std::chrono::steady_clock;

  using namespace ipt;

  template<typename Q, std::size_t D>
    concept is_queryable
      = requires (Q const& q, Index index, Point<D> point)
        {
          { q.at (index) } -> std::same_as<Point<D>>;
          { q.pos (point) } -> std::same_as<Index>;
          { q.size() } -> std::same_as<Index>;
          { q.bytes() } -> std::same_as<std::size_t>;
        };

  template<typename B, std::size_t D>
    concept is_builder
      = requires (B builder, Point<D> const& point)
        {
          builder.add (point);
          { B::name() } -> std::convertible_to<std::string_view>;
          { std::move (builder).build() };
        };

  struct RandomSample
  {
    std::size_t count;
    std::uint64_t seed;
  };

  template<std::size_t D>
    struct Data
  {
    // Data contains all points from the grid.
    //
    [[nodiscard]] explicit constexpr Data (Grid<D> grid)
    {
      add_points (grid);
      std::ranges::sort (_points);
    }

    // Data contains all points from the union of all grids.
    //
    template<typename... Grids>
      requires (sizeof... (Grids) > 1 && (std::same_as<Grids, Grid<D>> && ...))
    [[nodiscard]] constexpr Data (Grids const&... grids)
    {
      (add_points (grids), ...);

      std::ranges::sort (_points);

      auto duplicate_points {std::ranges::unique (_points)};
      _points.erase
        ( std::begin (duplicate_points)
        , std::end (_points)
        );
    }

    // Data contains at most sample.count many randomly samples points
    // from the grid.
    //
    Data (Grid<D> grid, RandomSample sample)
      : Data {grid}
    {
      if (sample.count < _points.size())
      {
        {
          auto engine {std::mt19937_64 {sample.seed}};
          std::ranges::shuffle (_points, engine);
        }

        _points.erase
          ( std::ranges::next (std::ranges::begin (_points), sample.count)
          , std::ranges::end (_points)
          );
        std::ranges::sort (_points);
      }
    }

    // Number of points,
    //
    [[nodiscard]] constexpr auto size() const noexcept -> std::size_t
    {
      return _points.size();
    }

    // Returns a random sample of sample.count many points selected
    // uniformly among all points. May contain duplicates.
    //
    [[nodiscard]] auto sample
      ( RandomSample sample
      ) const -> std::vector<Point<D>>
    {
      assert (!_points.empty());
      assert (sample.count > 0);

      auto engine {std::mt19937_64 {sample.seed}};
      auto distribution
        { std::uniform_int_distribution<std::size_t> {0, _points.size() - 1}
        };

      auto result {std::vector<Point<D>>{}};
      result.reserve (sample.count);

      std::generate_n
        ( std::back_inserter (result)
        , sample.count
        , [&] noexcept
          {
            return _points[distribution (engine)];
          }
        );

      return result;
    }

    // Returns a random sample of sample.count many valid indices. May
    // contain duplicates.
    //
    [[nodiscard]] auto index_sample
      ( RandomSample sample
      ) const -> std::vector<Index>
    {
      assert (!_points.empty());
      assert (sample.count > 0);

      auto engine {std::mt19937_64 {sample.seed}};
      auto distribution
        { std::uniform_int_distribution<Index> {0,_points.size() - 1}
        };

      auto result {std::vector<Index>{}};
      result.reserve (sample.count);

      std::generate_n
        ( std::back_inserter (result)
        , sample.count
        , [&] noexcept
          {
            return distribution (engine);
          }
        );

      return result;
    }

    // Returns the points, sorted, without duplicates.
    //
    [[nodiscard]] constexpr auto points
      (
      ) const noexcept -> std::vector<Point<D>> const&
    {
      return _points;
    }

    static constexpr auto max_query_count {std::size_t {1'000}};

    [[nodiscard]] constexpr auto query_count
      (
      ) const noexcept -> std::size_t
    {
      return std::min (max_query_count, _points.size());
    }

    [[nodiscard]] auto bounding_grid() const -> Grid<D>
    {
      auto maximas
        { std::ranges::fold_left
          ( _points
          , zeros<Coordinate, D>()
          , [] (auto maxima, auto const& point)
            {
              std::ranges::for_each
                ( std::views::iota (std::size_t {0}, D)
                , [&] (auto d)
                  {
                    maxima[d] = std::max (maxima[d], point[d]);
                  }
                );

              return maxima;
            }
          )
        };

      std::ranges::transform
        ( maximas
        , std::ranges::begin (maximas)
        , [] (auto e) { return e + 1; }
        );

      return Grid<D>
        { Origin<D> {zeros<Coordinate, D>()}
        , Extents<D> {maximas}
        , Strides<D> {ones<Coordinate, D>()}
        };
    }

  private:
    std::vector<Point<D>> _points;

    auto add_points (Grid<D> grid) -> void
    {
      _points.reserve (_points.size() + grid.size());

      struct Indices
      {
        [[nodiscard]] explicit constexpr Indices (Extents<D> extents) noexcept
          : _extents {std::move (extents)}
        {}
        auto operator++() -> Indices&
        {
          auto d {std::size_t {0}};

          while (d < D && std::cmp_equal (++_indices[d], _extents[d]))
          {
            _indices[d] = 0;
            d += 1;
          }

          return *this;
        }
        [[nodiscard]] constexpr auto at (std::size_t d) const noexcept -> Index
        {
          assert (d < D);

          return _indices[d];
        }

      private:
        Extents<D> _extents;
        std::array<Index, D> _indices {zeros<Index, D>()};
      };

      std::generate_n
        ( std::back_inserter (_points)
        , grid.size()
        , [&, indices {Indices {grid.extents()}}] mutable
          {
            auto coordinates {std::array<Coordinate, D>{}};

            std::ranges::for_each
              ( std::views::iota (std::size_t {0}, D)
              , [&] (auto d)
                {
                  coordinates[d]
                    = grid.origin()[d]
                    + indices.at (d)
                    * grid.strides()[d]
                    ;
                }
              );

            ++indices;

            return Point<D> {coordinates};
          }
        );
    }
  };

  template<std::size_t D, std::size_t N>
    [[nodiscard]] auto make_data (std::array<Grid<D>, N> const& grids) -> Data<D>
  {
    static_assert (1 < N);

    return std::invoke
      ( [&]<std::size_t... Indices>
          ( std::index_sequence<Indices...>
          )
        {
          return Data<D> {grids[Indices]...};
        }
      , std::make_index_sequence<N> {}
      );
  }

  struct Timing
  {
    [[nodiscard]] constexpr Timing
      ( Clock::duration duration
      , std::size_t operation_count
      , std::string_view operation_name
      ) noexcept
        : _duration {duration}
        , _operation_count {operation_count}
        , _operation_name {operation_name}
    {}

    template<std::invocable F>
      [[nodiscard]] Timing
        ( std::uint64_t& sink
        , std::size_t operation_count
        , std::string_view operation_name
        , F&& function
        , bool warmup = false
        )
          : Timing
            { std::invoke
              ( [&sink, &function, warmup]
                {
                  if (warmup)
                  {
                    auto const checksum
                      { static_cast<std::uint64_t> (std::invoke (function))
                      };
                    sink ^= checksum + 0x9e3779b97f4a7c15ULL;
                  }
                  auto const start {Clock::now()};
                  auto const checksum
                    { static_cast<std::uint64_t> (std::invoke (function))
                    };
                  auto const stop {Clock::now()};
                  sink ^= checksum + 0x9e3779b97f4a7c15ULL;
                  return stop - start;
                }
              )
            , operation_count
            , operation_name
            }
    {}

    [[nodiscard]] auto duration() const noexcept -> Clock::duration
    {
      return _duration;
    }
    [[nodiscard]] auto operation_count() const noexcept -> std::size_t
    {
      return _operation_count;
    }
    [[nodiscard]] auto operation_name() const noexcept -> std::string_view
    {
      return _operation_name;
    }
    [[nodiscard]] auto nanoseconds_per_operation() const -> double
    {
      assert (_operation_count > 0);

      return static_cast<double>
        ( std::chrono::duration_cast<std::chrono::nanoseconds> (_duration)
        . count()
        )
        / static_cast<double> (_operation_count)
        ;
    }
    [[nodiscard]] auto millions_per_second() const -> double
    {
      assert (_duration.count() > 0);

      return static_cast<double> (_operation_count)
        / std::chrono::duration<double> (_duration).count()
        / 1'000'000.0
        ;
    }

  private:
    Clock::duration _duration;
    std::size_t _operation_count;
    std::string_view _operation_name;
  };

  template<std::size_t D>
    struct BenchmarkContext
  {
    Grid<D> grid;
    std::uint64_t seed;
    std::string_view scenario;
    std::size_t point_percentage;
    std::size_t point_count{};
    std::size_t query_count{};
    std::size_t all_query_count{};
  };

  template<std::size_t D>
    struct Measurement
  {
    BenchmarkContext<D> const& context;
    std::string_view algorithm;
    std::string_view metric;
  };

  template<std::size_t D, is_builder<D> Builder>
    struct TimedBuild
  {
    using Queryable = decltype (std::declval<Builder>().build());

    [[nodiscard]] explicit TimedBuild
      ( std::span<Point<D> const> points
      )
      requires std::default_initializable<Builder>
        : TimedBuild {points, Builder {}}
    {}

    [[nodiscard]] TimedBuild
      ( std::span<Point<D> const> points
      , Builder builder
      )
        : TimedBuild
          { std::invoke
            ( [&points, builder = std::move (builder)] () mutable
              {
                assert (!points.empty());

                auto const start {Clock::now()};
                std::ranges::for_each
                  ( points
                  , [&builder] (auto const& point) noexcept
                    {
                      builder.add (point);
                    }
                  );
                auto result {std::move (builder).build()};
                auto const stop {Clock::now()};

                return std::make_tuple (std::move (result), stop - start);
              }
            )
              , points.size()
              }
            {}

    Queryable queryable;
    Timing construction;

  private:
    [[nodiscard]] TimedBuild
      ( std::tuple<Queryable, Clock::duration> timed
      , std::size_t point_count
      )
        : queryable {std::move (std::get<Queryable> (timed))}
        , construction {std::get<Clock::duration> (timed), point_count, "point"}
    {}
  };

  template<std::size_t D, is_queryable<D> Structure>
    struct TimedQueries
  {
    [[nodiscard]] TimedQueries
      ( BenchmarkContext<D> const& context
      , std::string_view algorithm
      , std::uint64_t& sink
      , Structure const& structure
      , std::span<Point<D> const> points
      , std::span<Point<D> const> point_sample
      , std::span<Point<D> const> all_point_sample
      , std::span<Index const> index_sample
      , std::span<Index const> all_index_sample
      )
        : context {context}
        , algorithm {algorithm}
        , pos_random
          { sink
          , point_sample.size()
          , "pos"
          , [&]
            {
              return std::ranges::fold_left
                ( point_sample
                , Index {0}
                , [&structure] (auto sum, auto const& point) noexcept
                  {
                    auto const index {structure.pos (point)};
                    assert (structure.at (index) == point);

                    return sum + index;
                  }
                );
            }
          , small_sample (point_sample.size())
          }
        , pos_all
          { sink
          , all_point_sample.size()
          , "pos"
          , [&]
            {
              return std::ranges::fold_left
                ( all_point_sample
                , Index {0}
                , [&structure] (auto sum, auto const& point) noexcept
                  {
                    auto const index {structure.pos (point)};
                    assert (structure.at (index) == point);

                    return sum + index;
                  }
                );
            }
          , small_sample (all_point_sample.size())
          }
        , at_random
          { sink
          , index_sample.size()
          , "at"
          , [&]
            {
              return std::ranges::fold_left
                ( index_sample
                , std::uint64_t {0}
                , [&structure] (auto sum, auto index) noexcept
                  {
                    auto const point {structure.at (index)};
                    assert (structure.pos (point) == index);

                    return sum + static_cast<std::uint64_t> (point[0]);
                  }
                );
            }
          , small_sample (index_sample.size())
          }
        , at_all
          { sink
          , all_index_sample.size()
          , "at"
          , [&]
            {
              return std::ranges::fold_left
                ( all_index_sample
                , std::uint64_t {0}
                , [&structure] (auto sum, auto index) noexcept
                  {
                    auto const point {structure.at (index)};
                    assert (structure.pos (point) == index);

                    return sum + static_cast<std::uint64_t> (point[0]);
                  }
                );
            }
          , small_sample (all_index_sample.size())
          }
    {}

    BenchmarkContext<D> const& context;
    std::string_view algorithm;
    Timing pos_random;
    Timing pos_all;
    Timing at_random;
    Timing at_all;

  private:
    // Sample sizes below this threshold do not amortise the cold-cache and
    // branch-predictor cost of the first iteration into the steady-state
    // measurement; for those a single warmup pass over the same sample is
    // run before the timed pass.
    [[nodiscard]] static constexpr auto small_sample
      ( std::size_t operation_count
      ) noexcept -> bool
    {
      return operation_count < 100;
    }
  };

  template<std::size_t D, typename Structure>
    TimedQueries (BenchmarkContext<D> const&, std::string_view, std::uint64_t&, Structure const&, std::vector<Point<D>> const&, std::vector<Point<D>> const&, std::vector<Point<D>> const&, std::vector<Index> const&, std::vector<Index> const&) -> TimedQueries<D, Structure>;

  auto random_seed() -> std::uint64_t
  {
    static auto engine {std::mt19937_64 {20260415ULL}};
    return engine();
  }

  // Densities used for the per-baseline selection/restriction sweep.
  // Each density d gives queries that target d * |grid| points.
  inline constexpr auto select_densities
    { std::array<double, 4> {0.001, 0.01, 0.1, 1.0} };

  inline constexpr auto select_density_tags
    { std::array<std::string_view, 4>
      {"d0001", "d001", "d01", "d1"}
    };

  inline constexpr auto select_queries_per_density
    {std::size_t {32}};

  // Build a query cuboid covering an axis-aligned sub-bbox of `world`
  // whose target volume is `density * world.size()`.
  template<std::size_t D, typename RandomEngine>
    [[nodiscard]] auto random_select_query
      ( ipt::Cuboid<D> const& world
      , double density
      , RandomEngine& random_engine
      ) -> ipt::Cuboid<D>
  {
    auto const target_volume
      { std::max
        ( Index {1}
        , static_cast<Index>
            (density * static_cast<double> (world.size()))
        )
      };

    auto const per_axis_extent
      { std::pow
        ( static_cast<double> (target_volume)
        , 1.0 / static_cast<double> (D)
        )
      };

    auto build_one
      { [&] (auto d) -> ipt::Ruler
        {
          auto const& ruler {world.ruler (d)};
          auto const ruler_count
            { static_cast<Index>
                ((ruler.end() - ruler.begin()) / ruler.stride())
            };
          auto const target_count
            { std::clamp
              ( static_cast<Index> (per_axis_extent + 0.5)
              , Index {1}
              , ruler_count
              )
            };
          auto const max_start_count {ruler_count - target_count};
          auto const start_count
            { (max_start_count == 0)
                ? Index {0}
                : std::uniform_int_distribution<Index>
                    {Index {0}, max_start_count} (random_engine)
            };
          auto const begin
            { static_cast<Coordinate>
                ( ruler.begin()
                + static_cast<Coordinate> (start_count) * ruler.stride()
                )
            };
          auto const end
            { static_cast<Coordinate>
                ( begin
                + static_cast<Coordinate> (target_count) * ruler.stride()
                )
            };

          return ipt::Ruler {begin, ruler.stride(), end};
        }
      };

    return std::invoke
      ( [&]<std::size_t... Dimensions>
          (std::index_sequence<Dimensions...>) -> ipt::Cuboid<D>
        {
          return ipt::Cuboid<D>
            { std::array<ipt::Ruler, D> {build_one (Dimensions)...} };
        }
      , std::make_index_sequence<D> {}
      );
  }

  // Per-baseline selection + restriction microbenchmark.
  //
  // For each density tier this prints four lines using the same
  // {context} algorithm=NAME metric=... format as the existing query
  // measurements:
  //
  //   metric=select_<tag>            ns_per_select
  //   metric=restrict_<tag>          ns_per_restrict
  //   metric=select_<tag>_cuboids    value=<sum of yielded cuboids>
  //   metric=select_<tag>_points     value=<sum of yielded points>
  template<std::size_t D, typename Structure>
    auto run_select_sweep
      ( BenchmarkContext<D> const& context
      , std::string_view           algorithm
      , Structure const&           structure
      , std::uint64_t&             sink
      ) -> void
  {
    auto const world {to_ipt_cuboid (context.grid)};
    auto random_engine {std::mt19937_64 {context.seed ^ 0x53'4C'43'54ULL}};

    for (auto density_index {std::size_t {0}};
         density_index < select_densities.size();
         ++density_index)
    {
      auto const density {select_densities[density_index]};
      auto const tag {select_density_tags[density_index]};

      auto queries {std::vector<ipt::Cuboid<D>> {}};
      queries.reserve (select_queries_per_density);
      for ( auto query_index {std::size_t {0}}
          ; query_index < select_queries_per_density
          ; ++query_index
          )
      {
        queries.push_back (random_select_query (world, density, random_engine));
      }

      auto total_cuboids {std::size_t {0}};
      auto total_points {Index {0}};
      auto cuboid_counts
        {std::vector<std::size_t> (queries.size(), 0)};

      auto const select_timing
        { Timing
          { sink
          , queries.size()
          , "select"
          , [&] () noexcept -> std::uint64_t
            {
              auto checksum {std::uint64_t {0}};

              for ( auto query_index {std::size_t {0}}
                  ; query_index < queries.size()
                  ; ++query_index
                  )
              {
                auto cuboid_count {std::size_t {0}};
                for ( auto const& cuboid
                    : structure.select (queries[query_index])
                    )
                {
                  cuboid_count += 1;
                  total_points += cuboid.size();
                  checksum
                    += static_cast<std::uint64_t> (cuboid.size());
                }
                cuboid_counts[query_index] = cuboid_count;
                total_cuboids += cuboid_count;
              }

              return checksum;
            }
          }
        };

      auto const restrict_timing
        { Timing
          { sink
          , queries.size()
          , "restrict"
          , [&] () noexcept -> std::uint64_t
            {
              auto checksum {std::uint64_t {0}};

              for ( auto query_index {std::size_t {0}}
                  ; query_index < queries.size()
                  ; ++query_index
                  )
              {
                if (cuboid_counts[query_index] == 0)
                {
                  continue;
                }

                auto restricted
                  { ipt::IPT<D>
                    { std::from_range
                    , structure.select (queries[query_index])
                    }
                  };
                checksum
                  += static_cast<std::uint64_t> (restricted.size());
              }

              return checksum;
            }
          }
        };

      auto const select_metric
        {std::format ("select_{}", tag)};
      auto const restrict_metric
        {std::format ("restrict_{}", tag)};
      auto const cuboids_metric
        {std::format ("select_{}_cuboids", tag)};
      auto const points_metric
        {std::format ("select_{}_points", tag)};

      std::print
        ( "{} {}\n"
        , Measurement<D> {context, algorithm, select_metric}
        , select_timing
        );
      std::print
        ( "{} {}\n"
        , Measurement<D> {context, algorithm, restrict_metric}
        , restrict_timing
        );
      std::print
        ( "{} value={}\n"
        , Measurement<D> {context, algorithm, cuboids_metric}
        , total_cuboids
        );
      std::print
        ( "{} value={}\n"
        , Measurement<D> {context, algorithm, points_metric}
        , total_points
        );
    }
  }

  template<std::size_t D>
    auto run_scenario
      ( Data<D> const& data
      , BenchmarkContext<D> context
      , std::uint64_t& sink
      , bool benchmark_regular_baseline
      ) -> void
  {
    if (!selected_scenario (context.scenario))
    {
      return;
    }

    auto const run_greedy_merge
      { selected_algorithm (create::GreedyPlusMerge<D>::name())
      };
    auto const run_greedy_combine
      { selected_algorithm (create::GreedyPlusCombine<D>::name())
      };
    auto const run_sorted_points
      { selected_algorithm (SortedPoints<D>::name())
      };
    auto const run_dense_bitset
      { selected_algorithm (DenseBitset<D>::name())
      };
    auto const run_block_bitmap
      { selected_algorithm (BlockBitmap<D>::name())
      };
    auto const run_roaring
      { selected_algorithm (Roaring<D>::name())
      };
    auto const run_lex_run
      { selected_algorithm (LexRun<D>::name())
      };
    auto const run_row_csr_k1
      { selected_algorithm (RowCSR<D, 1>::name())
      };
    auto const run_row_csr_km
      { selected_algorithm (RowCSR<D, D - 1>::name())
      };
    auto const run_grid
      { benchmark_regular_baseline && selected_algorithm (Grid<D>::name())
      };
    auto const run_regular_ipt
      { benchmark_regular_baseline && selected_algorithm (create::Regular<D>::name())
      };

    if ( !run_greedy_merge
       && !run_greedy_combine
       && !run_sorted_points
       && !run_dense_bitset
       && !run_block_bitmap
       && !run_roaring
       && !run_lex_run
       && !run_row_csr_k1
       && !run_row_csr_km
       && !run_grid
       && !run_regular_ipt
       )
    {
      return;
    }

    context.point_count = data.size();
    context.query_count = data.query_count();

    auto const point_sample
      { data.sample
        ( { context.query_count
          , random_seed()
          }
        )
      };
    auto const index_sample
      { data.index_sample
        ( { context.query_count
          , random_seed()
          }
        )
      };
    auto const all_point_sample
      { evenly_spaced_sample
          ( std::span<Point<D> const> {data.points()}
          , benchmark_all_query_cap()
          )
      };
    auto const all_index_sample
      { evenly_spaced_index_sample
          ( data.points().size()
          , benchmark_all_query_cap()
          )
      };
    context.all_query_count = all_index_sample.size();

    auto greedy_merge
      { std::optional<TimedBuild<D, create::GreedyPlusMerge<D>>> {}
      };
    if (run_greedy_merge || run_greedy_combine || run_regular_ipt)
    {
      greedy_merge.emplace (data.points());
    }

    if (run_greedy_merge)
    {
      assert (greedy_merge.has_value());

      std::print ("{} {}\n", Measurement<D> {context, create::GreedyPlusMerge<D>::name(), "construct"}, greedy_merge->construction);
      std::print ("{} value={}\n", Measurement<D> {context, create::GreedyPlusMerge<D>::name(), "ipt_bytes"}, greedy_merge->queryable.bytes());
      std::print ("{} value={}\n", Measurement<D> {context, create::GreedyPlusMerge<D>::name(), "cuboids"}, greedy_merge->queryable.number_of_entries());
      std::print
        ( "{}"
        , TimedQueries
          { context
          , create::GreedyPlusMerge<D>::name()
          , sink
          , greedy_merge->queryable
          , data.points()
          , point_sample
          , all_point_sample
          , index_sample
          , all_index_sample
          }
        );
      run_select_sweep
        (context, create::GreedyPlusMerge<D>::name(), greedy_merge->queryable, sink);
    }

    if (run_greedy_combine)
    {
      auto const greedy_combine
        { TimedBuild<D, create::GreedyPlusCombine<D>> {data.points()}
        };

      assert (greedy_merge.has_value());
      assert (greedy_merge->queryable == greedy_combine.queryable);

      std::print ("{} {}\n", Measurement<D> {context, create::GreedyPlusCombine<D>::name(), "construct"}, greedy_combine.construction);
      std::print ("{} value={}\n", Measurement<D> {context, create::GreedyPlusCombine<D>::name(), "ipt_bytes"}, greedy_combine.queryable.bytes());
      std::print ("{} value={}\n", Measurement<D> {context, create::GreedyPlusCombine<D>::name(), "cuboids"}, greedy_combine.queryable.number_of_entries());
      std::print
        ( "{}"
        , TimedQueries
          { context
          , create::GreedyPlusCombine<D>::name()
          , sink
          , greedy_combine.queryable
          , data.points()
          , point_sample
          , all_point_sample
          , index_sample
          , all_index_sample
          }
        );
      run_select_sweep
        (context, create::GreedyPlusCombine<D>::name(), greedy_combine.queryable, sink);
    }

    if (run_sorted_points)
    {
      auto const sorted_points
        { TimedBuild<D, SortedPoints<D>> {data.points()}
        };

      std::print ("{} {}\n", Measurement<D> {context, SortedPoints<D>::name(), "construct"}, sorted_points.construction);
      std::print ("{} value={}\n", Measurement<D> {context, SortedPoints<D>::name(), "point_bytes"}, sorted_points.queryable.bytes());
      std::print
        ( "{}"
        , TimedQueries
          { context
          , SortedPoints<D>::name()
          , sink
          , sorted_points.queryable
          , data.points()
          , point_sample
          , all_point_sample
          , index_sample
          , all_index_sample
          }
        );
      run_select_sweep
        (context, SortedPoints<D>::name(), sorted_points.queryable, sink);
    }

    if (run_dense_bitset)
    {
      auto const dense_bitset
        { TimedBuild<D, DenseBitset<D>>
          { data.points()
          , DenseBitset<D> {context.grid}
          }
        };

      std::print ("{} {}\n", Measurement<D> {context, DenseBitset<D>::name(), "construct"}, dense_bitset.construction);
      std::print ("{} value={}\n", Measurement<D> {context, DenseBitset<D>::name(), "densebitset_bytes"}, dense_bitset.queryable.bytes());
      std::print
        ( "{}"
        , TimedQueries
          { context
          , DenseBitset<D>::name()
          , sink
          , dense_bitset.queryable
          , data.points()
          , point_sample
          , all_point_sample
          , index_sample
          , all_index_sample
          }
        );
      run_select_sweep
        (context, DenseBitset<D>::name(), dense_bitset.queryable, sink);
    }

    if (run_block_bitmap)
    {
      auto const block_bitmap
        { TimedBuild<D, BlockBitmap<D>>
          { data.points()
          , BlockBitmap<D> {context.grid}
          }
        };

      std::print ("{} {}\n", Measurement<D> {context, BlockBitmap<D>::name(), "construct"}, block_bitmap.construction);
      std::print ("{} value={}\n", Measurement<D> {context, BlockBitmap<D>::name(), "blockbitmap_bytes"}, block_bitmap.queryable.bytes());
      std::print
        ( "{}"
        , TimedQueries
          { context
          , BlockBitmap<D>::name()
          , sink
          , block_bitmap.queryable
          , data.points()
          , point_sample
          , all_point_sample
          , index_sample
          , all_index_sample
          }
        );
      run_select_sweep
        (context, BlockBitmap<D>::name(), block_bitmap.queryable, sink);
    }

    if (run_roaring)
    {
      auto const roaring_baseline
        { TimedBuild<D, Roaring<D>>
          { data.points()
          , Roaring<D> {context.grid}
          }
        };

      std::print ("{} {}\n", Measurement<D> {context, Roaring<D>::name(), "construct"}, roaring_baseline.construction);
      std::print ("{} value={}\n", Measurement<D> {context, Roaring<D>::name(), "roaring_bytes"}, roaring_baseline.queryable.bytes());
      std::print
        ( "{}"
        , TimedQueries
          { context
          , Roaring<D>::name()
          , sink
          , roaring_baseline.queryable
          , data.points()
          , point_sample
          , all_point_sample
          , index_sample
          , all_index_sample
          }
        );
      run_select_sweep
        (context, Roaring<D>::name(), roaring_baseline.queryable, sink);
    }

    if (run_lex_run)
    {
      auto const lex_run
        { TimedBuild<D, LexRun<D>> {data.points()}
        };

      std::print ("{} {}\n", Measurement<D> {context, LexRun<D>::name(), "construct"}, lex_run.construction);
      std::print ("{} value={}\n", Measurement<D> {context, LexRun<D>::name(), "lexrun_bytes"}, lex_run.queryable.bytes());
      std::print
        ( "{}"
        , TimedQueries
          { context
          , LexRun<D>::name()
          , sink
          , lex_run.queryable
          , data.points()
          , point_sample
          , all_point_sample
          , index_sample
          , all_index_sample
          }
        );
      run_select_sweep
        (context, LexRun<D>::name(), lex_run.queryable, sink);
    }

    if (run_row_csr_k1)
    {
      auto const row_csr_k1
        { TimedBuild<D, RowCSR<D, 1>> {data.points()}
        };

      std::print ("{} {}\n", Measurement<D> {context, RowCSR<D, 1>::name(), "construct"}, row_csr_k1.construction);
      std::print ("{} value={}\n", Measurement<D> {context, RowCSR<D, 1>::name(), "rowcsrk1_bytes"}, row_csr_k1.queryable.bytes());
      std::print
        ( "{}"
        , TimedQueries
          { context
          , RowCSR<D, 1>::name()
          , sink
          , row_csr_k1.queryable
          , data.points()
          , point_sample
          , all_point_sample
          , index_sample
          , all_index_sample
          }
        );
      run_select_sweep
        (context, RowCSR<D, 1>::name(), row_csr_k1.queryable, sink);
    }

    if (run_row_csr_km)
    {
      auto const row_csr_km
        { TimedBuild<D, RowCSR<D, D - 1>> {data.points()}
        };

      std::print ("{} {}\n", Measurement<D> {context, RowCSR<D, D - 1>::name(), "construct"}, row_csr_km.construction);
      std::print ("{} value={}\n", Measurement<D> {context, RowCSR<D, D - 1>::name(), "rowcsrkm_bytes"}, row_csr_km.queryable.bytes());
      std::print
        ( "{}"
        , TimedQueries
          { context
          , RowCSR<D, D - 1>::name()
          , sink
          , row_csr_km.queryable
          , data.points()
          , point_sample
          , all_point_sample
          , index_sample
          , all_index_sample
          }
        );
      run_select_sweep
        (context, RowCSR<D, D - 1>::name(), row_csr_km.queryable, sink);
    }

    if (run_grid)
    {
      auto const grid_construction
        { Timing
          { sink
          , context.point_count
          , "point"
          , [&] () noexcept -> std::uint64_t
            {
              auto const grid
                { Grid<D>
                  { context.grid.origin()
                  , context.grid.extents()
                  , context.grid.strides()
                  }
                };
              return static_cast<std::uint64_t> (grid.size());
            }
          }
        };

      std::print ("{} {}\n", Measurement<D> {context, Grid<D>::name(), "construct"}, grid_construction);
      std::print ("{} value={}\n", Measurement<D> {context, Grid<D>::name(), "grid_bytes"}, context.grid.bytes());
      std::print
        ( "{}"
        , TimedQueries
          { context
          , Grid<D>::name()
          , sink
          , context.grid
          , data.points()
          , point_sample
          , all_point_sample
          , index_sample
          , all_index_sample
          }
        );
      run_select_sweep
        (context, Grid<D>::name(), context.grid, sink);
    }

    if (run_regular_ipt)
    {
      auto [regular_queryable, regular_duration]
        { std::invoke
          ( [&]
            {
              auto const start {Clock::now()};
              auto queryable
                { create::Regular<D>
                  { make_cuboid
                    ( context.grid.origin()
                    , context.grid.extents()
                    , context.grid.strides()
                    )
                  }
                  .build()
                };
              auto const stop {Clock::now()};

              return std::make_tuple (std::move (queryable), stop - start);
            }
          )
        };
      auto const regular_construction
        { Timing {regular_duration, context.point_count, "point"}
        };

      assert (greedy_merge.has_value());
      assert (greedy_merge->queryable == regular_queryable);

      std::print ("{} {}\n", Measurement<D> {context, create::Regular<D>::name(), "construct"}, regular_construction);
      std::print ("{} value={}\n", Measurement<D> {context, create::Regular<D>::name(), "ipt_bytes"}, regular_queryable.bytes());
      std::print ("{} value={}\n", Measurement<D> {context, create::Regular<D>::name(), "cuboids"}, regular_queryable.number_of_entries());
      std::print
        ( "{}"
        , TimedQueries
          { context
          , create::Regular<D>::name()
          , sink
          , regular_queryable
          , data.points()
          , point_sample
          , all_point_sample
          , index_sample
          , all_index_sample
          }
        );
      run_select_sweep
        (context, create::Regular<D>::name(), regular_queryable, sink);
    }
  }
}

namespace std
{
  template<size_t D>
    struct formatter<Grid<D>>
  {
    constexpr auto parse (format_parse_context& context)
    {
      return context.begin();
    }

    auto format
      ( Grid<D> const& grid
      , format_context& context
      ) const
    {
      auto out {context.out()};
      auto first {true};

      ranges::for_each
        ( grid.extents()
        , [&out, &first] (auto extent)
          {
            if (!first)
            {
              out = format_to (out, "x");
            }
            out = format_to (out, "{}", extent);
            first = false;
          }
        );

      return out;
    }
  };

  template<size_t D>
    struct formatter<BenchmarkContext<D>>
  {
    constexpr auto parse (format_parse_context& context)
    {
      return context.begin();
    }

    auto format
      ( BenchmarkContext<D> const& benchmark_context
      , format_context& context
      ) const
    {
      return format_to
        ( context.out()
        , "grid={} seed={} scenario={} point_percentage={}"
        " point_count={} query_count={} all_query_count={}"
        , benchmark_context.grid
        , benchmark_context.seed
        , benchmark_context.scenario
        , benchmark_context.point_percentage
        , benchmark_context.point_count
        , benchmark_context.query_count
        , benchmark_context.all_query_count
        );
    }
  };

  template<>
    struct formatter<Timing>
  {
    constexpr auto parse (format_parse_context& context)
    {
      return context.begin();
    }

    auto format
      ( Timing const& artifact
      , format_context& context
      ) const
    {
      return format_to
        ( context.out()
        , "ticks={} ns_per_{}={:.3f} M{}_per_sec={:.3f}"
        , artifact.duration().count()
        , artifact.operation_name()
        , artifact.nanoseconds_per_operation()
        , artifact.operation_name()
        , artifact.millions_per_second()
        );
    }
  };

  template<size_t D>
    struct formatter<Measurement<D>>
  {
    constexpr auto parse (format_parse_context& context)
    {
      return context.begin();
    }

    auto format
      ( Measurement<D> const& measurement
      , format_context& context
      ) const
    {
      return format_to
        ( context.out()
        , "{} algorithm={} metric={}"
        , measurement.context
        , measurement.algorithm
        , measurement.metric
        );
    }
  };

  template<size_t D, typename Structure>
    struct formatter<TimedQueries<D, Structure>>
  {
    constexpr auto parse (format_parse_context& context)
    {
      return context.begin();
    }

    auto format
      ( TimedQueries<D, Structure> const& timed
      , format_context& context
      ) const
    {
      return format_to
        ( context.out()
        , "{} algorithm={} metric=pos_random {}\n"
          "{} algorithm={} metric=pos_all {}\n"
          "{} algorithm={} metric=at_random {}\n"
          "{} algorithm={} metric=at_all {}\n"
        , timed.context, timed.algorithm, timed.pos_random
        , timed.context, timed.algorithm, timed.pos_all
        , timed.context, timed.algorithm, timed.at_random
        , timed.context, timed.algorithm, timed.at_all
        );
    }
  };
}

#ifndef NDEBUG
namespace verify
{
  using namespace ipt;

  // ---- ruler / cuboid intersect ------------------------------------

  inline auto enumerate_ruler
    ( Ruler const& ruler
    ) -> std::vector<Coordinate>
  {
    auto out {std::vector<Coordinate> {}};
    for (auto value {ruler.begin()}; value < ruler.end(); value += ruler.stride())
    {
      out.push_back (value);
    }
    return out;
  }

  inline auto reference_intersect
    ( Ruler const& a
    , Ruler const& b
    ) -> std::vector<Coordinate>
  {
    auto va {enumerate_ruler (a)};
    auto vb {enumerate_ruler (b)};
    auto out {std::vector<Coordinate> {}};
    std::ranges::set_intersection (va, vb, std::back_inserter (out));
    return out;
  }

  inline auto check
    ( bool ok
    , char const* message
    ) -> void
  {
    if (!ok)
    {
      throw std::runtime_error
        {std::format ("verify intersect_test FAIL: {}", message)};
    }
  }

  inline auto check_intersect_matches_reference
    ( Ruler const& a
    , Ruler const& b
    , char const* label
    ) -> void
  {
    auto const reference {reference_intersect (a, b)};
    auto const result {a.intersect (b)};

    if (reference.empty())
    {
      check (!result.has_value(), label);
      return;
    }

    if (!result.has_value())
    {
      throw std::runtime_error
        { std::format
            ( "verify intersect_test FAIL: {} expected non-empty result of size {}"
            , label
            , reference.size()
            )
        };
    }

    auto const actual {enumerate_ruler (*result)};
    check (actual == reference, label);
  }

  inline auto run_intersect() -> void
  {
    // Ruler::intersect cases
    check_intersect_matches_reference
      (Ruler {0, 1, 10}, Ruler {3, 1, 7}, "stride-1 overlap");
    check_intersect_matches_reference
      (Ruler {0, 1, 5}, Ruler {7, 1, 10}, "disjoint");
    check_intersect_matches_reference
      (Ruler {0, 2, 10}, Ruler {1, 2, 11}, "stride-2 lattice-disjoint");
    check_intersect_matches_reference
      (Ruler {0, 2, 20}, Ruler {4, 2, 16}, "stride-2 aligned");
    check_intersect_matches_reference
      (Ruler {0, 2, 30}, Ruler {0, 3, 30}, "stride 2 vs 3 from 0");
    check_intersect_matches_reference
      (Ruler {1, 2, 31}, Ruler {0, 3, 30}, "stride 2 vs 3 offset");
    check_intersect_matches_reference
      ( Ruler {Ruler::Singleton {5}}
      , Ruler {Ruler::Singleton {5}}
      , "singleton match"
      );
    check_intersect_matches_reference
      ( Ruler {Ruler::Singleton {5}}
      , Ruler {Ruler::Singleton {6}}
      , "singleton mismatch"
      );
    check_intersect_matches_reference
      (Ruler {0, 4, 100}, Ruler {6, 6, 102}, "stride 4 vs 6");
    check_intersect_matches_reference
      (Ruler {-10, 3, 20}, Ruler {-7, 5, 18}, "negative coordinates");

    // Cuboid::intersect
    {
      auto const c1 {Cuboid<2> {std::array<Ruler, 2> {Ruler {0, 1, 10}, Ruler {0, 1, 10}}}};
      auto const c2 {Cuboid<2> {std::array<Ruler, 2> {Ruler {3, 1, 8}, Ruler {2, 1, 7}}}};
      auto const r {c1.intersect (c2)};
      check (r.has_value(), "cuboid 2D intersect non-empty");
      check (r->ruler (0).begin() == 3, "cuboid 2D ruler0 begin");
      check (r->ruler (0).end() == 8, "cuboid 2D ruler0 end");
      check (r->ruler (1).begin() == 2, "cuboid 2D ruler1 begin");
      check (r->ruler (1).end() == 7, "cuboid 2D ruler1 end");
      check (r->size() == Index {25}, "cuboid 2D size");
    }
    {
      auto const c1 {Cuboid<2> {std::array<Ruler, 2> {Ruler {0, 1, 10}, Ruler {0, 1, 10}}}};
      auto const c2 {Cuboid<2> {std::array<Ruler, 2> {Ruler {0, 1, 10}, Ruler {20, 1, 30}}}};
      check (!c1.intersect (c2).has_value(), "cuboid 2D empty axis");
    }

    // Cuboid::contains
    {
      auto const c {Cuboid<2> {std::array<Ruler, 2> {Ruler {0, 2, 10}, Ruler {1, 3, 10}}}};
      check ( c.contains (Point<2> {std::array<Coordinate, 2> {4, 7}}), "contains hit");
      check (!c.contains (Point<2> {std::array<Coordinate, 2> {3, 7}}), "contains stride miss");
      check (!c.contains (Point<2> {std::array<Coordinate, 2> {4, 8}}), "contains stride miss 2");
      check (!c.contains (Point<2> {std::array<Coordinate, 2> {12, 7}}), "contains bbox miss");
    }

    // enumerate
    {
      auto const cuboid
        { Cuboid<2> {std::array<Ruler, 2> {Ruler {0, 2, 6}, Ruler {1, 1, 4}}}
        };
      auto points {std::vector<Point<2>> {}};
      for (auto const& point : enumerate (cuboid))
      {
        points.push_back (point);
      }
      check (points.size() == cuboid.size(), "enumerate size");
      check (points.front() == cuboid.glb(), "enumerate first");
      check (points.back() == cuboid.lub(), "enumerate last");
      check (std::ranges::is_sorted (points), "enumerate sorted");
    }

    // IPT::select on a small synthetic IPT.
    {
      auto cuboids {std::vector<Cuboid<2>>
        { Cuboid<2> {std::array<Ruler, 2> {Ruler {0, 1, 3}, Ruler {0, 1, 5}}}
        , Cuboid<2> {std::array<Ruler, 2> {Ruler {3, 1, 7}, Ruler {0, 1, 5}}}
        , Cuboid<2> {std::array<Ruler, 2> {Ruler {7, 1, 10}, Ruler {0, 1, 5}}}
        }};

      auto const ipt
        { IPT<2>
          { std::from_range
          , std::vector<Cuboid<2>> {cuboids}
          }
        };

      check (ipt.size() == Index {50}, "from_range size");
      check (ipt.number_of_entries() == 3, "from_range entry count");

      auto const query
        { Cuboid<2> {std::array<Ruler, 2> {Ruler {2, 1, 8}, Ruler {1, 1, 3}}} };

      auto results {std::vector<Cuboid<2>> {}};
      for (auto const& cuboid : ipt.select (query))
      {
        results.push_back (cuboid);
      }
      check (results.size() == 3, "select yields 3 sub-cuboids");

      auto const expected {std::vector<Cuboid<2>>
        { Cuboid<2> {std::array<Ruler, 2> {Ruler {2, 1, 3}, Ruler {1, 1, 3}}}
        , Cuboid<2> {std::array<Ruler, 2> {Ruler {3, 1, 7}, Ruler {1, 1, 3}}}
        , Cuboid<2> {std::array<Ruler, 2> {Ruler {7, 1, 8}, Ruler {1, 1, 3}}}
        }};
      check (results == expected, "select sub-cuboids match expected");

      // Round-trip: from_range with the select view yields a sub-IPT
      auto const sub {IPT<2> {std::from_range, ipt.select (query)}};
      check (sub.number_of_entries() == 3, "round-trip entry count");
      check (sub.size() == Index {12}, "round-trip size");

      auto round_trip_points {std::vector<Point<2>> {}};
      for (auto i {Index {0}}; i < sub.size(); ++i)
      {
        round_trip_points.push_back (sub.at (i));
      }

      auto selected_points {std::vector<Point<2>> {}};
      for (auto const& cuboid : ipt.select (query))
      {
        for (auto const& point : enumerate (cuboid))
        {
          selected_points.push_back (point);
        }
      }
      std::ranges::sort (selected_points);
      auto round_trip_points_sorted {round_trip_points};
      std::ranges::sort (round_trip_points_sorted);
      check
        ( round_trip_points_sorted == selected_points
        , "round-trip points match selection"
        );

      auto const empty_query
        { Cuboid<2> {std::array<Ruler, 2> {Ruler {100, 1, 110}, Ruler {0, 1, 5}}} };
      auto const empty_view {ipt.select (empty_query)};
      check (std::begin (empty_view) == std::default_sentinel, "empty query yields empty view");
    }

    std::print ("verify intersect_test: OK\n");
  }

  // ---- corner-steal ------------------------------------------------

  template<std::size_t D>
    class CornerSteal
  {
    static_assert (D >= 2);

  public:
    [[nodiscard]] explicit CornerSteal
      ( std::size_t copies
      , Coordinate extent = Coordinate {2}
      )
      : _copies {copies}
      , _extent {extent}
      , _points {make_points()}
    {
      if (_copies == 0)
      {
        throw std::invalid_argument
          {"CornerSteal: copies must be positive"};
      }
      if (!(_extent > 1))
      {
        throw std::invalid_argument
          {"CornerSteal: extent must be at least two"};
      }
      if (!std::ranges::is_sorted (_points))
      {
        throw std::logic_error
          {"CornerSteal: generated points must be sorted"};
      }
      if (std::ranges::adjacent_find (_points) != std::ranges::end (_points))
      {
        throw std::logic_error
          {"CornerSteal: generated points must be unique"};
      }
    }

    [[nodiscard]] constexpr auto points
      (
      ) const noexcept -> std::vector<Point<D>> const&
    {
      return _points;
    }

    [[nodiscard]] constexpr auto copies() const noexcept -> std::size_t
    {
      return _copies;
    }

    [[nodiscard]] constexpr auto optimal_entry_count
      (
      ) const noexcept -> std::size_t
    {
      return 2 * _copies;
    }

    [[nodiscard]] constexpr auto greedy_plus_merge_entry_count
      (
      ) const noexcept -> std::size_t
    {
      return _copies * (D + 1);
    }

    [[nodiscard]] auto optimal_ipt() const -> IPT<D>
    {
      auto entries {std::vector<Entry<D>> {}};
      entries.reserve (optimal_entry_count());

      auto begin {Index {0}};
      for (auto copy_index : std::views::iota (std::size_t {0}, _copies))
      {
        entries.emplace_back
          ( Cuboid<D>
            { typename Cuboid<D>::Singleton
              {corner_point (copy_index)}
            }
          , begin
          );
        begin = entries.back().end();

        entries.emplace_back (block_cuboid (copy_index), begin);
        begin = entries.back().end();
      }

      return IPT<D> {std::move (entries)};
    }

  private:
    [[nodiscard]] constexpr auto copy_offset
      ( std::size_t copy_index
      ) const noexcept -> Coordinate
    {
      return static_cast<Coordinate> (copy_index) * copy_stride();
    }

    [[nodiscard]] constexpr auto copy_stride() const noexcept -> Coordinate
    {
      return _extent + Coordinate {2};
    }

    [[nodiscard]] auto corner_point
      ( std::size_t copy_index
      ) const noexcept -> Point<D>
    {
      auto coordinates {std::array<Coordinate, D> {}};
      std::ranges::fill (coordinates, Coordinate {1});
      coordinates[0] = copy_offset (copy_index);
      return Point<D> {coordinates};
    }

    [[nodiscard]] auto block_cuboid
      ( std::size_t copy_index
      ) const noexcept -> Cuboid<D>
    {
      auto rulers
        { std::invoke
          ( [&]<std::size_t... Dimensions>
              ( std::index_sequence<Dimensions...>
              )
            {
              return std::array<Ruler, D>
                { ( static_cast<void> (Dimensions)
                  , Ruler {Coordinate {1}, Coordinate {1}, _extent + Coordinate {1}}
                  )...
                };
            }
          , std::make_index_sequence<D> {}
          )
        };
      rulers[0]
        = Ruler
          { copy_offset (copy_index) + Coordinate {1}
          , Coordinate {1}
          , copy_offset (copy_index) + _extent + Coordinate {1}
          };

      return Cuboid<D> {rulers};
    }

    [[nodiscard]] constexpr auto reserve_point_count
      (
      ) const noexcept -> std::size_t
    {
      auto block_points {std::size_t {1}};
      for ([[maybe_unused]] auto dimension : std::views::iota (std::size_t {0}, D))
      {
        block_points *= static_cast<std::size_t> (_extent);
      }
      return _copies * (block_points + 1);
    }

    auto append_block_points
      ( std::size_t copy_index
      , std::size_t dimension
      , std::array<Coordinate, D>& coordinates
      , std::vector<Point<D>>& points
      ) const -> void
    {
      if (dimension == D)
      {
        points.emplace_back (coordinates);
        return;
      }

      auto const begin
        { dimension == 0
            ? copy_offset (copy_index) + Coordinate {1}
            : Coordinate {1}
        };
      auto const end
        { dimension == 0
            ? copy_offset (copy_index) + _extent + Coordinate {1}
            : _extent + Coordinate {1}
        };

      for (auto coordinate {begin}; coordinate < end; ++coordinate)
      {
        coordinates[dimension] = coordinate;
        append_block_points (copy_index, dimension + 1, coordinates, points);
      }
    }

    [[nodiscard]] auto make_points() const -> std::vector<Point<D>>
    {
      auto points {std::vector<Point<D>> {}};
      points.reserve (reserve_point_count());

      for (auto copy_index : std::views::iota (std::size_t {0}, _copies))
      {
        points.push_back (corner_point (copy_index));
        auto coordinates {std::array<Coordinate, D> {}};
        append_block_points (copy_index, 0, coordinates, points);
      }

      return points;
    }

    std::size_t _copies;
    Coordinate _extent;
    std::vector<Point<D>> _points;
  };

  template<std::size_t D, typename Builder>
    [[nodiscard]] auto build_ipt
      ( std::span<Point<D> const> points
      ) -> IPT<D>
  {
    auto builder {Builder {}};
    std::ranges::for_each
      ( points
      , [&builder] (auto const& point) { builder.add (point); }
      );
    return std::move (builder).build();
  }

  template<std::size_t D>
    auto verify_corner_steal_case
      ( std::size_t copies
      ) -> void
  {
    auto const source {CornerSteal<D> {copies}};
    auto const expected {source.optimal_ipt()};
    auto const greedy_merge
      { build_ipt<D, create::GreedyPlusMerge<D>>
          (std::span<Point<D> const> {source.points()})
      };
    auto const greedy_combine
      { build_ipt<D, create::GreedyPlusCombine<D>>
          (std::span<Point<D> const> {source.points()})
      };

    auto const label
      { std::format
          ("corner-steal D={} copies={}", D, source.copies())
      };

    if (!(greedy_combine == expected))
    {
      throw std::runtime_error
        { std::format
            ( "{}: GreedyPlusCombine did not recover the expected optimal IPT"
            , label
            )
        };
    }
    if (greedy_combine.number_of_entries() != source.optimal_entry_count())
    {
      throw std::runtime_error
        { std::format
            ( "{}: expected {} GreedyPlusCombine entries, got {}"
            , label
            , source.optimal_entry_count()
            , greedy_combine.number_of_entries()
            )
        };
    }
    if (greedy_merge.number_of_entries() != source.greedy_plus_merge_entry_count())
    {
      throw std::runtime_error
        { std::format
            ( "{}: expected {} GreedyPlusMerge entries, got {}"
            , label
            , source.greedy_plus_merge_entry_count()
            , greedy_merge.number_of_entries()
            )
        };
    }
    if ( 2 * greedy_merge.number_of_entries()
       != (D + 1) * greedy_combine.number_of_entries()
       )
    {
      throw std::runtime_error
        {std::format ("{}: GreedyPlusMerge ratio is not (D+1)/2", label)};
    }

    std::print
      ( "verify corner_steal D={} copies={} points={} optimum={} "
        "greedy-plus-merge={} greedy-plus-combine={}\n"
      , D
      , source.copies()
      , source.points().size()
      , expected.number_of_entries()
      , greedy_merge.number_of_entries()
      , greedy_combine.number_of_entries()
      );
  }

  inline auto run_corner_steal() -> void
  {
    verify_corner_steal_case<2> (1);
    verify_corner_steal_case<2> (4);
    verify_corner_steal_case<3> (1);
    verify_corner_steal_case<3> (3);
    verify_corner_steal_case<4> (1);
    verify_corner_steal_case<4> (3);
    verify_corner_steal_case<5> (1);
    verify_corner_steal_case<5> (2);
    verify_corner_steal_case<6> (1);
    verify_corner_steal_case<6> (2);
    verify_corner_steal_case<7> (1);
    verify_corner_steal_case<8> (1);
    std::print ("verify corner_steal: OK 12 cases\n");
  }

  // ---- cross-implementation select verification --------------------

  template<std::size_t D, typename Implementation>
    [[nodiscard]] auto materialize_points
      ( Implementation const& implementation
      , Cuboid<D> const& query
      ) -> std::vector<Point<D>>
  {
    auto points {std::vector<Point<D>> {}};
    for (auto const& cuboid : implementation.select (query))
    {
      for (auto const& point : enumerate (cuboid))
      {
        points.push_back (point);
      }
    }
    std::ranges::sort (points);
    auto duplicate_points {std::ranges::unique (points)};
    points.erase
      (std::begin (duplicate_points), std::end (points));
    return points;
  }

  template<std::size_t D, typename Implementation>
    auto check_one
      ( std::string_view name
      , Implementation const& implementation
      , Cuboid<D> const& query
      , std::vector<Point<D>> const& reference
      , std::size_t query_index
      ) -> bool
  {
    auto const actual {materialize_points (implementation, query)};
    if (actual != reference)
    {
      std::print
        ( stderr
        , "MISMATCH query={} implementation={} reference_size={} got_size={}\n"
        , query_index
        , name
        , reference.size()
        , actual.size()
        );
      return false;
    }
    return true;
  }

  inline auto run_select() -> void
  {
    auto const grid
      { Grid<3>
        { Origin<3> {std::array<Coordinate, 3> {0, 0, 0}}
        , Extents<3> {std::array<Coordinate, 3> {16, 16, 16}}
        , Strides<3> {std::array<Coordinate, 3> {1, 1, 1}}
        }
      };
    auto const world {to_ipt_cuboid (grid)};
    auto data {Data<3> {grid}};
    auto const& points {data.points()};

    auto sorted_points_builder {SortedPoints<3> {}};
    for (auto const& point : points) { sorted_points_builder.add (point); }
    auto sorted_points_built {std::move (sorted_points_builder).build()};

    auto dense_bitset_builder {DenseBitset<3> {grid}};
    for (auto const& point : points) { dense_bitset_builder.add (point); }
    auto dense_bitset_built {std::move (dense_bitset_builder).build()};

    auto block_bitmap_builder {BlockBitmap<3> {grid}};
    for (auto const& point : points) { block_bitmap_builder.add (point); }
    auto block_bitmap_built {std::move (block_bitmap_builder).build()};

    auto roaring_builder {Roaring<3> {grid}};
    for (auto const& point : points) { roaring_builder.add (point); }
    auto roaring_built {std::move (roaring_builder).build()};

    auto lex_run_builder {LexRun<3> {}};
    for (auto const& point : points) { lex_run_builder.add (point); }
    auto lex_run_built {std::move (lex_run_builder).build()};

    auto row_csr_k1_builder {RowCSR<3, 1> {}};
    for (auto const& point : points) { row_csr_k1_builder.add (point); }
    auto row_csr_k1_built {std::move (row_csr_k1_builder).build()};

    auto row_csr_k2_builder {RowCSR<3, 2> {}};
    for (auto const& point : points) { row_csr_k2_builder.add (point); }
    auto row_csr_k2_built {std::move (row_csr_k2_builder).build()};

    auto regular_ipt_built
      {std::move (create::Regular<3> {world}).build()};

    auto greedy_plus_combine_builder {create::GreedyPlusCombine<3> {}};
    for (auto const& point : points) {greedy_plus_combine_builder.add (point); }
    auto greedy_plus_combine_built
      {std::move (greedy_plus_combine_builder).build()};

    auto greedy_plus_merge_builder {create::GreedyPlusMerge<3> {}};
    for (auto const& point : points) { greedy_plus_merge_builder.add (point); }
    auto greedy_plus_merge_built
      {std::move (greedy_plus_merge_builder).build()};

    auto random_engine {std::mt19937_64 {0xDEADBEEF'1234ULL}};
    auto any_failed {false};
    constexpr auto query_count {std::size_t {200}};

    for ( auto query_index {std::size_t {0}}
        ; query_index < query_count
        ; ++query_index
        )
    {
      auto build_one
        { [&] (auto d) -> Ruler
          {
            auto const& ruler {world.ruler (d)};
            auto const ruler_count
              { static_cast<Index>
                  ((ruler.end() - ruler.begin()) / ruler.stride())
              };
            auto target_count
              { std::uniform_int_distribution<Index>
                  {Index {1}, ruler_count} (random_engine)
              };
            auto const max_start_index {ruler_count - target_count};
            auto const start_index
              { (max_start_index == 0)
                  ? Index {0}
                  : std::uniform_int_distribution<Index>
                      {Index {0}, max_start_index} (random_engine)
              };
            auto const begin
              { static_cast<Coordinate>
                  ( ruler.begin()
                  + static_cast<Coordinate> (start_index) * ruler.stride()
                  )
              };
            auto const end
              { static_cast<Coordinate>
                  ( begin
                  + static_cast<Coordinate> (target_count) * ruler.stride()
                  )
              };
            return Ruler {begin, ruler.stride(), end};
          }
        };
      auto const query
        { Cuboid<3>
          { std::array<Ruler, 3>
            {build_one (0), build_one (1), build_one (2)}
          }
        };

      auto const reference {materialize_points (sorted_points_built, query)};

      any_failed |= !check_one ("dense-bitset", dense_bitset_built, query, reference, query_index);
      any_failed |= !check_one ("block-bitmap", block_bitmap_built, query, reference, query_index);
      any_failed |= !check_one ("roaring", roaring_built, query, reference, query_index);
      any_failed |= !check_one ("lex-run", lex_run_built, query, reference, query_index);
      any_failed |= !check_one ("row-csr-k1", row_csr_k1_built, query, reference, query_index);
      any_failed |= !check_one ("row-csr-k2", row_csr_k2_built, query, reference, query_index);
      any_failed |= !check_one ("regular-ipt", regular_ipt_built, query, reference, query_index);
      any_failed |= !check_one ("greedy-plus-combine", greedy_plus_combine_built, query, reference, query_index);
      any_failed |= !check_one ("greedy-plus-merge", greedy_plus_merge_built, query, reference, query_index);
      any_failed |= !check_one ("grid", grid, query, reference, query_index);
    }

    if (any_failed)
    {
      throw std::runtime_error {"verify select_verify: FAIL"};
    }

    std::print
      ( "verify select_verify: OK {} queries, all impls agree\n"
      , query_count
      );
  }
}
#endif // !NDEBUG

#ifndef IPT_BENCHMARK_NO_MAIN
auto main() noexcept -> int try
{
  auto sink {std::uint64_t {0}};

#ifndef NDEBUG
  verify::run_intersect();
  verify::run_corner_steal();
  verify::run_select();
#endif

#ifdef NDEBUG
  auto const grids
    { std::array<Grid<3>, 4>
      { Grid<3>
        { Origin<3> {0, 0, 0}
        , Extents<3> {10, 10, 10}
        , Strides<3> {1, 1, 1}
        }
      , Grid<3>
        { Origin<3> {0, 0, 0}
        , Extents<3> {100, 10, 10}
        , Strides<3> {1, 1, 1}
        }
      , Grid<3>
        { Origin<3> {0, 0, 0}
        , Extents<3> {100, 100, 10}
        , Strides<3> {1, 1, 1}
        }
      , Grid<3>
        { Origin<3> {0, 0, 0}
        , Extents<3> {100, 100, 100}
        , Strides<3> {1, 1, 1}
        }
      }
    };
#else
  auto const grids
    { std::array<Grid<3>, 1>
      { Grid<3>
        { Origin<3> {0, 0, 0}
        , Extents<3> {10, 10, 10}
        , Strides<3> {1, 1, 1}
        }
      }
    };
#endif
  struct RandomScenario
  {
    char const* name;
    std::size_t point_percentage;
  };
#ifdef NDEBUG
  auto const random_scenarios
    { std::array<RandomScenario, 12>
      { RandomScenario {"random-1pct", 1}
      , RandomScenario {"random-2pct", 2}
      , RandomScenario {"random-5pct", 5}
      , RandomScenario {"random-10pct", 10}
      , RandomScenario {"random-25pct", 25}
      , RandomScenario {"random-50pct", 50}
      , RandomScenario {"random-75pct", 75}
      , RandomScenario {"random-90pct", 90}
      , RandomScenario {"random-95pct", 95}
      , RandomScenario {"random-98pct", 98}
      , RandomScenario {"random-99pct", 99}
      , RandomScenario {"random-100pct", 100}
      }
    };
#else
  auto const random_scenarios
    { std::array<RandomScenario, 3>
      { RandomScenario {"random-1pct", 1}
      , RandomScenario {"random-50pct", 50}
      , RandomScenario {"random-100pct", 100}
      }
    };
#endif
#ifdef NDEBUG
  auto const default_seed_count {std::size_t {30}};
  constexpr auto run_full_benchmark_suite {true};
#else
  auto const default_seed_count {std::size_t {1}};
  constexpr auto run_full_benchmark_suite {false};
#endif
  auto const seed_count
    { benchmark_selection().seed_count.value_or (default_seed_count)
    };
  auto seeds {std::vector<std::uint64_t> (seed_count)};
  std::ranges::generate (seeds, random_seed);

  auto const run_regular_scenario {selected_scenario ("regular")};
  auto const run_random_scenarios
    { std::ranges::any_of
        ( random_scenarios
        , [] (auto const& scenario) noexcept
          {
            return selected_scenario (scenario.name);
          }
        )
    };

  std::ranges::for_each
    ( grids
    , [&random_scenarios, &run_random_scenarios, &run_regular_scenario, &seeds, &sink]
        (Grid<3> const& grid) noexcept
      {
        if (!(run_regular_scenario || run_random_scenarios))
        {
          return;
        }

        std::ranges::for_each
          ( seeds
          , [&grid, &random_scenarios, &run_regular_scenario, &sink]
              (std::uint64_t seed) noexcept
            {
              auto const regular_data {Data<3> {grid}};

              if (run_regular_scenario)
              {
                run_scenario
                  ( regular_data
                  , BenchmarkContext<3>
                    { .grid = grid
                    , .seed = seed
                    , .scenario = "regular"
                    , .point_percentage = 100
                    }
                  , sink
                  , true
                  );
              }

              std::ranges::for_each
                ( random_scenarios
                , [&grid, seed, &regular_data, &sink]
                    ( RandomScenario const& scenario
                    ) noexcept
                  {
                    if (!selected_scenario (scenario.name))
                    {
                      return;
                    }

                    auto const random_data
                      { Data<3>
                        { grid
                        , RandomSample
                          { std::clamp
                            ( regular_data.size()
                                * scenario.point_percentage / 100
                            , std::size_t {1}
                            , regular_data.size()
                            )
                          , random_seed()
                          }
                        }
                      };
                    run_scenario
                      ( random_data
                      , BenchmarkContext<3>
                        { .grid = grid
                        , .seed = seed
                        , .scenario = scenario.name
                        , .point_percentage = scenario.point_percentage
                        }
                      , sink
                      , false
                      );
                  }
                );
            }
          );
      }
    );

  if (selected_scenario ("multiple-survey-2-l"))
  {
    auto const multiple_survey_two_l
      { Data<3>
        {
#ifdef NDEBUG
          Grid<3>
            { Origin<3> {0, 0, 0}
            , Extents<3> {360, 160, 1}
            , Strides<3> {1, 3, 1}
            }
        , Grid<3>
            { Origin<3> {0, 160, 0}
            , Extents<3> {60, 240, 1}
            , Strides<3> {3, 7, 1}
            }
#else
          Grid<3> { Origin<3> {0, 0, 0},   Extents<3> {4, 5, 1}, Strides<3> {1, 3, 1} }
        , Grid<3> { Origin<3> {0, 160, 0}, Extents<3> {3, 4, 1}, Strides<3> {3, 7, 1} }
#endif
        }
      };
    std::ranges::for_each
      ( seeds
      , [&multiple_survey_two_l, &sink]
          (std::uint64_t seed) noexcept
        {
          run_scenario
            ( multiple_survey_two_l
            , BenchmarkContext<3>
              { .grid = multiple_survey_two_l.bounding_grid()
              , .seed = seed
              , .scenario = "multiple-survey-2-l"
              , .point_percentage = 100
              }
            , sink
            , false
            );
        }
      );
  }

  if (run_full_benchmark_suite && selected_scenario ("multiple-survey-3-steps"))
  {
    auto const multiple_survey_three_steps
      { Data<3>
        {
#ifdef NDEBUG
          Grid<3>
            { Origin<3> {0, 0, 0}
            , Extents<3> {360, 240, 1}
            , Strides<3> {1, 1, 1}
            }
        , Grid<3>
            { Origin<3> {320, 60, 0}
            , Extents<3> {200, 280, 1}
            , Strides<3> {3, 5, 1}
            }
        , Grid<3>
            { Origin<3> {160, 360, 0}
            , Extents<3> {320, 160, 1}
            , Strides<3> {7, 2, 1}
            }
#else
          Grid<3> { Origin<3> {0, 0, 0},     Extents<3> {4, 5, 1}, Strides<3> {1, 1, 1} }
        , Grid<3> { Origin<3> {320, 60, 0},  Extents<3> {3, 4, 1}, Strides<3> {3, 5, 1} }
        , Grid<3> { Origin<3> {160, 360, 0}, Extents<3> {4, 3, 1}, Strides<3> {7, 2, 1} }
#endif
        }
      };
    std::ranges::for_each
      ( seeds
      , [&multiple_survey_three_steps, &sink]
          (std::uint64_t seed) noexcept
        {
          run_scenario
            ( multiple_survey_three_steps
            , BenchmarkContext<3>
              { .grid = multiple_survey_three_steps.bounding_grid()
              , .seed = seed
              , .scenario = "multiple-survey-3-steps"
              , .point_percentage = 100
              }
            , sink
            , false
            );
        }
      );
  }

  if (run_full_benchmark_suite && selected_scenario ("multiple-survey-4-overlap"))
  {
    auto const multiple_survey_four_overlap
      { Data<3>
        {
#ifdef NDEBUG
          Grid<3>
            { Origin<3> {0, 0, 0}
            , Extents<3> {400, 450, 1}
            , Strides<3> {1, 1, 1}
            }
        , Grid<3>
            { Origin<3> {240, 180, 0}
            , Extents<3> {440, 360, 1}
            , Strides<3> {2, 3, 1}
            }
        , Grid<3>
            { Origin<3> {120, 600, 0}
            , Extents<3> {360, 300, 1}
            , Strides<3> {5, 1, 1}
            }
        , Grid<3>
            { Origin<3> {560, 0, 0}
            , Extents<3> {240, 660, 1}
            , Strides<3> {3, 7, 1}
            }
#else
          Grid<3> { Origin<3> {0, 0, 0},    Extents<3> {4, 5, 1}, Strides<3> {1, 1, 1} }
        , Grid<3> { Origin<3> {240, 180, 0}, Extents<3> {3, 4, 1}, Strides<3> {2, 3, 1} }
        , Grid<3> { Origin<3> {120, 600, 0}, Extents<3> {4, 4, 1}, Strides<3> {5, 1, 1} }
        , Grid<3> { Origin<3> {560, 0, 0},  Extents<3> {3, 4, 1}, Strides<3> {3, 7, 1} }
#endif
        }
      };
    std::ranges::for_each
      ( seeds
      , [&multiple_survey_four_overlap, &sink]
          (std::uint64_t seed) noexcept
        {
          run_scenario
            ( multiple_survey_four_overlap
            , BenchmarkContext<3>
              { .grid = multiple_survey_four_overlap.bounding_grid()
              , .seed = seed
              , .scenario = "multiple-survey-4-overlap"
              , .point_percentage = 100
              }
            , sink
            , false
            );
        }
      );
  }

  if (run_full_benchmark_suite && selected_scenario ("multiple-survey-5-mixed"))
  {
    auto const multiple_survey_five_mixed
      { Data<3>
        {
#ifdef NDEBUG
          Grid<3>
            { Origin<3> {0, 0, 0}
            , Extents<3> {440, 660, 1}
            , Strides<3> {1, 1, 1}
            }
        , Grid<3>
            { Origin<3> {200, 300, 0}
            , Extents<3> {320, 540, 1}
            , Strides<3> {3, 7, 1}
            }
        , Grid<3>
            { Origin<3> {560, 120, 0}
            , Extents<3> {300, 480, 1}
            , Strides<3> {7, 3, 1}
            }
        , Grid<3>
            { Origin<3> {80, 900, 0}
            , Extents<3> {520, 360, 1}
            , Strides<3> {5, 9, 1}
            }
        , Grid<3>
            { Origin<3> {720, 660, 0}
            , Extents<3> {280, 420, 1}
            , Strides<3> {10, 3, 1}
            }
#else
          Grid<3> { Origin<3> {0, 0, 0},    Extents<3> {4, 5, 1}, Strides<3> {1, 1, 1}  }
        , Grid<3> { Origin<3> {200, 300, 0}, Extents<3> {3, 4, 1}, Strides<3> {3, 7, 1}  }
        , Grid<3> { Origin<3> {560, 120, 0}, Extents<3> {3, 4, 1}, Strides<3> {7, 3, 1}  }
        , Grid<3> { Origin<3> {80, 900, 0},  Extents<3> {4, 3, 1}, Strides<3> {5, 9, 1}  }
        , Grid<3> { Origin<3> {720, 660, 0}, Extents<3> {3, 4, 1}, Strides<3> {10, 3, 1} }
#endif
        }
      };
    std::ranges::for_each
      ( seeds
      , [&multiple_survey_five_mixed, &sink]
          (std::uint64_t seed) noexcept
        {
          run_scenario
            ( multiple_survey_five_mixed
            , BenchmarkContext<3>
              { .grid = multiple_survey_five_mixed.bounding_grid()
              , .seed = seed
              , .scenario = "multiple-survey-5-mixed"
              , .point_percentage = 100
              }
            , sink
            , false
            );
        }
      );
  }

  if (run_full_benchmark_suite && selected_scenario ("multiple-survey-6-bands"))
  {
    auto const multiple_survey_six_bands
      { Data<3>
        {
#ifdef NDEBUG
          Grid<3>
            { Origin<3> {0, 0, 0}
            , Extents<3> {640, 360, 1}
            , Strides<3> {1, 1, 1}
            }
        , Grid<3>
            { Origin<3> {520, 80, 0}
            , Extents<3> {520, 360, 1}
            , Strides<3> {2, 3, 1}
            }
        , Grid<3>
            { Origin<3> {160, 440, 0}
            , Extents<3> {520, 300, 1}
            , Strides<3> {5, 7, 1}
            }
        , Grid<3>
            { Origin<3> {760, 440, 0}
            , Extents<3> {440, 320, 1}
            , Strides<3> {9, 4, 1}
            }
        , Grid<3>
            { Origin<3> {0, 840, 0}
            , Extents<3> {600, 240, 1}
            , Strides<3> {4, 9, 1}
            }
        , Grid<3>
            { Origin<3> {640, 840, 0}
            , Extents<3> {480, 240, 1}
            , Strides<3> {7, 5, 1}
            }
#else
          Grid<3> { Origin<3> {0, 0, 0},    Extents<3> {4, 5, 1}, Strides<3> {1, 1, 1} }
        , Grid<3> { Origin<3> {520, 80, 0},  Extents<3> {3, 4, 1}, Strides<3> {2, 3, 1} }
        , Grid<3> { Origin<3> {160, 440, 0}, Extents<3> {4, 3, 1}, Strides<3> {5, 7, 1} }
        , Grid<3> { Origin<3> {760, 440, 0}, Extents<3> {3, 4, 1}, Strides<3> {9, 4, 1} }
        , Grid<3> { Origin<3> {0, 840, 0},   Extents<3> {4, 3, 1}, Strides<3> {4, 9, 1} }
        , Grid<3> { Origin<3> {640, 840, 0}, Extents<3> {3, 4, 1}, Strides<3> {7, 5, 1} }
#endif
        }
      };
    std::ranges::for_each
      ( seeds
      , [&multiple_survey_six_bands, &sink]
          (std::uint64_t seed) noexcept
        {
          run_scenario
            ( multiple_survey_six_bands
            , BenchmarkContext<3>
              { .grid = multiple_survey_six_bands.bounding_grid()
              , .seed = seed
              , .scenario = "multiple-survey-6-bands"
              , .point_percentage = 100
              }
            , sink
            , false
            );
        }
      );
  }

  if (run_full_benchmark_suite && selected_scenario ("multiple-survey-7-large"))
  {
    auto const multiple_survey_seven_large
      { Data<3>
        {
#ifdef NDEBUG
          Grid<3>
            { Origin<3> {0, 0, 0}
            , Extents<3> {560, 440, 1}
            , Strides<3> {1, 1, 1}
            }
        , Grid<3>
            { Origin<3> {440, 80, 0}
            , Extents<3> {520, 440, 1}
            , Strides<3> {3, 5, 1}
            }
        , Grid<3>
            { Origin<3> {80, 480, 0}
            , Extents<3> {520, 360, 1}
            , Strides<3> {5, 3, 1}
            }
        , Grid<3>
            { Origin<3> {640, 520, 0}
            , Extents<3> {440, 360, 1}
            , Strides<3> {7, 9, 1}
            }
        , Grid<3>
            { Origin<3> {160, 920, 0}
            , Extents<3> {480, 320, 1}
            , Strides<3> {9, 7, 1}
            }
        , Grid<3>
            { Origin<3> {720, 960, 0}
            , Extents<3> {440, 280, 1}
            , Strides<3> {10, 1, 1}
            }
        , Grid<3>
            { Origin<3> {320, 280, 0}
            , Extents<3> {520, 520, 1}
            , Strides<3> {2, 7, 1}
            }
#else
          Grid<3> { Origin<3> {0, 0, 0},    Extents<3> {4, 5, 1}, Strides<3> {1, 1, 1}  }
        , Grid<3> { Origin<3> {440, 80, 0},  Extents<3> {3, 4, 1}, Strides<3> {3, 5, 1}  }
        , Grid<3> { Origin<3> {80, 480, 0},  Extents<3> {4, 3, 1}, Strides<3> {5, 3, 1}  }
        , Grid<3> { Origin<3> {640, 520, 0}, Extents<3> {3, 4, 1}, Strides<3> {7, 9, 1}  }
        , Grid<3> { Origin<3> {160, 920, 0}, Extents<3> {4, 3, 1}, Strides<3> {9, 7, 1}  }
        , Grid<3> { Origin<3> {720, 960, 0}, Extents<3> {3, 5, 1}, Strides<3> {10, 1, 1} }
        , Grid<3> { Origin<3> {320, 280, 0}, Extents<3> {4, 4, 1}, Strides<3> {2, 7, 1}  }
#endif
        }
      };
    std::ranges::for_each
      ( seeds
      , [&multiple_survey_seven_large, &sink]
          (std::uint64_t seed) noexcept
        {
          run_scenario
            ( multiple_survey_seven_large
            , BenchmarkContext<3>
              { .grid = multiple_survey_seven_large.bounding_grid()
              , .seed = seed
              , .scenario = "multiple-survey-7-large"
              , .point_percentage = 100
              }
            , sink
            , false
            );
        }
      );
  }

  // True 3D scenario: 12 overlapping survey blocks in a 3×4 arrangement.
  // All blocks have non-trivial strides in every dimension; strides are
  // mildly different across blocks (x/y ∈ {2,3}, z ∈ {3,4,5}).  Adjacent
  // columns (x) and adjacent rows (y) share a narrow overlap band; blocks
  // that are far apart do not overlap at all.
  if (selected_scenario ("multiple-survey-8-threed"))
  {
    auto const multiple_survey_eight_threed
      { Data<3>
        {
#ifdef NDEBUG
          // Row 0
          Grid<3> { Origin<3> {  0,   0, 0}, Extents<3> {30, 25, 10}, Strides<3> {2, 3, 4} }
        , Grid<3> { Origin<3> { 52,   0, 0}, Extents<3> {30, 25, 10}, Strides<3> {3, 3, 5} }
        , Grid<3> { Origin<3> {132,   0, 0}, Extents<3> {30, 25, 10}, Strides<3> {2, 3, 3} }
          // Row 1 (y-origin 66, slight y-overlap with row 0 which ends at 72)
        , Grid<3> { Origin<3> {  0,  66, 0}, Extents<3> {30, 25, 10}, Strides<3> {3, 2, 5} }
        , Grid<3> { Origin<3> { 52,  66, 0}, Extents<3> {30, 25, 10}, Strides<3> {2, 2, 4} }
        , Grid<3> { Origin<3> {132,  66, 0}, Extents<3> {30, 25, 10}, Strides<3> {3, 2, 3} }
          // Row 2 (y-origin 108, slight y-overlap with row 1 which ends at 114)
        , Grid<3> { Origin<3> {  0, 108, 0}, Extents<3> {30, 25, 10}, Strides<3> {2, 3, 3} }
        , Grid<3> { Origin<3> { 52, 108, 0}, Extents<3> {30, 25, 10}, Strides<3> {3, 3, 4} }
        , Grid<3> { Origin<3> {132, 108, 0}, Extents<3> {30, 25, 10}, Strides<3> {2, 3, 5} }
          // Row 3 (y-origin 174, slight y-overlap with row 2 which ends at 180)
        , Grid<3> { Origin<3> {  0, 174, 0}, Extents<3> {30, 25, 10}, Strides<3> {3, 2, 4} }
        , Grid<3> { Origin<3> { 52, 174, 0}, Extents<3> {30, 25, 10}, Strides<3> {2, 2, 3} }
        , Grid<3> { Origin<3> {132, 174, 0}, Extents<3> {30, 25, 10}, Strides<3> {3, 2, 5} }
#else
          Grid<3> { Origin<3> {  0,   0, 0}, Extents<3> {3, 3, 3}, Strides<3> {2, 3, 4} }
        , Grid<3> { Origin<3> { 52,   0, 0}, Extents<3> {3, 3, 3}, Strides<3> {3, 3, 5} }
        , Grid<3> { Origin<3> {132,   0, 0}, Extents<3> {3, 3, 3}, Strides<3> {2, 3, 3} }
        , Grid<3> { Origin<3> {  0,  66, 0}, Extents<3> {3, 3, 3}, Strides<3> {3, 2, 5} }
        , Grid<3> { Origin<3> { 52,  66, 0}, Extents<3> {3, 3, 3}, Strides<3> {2, 2, 4} }
        , Grid<3> { Origin<3> {132,  66, 0}, Extents<3> {3, 3, 3}, Strides<3> {3, 2, 3} }
        , Grid<3> { Origin<3> {  0, 108, 0}, Extents<3> {3, 3, 3}, Strides<3> {2, 3, 3} }
        , Grid<3> { Origin<3> { 52, 108, 0}, Extents<3> {3, 3, 3}, Strides<3> {3, 3, 4} }
        , Grid<3> { Origin<3> {132, 108, 0}, Extents<3> {3, 3, 3}, Strides<3> {2, 3, 5} }
        , Grid<3> { Origin<3> {  0, 174, 0}, Extents<3> {3, 3, 3}, Strides<3> {3, 2, 4} }
        , Grid<3> { Origin<3> { 52, 174, 0}, Extents<3> {3, 3, 3}, Strides<3> {2, 2, 3} }
        , Grid<3> { Origin<3> {132, 174, 0}, Extents<3> {3, 3, 3}, Strides<3> {3, 2, 5} }
#endif
        }
      };
    std::ranges::for_each
      ( seeds
      , [&multiple_survey_eight_threed, &sink]
          (std::uint64_t seed) noexcept
        {
          run_scenario
            ( multiple_survey_eight_threed
            , BenchmarkContext<3>
              { .grid = multiple_survey_eight_threed.bounding_grid()
              , .seed = seed
              , .scenario = "multiple-survey-8-threed"
              , .point_percentage = 100
              }
            , sink
            , false
            );
        }
      );
  }

  // 5D scenario derived from the true-3D overlap case above.  The first three
  // dimensions keep the same 3x4 overlapping block layout with mildly varying
  // strides; each 3D point is expanded by a fixed dense 125x80 grid in the
  // last two dimensions.  In release builds this yields 108M raw points before
  // duplicate removal from the deliberate 3D overlaps.
  if (selected_scenario ("multiple-survey-9-5d"))
  {
    auto const multiple_survey_nine_five_d
      { Data<5>
        {
#ifdef NDEBUG
          // Row 0
          Grid<5> { Origin<5> { 0,  0, 0, 0, 0}, Extents<5> {10, 10, 9, 125, 80}, Strides<5> {2, 3, 4, 1, 1} }
        , Grid<5> { Origin<5> { 8,  0, 0, 0, 0}, Extents<5> {10, 10, 9, 125, 80}, Strides<5> {3, 3, 5, 1, 1} }
        , Grid<5> { Origin<5> {19,  0, 0, 0, 0}, Extents<5> {10, 10, 9, 125, 80}, Strides<5> {2, 3, 3, 1, 1} }
          // Row 1 (y-origin 9, slight y-overlap with row 0)
        , Grid<5> { Origin<5> { 0,  9, 0, 0, 0}, Extents<5> {10, 10, 9, 125, 80}, Strides<5> {3, 2, 5, 1, 1} }
        , Grid<5> { Origin<5> { 8,  9, 0, 0, 0}, Extents<5> {10, 10, 9, 125, 80}, Strides<5> {2, 2, 4, 1, 1} }
        , Grid<5> { Origin<5> {19,  9, 0, 0, 0}, Extents<5> {10, 10, 9, 125, 80}, Strides<5> {3, 2, 3, 1, 1} }
          // Row 2 (y-origin 16, slight y-overlap with row 1)
        , Grid<5> { Origin<5> { 0, 16, 0, 0, 0}, Extents<5> {10, 10, 9, 125, 80}, Strides<5> {2, 3, 3, 1, 1} }
        , Grid<5> { Origin<5> { 8, 16, 0, 0, 0}, Extents<5> {10, 10, 9, 125, 80}, Strides<5> {3, 3, 4, 1, 1} }
        , Grid<5> { Origin<5> {19, 16, 0, 0, 0}, Extents<5> {10, 10, 9, 125, 80}, Strides<5> {2, 3, 5, 1, 1} }
          // Row 3 (y-origin 25, slight y-overlap with row 2)
        , Grid<5> { Origin<5> { 0, 25, 0, 0, 0}, Extents<5> {10, 10, 9, 125, 80}, Strides<5> {3, 2, 4, 1, 1} }
        , Grid<5> { Origin<5> { 8, 25, 0, 0, 0}, Extents<5> {10, 10, 9, 125, 80}, Strides<5> {2, 2, 3, 1, 1} }
        , Grid<5> { Origin<5> {19, 25, 0, 0, 0}, Extents<5> {10, 10, 9, 125, 80}, Strides<5> {3, 2, 5, 1, 1} }
#else
          Grid<5> { Origin<5> { 0,  0, 0, 0, 0}, Extents<5> {2, 2, 2, 5, 4}, Strides<5> {2, 3, 4, 1, 1} }
        , Grid<5> { Origin<5> { 8,  0, 0, 0, 0}, Extents<5> {2, 2, 2, 5, 4}, Strides<5> {3, 3, 5, 1, 1} }
        , Grid<5> { Origin<5> {19,  0, 0, 0, 0}, Extents<5> {2, 2, 2, 5, 4}, Strides<5> {2, 3, 3, 1, 1} }
        , Grid<5> { Origin<5> { 0,  9, 0, 0, 0}, Extents<5> {2, 2, 2, 5, 4}, Strides<5> {3, 2, 5, 1, 1} }
        , Grid<5> { Origin<5> { 8,  9, 0, 0, 0}, Extents<5> {2, 2, 2, 5, 4}, Strides<5> {2, 2, 4, 1, 1} }
        , Grid<5> { Origin<5> {19,  9, 0, 0, 0}, Extents<5> {2, 2, 2, 5, 4}, Strides<5> {3, 2, 3, 1, 1} }
        , Grid<5> { Origin<5> { 0, 16, 0, 0, 0}, Extents<5> {2, 2, 2, 5, 4}, Strides<5> {2, 3, 3, 1, 1} }
        , Grid<5> { Origin<5> { 8, 16, 0, 0, 0}, Extents<5> {2, 2, 2, 5, 4}, Strides<5> {3, 3, 4, 1, 1} }
        , Grid<5> { Origin<5> {19, 16, 0, 0, 0}, Extents<5> {2, 2, 2, 5, 4}, Strides<5> {2, 3, 5, 1, 1} }
        , Grid<5> { Origin<5> { 0, 25, 0, 0, 0}, Extents<5> {2, 2, 2, 5, 4}, Strides<5> {3, 2, 4, 1, 1} }
        , Grid<5> { Origin<5> { 8, 25, 0, 0, 0}, Extents<5> {2, 2, 2, 5, 4}, Strides<5> {2, 2, 3, 1, 1} }
        , Grid<5> { Origin<5> {19, 25, 0, 0, 0}, Extents<5> {2, 2, 2, 5, 4}, Strides<5> {3, 2, 5, 1, 1} }
#endif
        }
      };
    std::ranges::for_each
      ( seeds
      , [&multiple_survey_nine_five_d, &sink]
          (std::uint64_t seed) noexcept
        {
          run_scenario
            ( multiple_survey_nine_five_d
            , BenchmarkContext<5>
              { .grid = multiple_survey_nine_five_d.bounding_grid()
              , .seed = seed
              , .scenario = "multiple-survey-9-5d"
              , .point_percentage = 100
              }
            , sink
            , false
            );
        }
      );
  }

  if constexpr (run_full_benchmark_suite)
  {
    // Larger true 3D scenario: 16 survey blocks in a 4x4 arrangement.
    // Extents vary mildly by row and column, strides stay in the same
    // low-integer range as scenario 8, and neighboring blocks overlap only
    // slightly.
    if (selected_scenario ("multiple-survey-10-sixteen"))
    {
      auto const multiple_survey_ten_sixteen
        { make_data
          ( make_rectangular_three_d_survey_grids
            ( std::array<Coordinate, 4> {0, 138, 304, 492}
            , std::array<Coordinate, 4> {0, 118, 254, 406}
            , std::array<Coordinate, 4> {62, 58, 64, 60}
            , std::array<Coordinate, 4> {52, 48, 54, 50}
            , std::array<Coordinate, 4> {18, 20, 19, 21}
            , std::array<Coordinate, 4> {2, 3, 2, 3}
            , std::array<Coordinate, 4> {2, 3, 2, 3}
            , std::array<Coordinate, 4> {4, 5, 3, 4}
            )
          )
        };

      std::ranges::for_each
        ( seeds
        , [&multiple_survey_ten_sixteen, &sink]
            (std::uint64_t seed) noexcept
          {
            run_scenario
              ( multiple_survey_ten_sixteen
              , BenchmarkContext<3>
                { .grid = multiple_survey_ten_sixteen.bounding_grid()
                , .seed = seed
                , .scenario = "multiple-survey-10-sixteen"
                , .point_percentage = 100
                }
              , sink
              , false
              );
          }
        );
    }

    // Even larger true 3D scenario: 36 survey blocks in a 6x6 arrangement.
    // The block sizes and strides vary mildly across rows and columns, and the
    // layout keeps only small intended overlaps between neighbors.
    if (selected_scenario ("multiple-survey-11-thirtysix"))
    {
      auto const multiple_survey_eleven_thirtysix
        { make_data
          ( make_rectangular_three_d_survey_grids
            ( std::array<Coordinate, 6> {0, 150, 405, 563, 824, 986}
            , std::array<Coordinate, 6> {0, 152, 385, 543, 794, 964}
            , std::array<Coordinate, 6> {80, 86, 82, 88, 84, 90}
            , std::array<Coordinate, 6> {72, 78, 74, 80, 76, 82}
            , std::array<Coordinate, 6> {36, 40, 38, 42, 39, 43}
            , std::array<Coordinate, 6> {2, 3, 2, 3, 2, 3}
            , std::array<Coordinate, 6> {2, 3, 2, 3, 2, 3}
            , std::array<Coordinate, 6> {4, 5, 3, 4, 5, 3}
            )
          )
        };

      std::ranges::for_each
        ( seeds
        , [&multiple_survey_eleven_thirtysix, &sink]
            (std::uint64_t seed) noexcept
          {
            run_scenario
              ( multiple_survey_eleven_thirtysix
              , BenchmarkContext<3>
                { .grid = multiple_survey_eleven_thirtysix.bounding_grid()
                , .seed = seed
                , .scenario = "multiple-survey-11-thirtysix"
                , .point_percentage = 100
                }
              , sink
              , false
              );
          }
        );
    }

    // Much larger true 3D scenario: 49 survey blocks in a 7x7 arrangement.
    // Extents and strides vary mildly across rows and columns, while the block
    // origins leave only small intended overlaps between neighboring blocks.
    if (selected_scenario ("multiple-survey-12-fortynine"))
    {
      auto const multiple_survey_twelve_fortynine
        { make_data
          ( make_rectangular_three_d_survey_grids
            ( std::array<Coordinate, 7> {0, 340, 874, 1210, 1740, 2088, 2628}
            , std::array<Coordinate, 7> {0, 296, 764, 1058, 1520, 1824, 2296}
            , std::array<Coordinate, 7> {180, 186, 182, 188, 184, 190, 178}
            , std::array<Coordinate, 7> {158, 164, 160, 166, 162, 168, 156}
            , std::array<Coordinate, 7> {68, 72, 70, 74, 71, 73, 69}
            , std::array<Coordinate, 7> {2, 3, 2, 3, 2, 3, 2}
            , std::array<Coordinate, 7> {2, 3, 2, 3, 2, 3, 2}
            , std::array<Coordinate, 7> {4, 5, 3, 4, 5, 3, 4}
            )
          )
        };

      std::ranges::for_each
        ( seeds
        , [&multiple_survey_twelve_fortynine, &sink]
            (std::uint64_t seed) noexcept
          {
            run_scenario
              ( multiple_survey_twelve_fortynine
              , BenchmarkContext<3>
                { .grid = multiple_survey_twelve_fortynine.bounding_grid()
                , .seed = seed
                , .scenario = "multiple-survey-12-fortynine"
                , .point_percentage = 100
                }
              , sink
              , false
              );
          }
        );
    }
  }

  return EXIT_SUCCESS;
}
catch (std::exception const& exception)
{
  std::print (stderr, "benchmark failed: {}\n", exception.what());

  return EXIT_FAILURE;
}
catch (...)
{
  std::print (stderr, "benchmark failed: UNKNOWN\n");

  return EXIT_FAILURE;
}
#endif // IPT_BENCHMARK_NO_MAIN
