// Shared benchmark infrastructure.
//
// This header is the bulk of the former monolithic benchmark.cpp.
// Each per-scenario binary in src/benchmark/scenarios/X.cpp includes
// this header and defines its own main().  The header has no env-
// variable input; each binary chooses scenario, seed_count and
// all_query_cap as compile-time constants and selects which
// algorithms to run by calling either run_scenario_cache_dependent
// or run_scenario_cache_independent (see bottom of file).
//
// The anonymous namespace produces a fresh internal copy per TU,
// which matches the original layout.  std::formatter specialisations
// for anonymous-namespace types (BenchmarkContext<D>, Measurement<D>,
// TimedQueries<D, ...>) live in this header and are therefore also
// per-TU; that is fine because each binary is its own TU.
//
#pragma once

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
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <baseline/BlockBitmap/MMap.hpp>
#include <baseline/BlockBitmap/Write.hpp>
#include <baseline/DenseBitset/MMap.hpp>
#include <baseline/DenseBitset/Write.hpp>
#include <baseline/Grid/Header.hpp>
#include <baseline/Grid/Write.hpp>
#include <baseline/LexRun/MMap.hpp>
#include <baseline/LexRun/Write.hpp>
#include <baseline/Roaring/MMap.hpp>
#include <baseline/Roaring/Write.hpp>
#include <baseline/RowCSR/MMap.hpp>
#include <baseline/RowCSR/Write.hpp>
#include <baseline/SortedPoints/MMap.hpp>
#include <baseline/SortedPoints/Write.hpp>
#include <generator>
#include <ipt/Coordinate.hpp>
#include <ipt/Cuboid.hpp>
#include <ipt/Entry.hpp>
#include <ipt/IPT.hpp>
#include <ipt/Index.hpp>
#include <ipt/Point.hpp>
#include <ipt/Ruler.hpp>
#include <ipt/storage/MMap.hpp>
#include <ipt/storage/Write.hpp>
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
#include <fcntl.h>
#include <unistd.h>
#include <utility>
#include <vector>
#include <baseline/BlockBitmap/BlockBitmap.hpp>
#include <baseline/DenseBitset/DenseBitset.hpp>
#include <baseline/Extents.hpp>
#include <baseline/Grid/Grid.hpp>
#include <baseline/LexRun/LexRun.hpp>
#include <baseline/Origin.hpp>
#include <baseline/Roaring/Roaring.hpp>
#include <baseline/RowCSR/RowCSR.hpp>
#include <baseline/SortedPoints/SortedPoints.hpp>
#include <baseline/Strides.hpp>
#include <baseline/enumerate.hpp>
#include <baseline/make_cuboid.hpp>
#include <baseline/zeros_ones.hpp>

namespace
{
  using namespace ipt;
  using namespace ipt::baseline;

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

}

namespace
{
  using Clock = std::chrono::steady_clock;

  using namespace ipt;
  using namespace ipt::baseline;


  // ---- storage benchmark plumbing (no env vars) ---------------------
  //
  // The storage benchmark is a separate binary (src/benchmark/scenarios/
  // storage.cpp) that calls run_storage_scenario directly.  These
  // helpers are shared with that binary and used to write the
  // serialized blobs to a temporary directory and to drop the page
  // cache between write and read.  The directory defaults to
  // /dev/shm/ipt-storage-bench but the storage binary may override it
  // by passing an explicit path.
  //
  // On tmpfs (/dev/shm) posix_fadvise(POSIX_FADV_DONTNEED) is a no-op
  // because tmpfs pages ARE the file content; we still issue it so
  // the same code path drops the page cache correctly on a real
  // filesystem.
  //
  inline constexpr std::string_view default_storage_directory
    {"/dev/shm/ipt-storage-bench"};

  [[nodiscard]] inline auto ensure_storage_directory
    ( std::filesystem::path const& path
    ) -> std::filesystem::path
  {
    std::filesystem::create_directories (path);
    return path;
  }

  // Drop file pages from the page cache.  No-op on tmpfs.
  inline auto drop_page_cache
    ( std::filesystem::path const& path
    ) -> void
  {
    auto const fd {::open (path.c_str(), O_RDONLY | O_CLOEXEC)};
    if (fd < 0)
    {
      return;
    }
    (void) ::posix_fadvise (fd, 0, 0, POSIX_FADV_DONTNEED);
    ::close (fd);
  }

  template<typename Callable>
    [[nodiscard]] auto time_once
      ( Callable&& callable
      ) -> Clock::duration
  {
    auto const start {Clock::now()};
    std::forward<Callable> (callable)();
    auto const stop {Clock::now()};
    return stop - start;
  }

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

  template<typename T, std::size_t D>
    [[nodiscard]] constexpr auto filled_array
      ( T value
      ) noexcept -> std::array<T, D>
  {
    auto result {std::array<T, D> {}};
    std::ranges::fill (result, value);
    return result;
  }

  template<std::size_t D>
    struct RestrictStencilSpec
    {
      std::string_view name;
      std::array<Index, D> left_radii;
      std::array<Index, D> right_radii;
    };

  struct RestrictDistributionSpec
  {
    std::string_view name;
    std::size_t axis_count;
    std::array<std::size_t, 2> unit_counts;
  };

  template<std::size_t D>
    auto restrict_stencil_specs() noexcept
      -> std::span<RestrictStencilSpec<D> const>
  {
    static auto const specs
      { [] ()
        {
          auto const symmetric
            { [] (std::string_view name, Index radius)
                {
                  return RestrictStencilSpec<D>
                    { name
                    , filled_array<Index, D> (radius)
                    , filled_array<Index, D> (radius)
                    };
                }
            };
          auto const slowest_halo
            { [] (std::string_view name, Index radius)
                {
                  auto left_radii {filled_array<Index, D> (Index {0})};
                  auto right_radii {filled_array<Index, D> (Index {0})};

                  left_radii[0] = radius;

                  return RestrictStencilSpec<D>
                    { name
                    , std::move (left_radii)
                    , std::move (right_radii)
                    };
                }
            };
          auto const slowest_elongated
            { []
                ( std::string_view name
                , Index slow_radius
                , Index fast_radius
                )
                {
                  auto left_radii {filled_array<Index, D> (fast_radius)};
                  auto right_radii {filled_array<Index, D> (fast_radius)};

                  left_radii[0] = slow_radius;
                  right_radii[0] = slow_radius;

                  return RestrictStencilSpec<D>
                    { name
                    , std::move (left_radii)
                    , std::move (right_radii)
                    };
                }
            };

          return std::to_array<RestrictStencilSpec<D>>
            ( { symmetric ("restrict_stencil_sym_r1", Index {1})
              , symmetric ("restrict_stencil_sym_r2", Index {2})
              , symmetric ("restrict_stencil_sym_r3", Index {3})
              , symmetric ("restrict_stencil_sym_r4", Index {4})
              , symmetric ("restrict_stencil_sym_r5", Index {5})
              , symmetric ("restrict_stencil_sym_r8", Index {8})
              , slowest_halo
                  ("restrict_stencil_halo_slowest_r4", Index {4})
              , slowest_elongated
                  ( "restrict_stencil_elongated_slowest_r8_fast_r2"
                  , Index {8}
                  , Index {2}
                  )
              }
            );
        }()
      };

    return specs;
  }

  auto restrict_distribution_specs() noexcept
    -> std::span<RestrictDistributionSpec const>
  {
    static auto const specs
      { std::to_array<RestrictDistributionSpec>
        ( { {"restrict_distribution_1d_p2", 1, {2, 1}}
          , {"restrict_distribution_1d_p4", 1, {4, 1}}
          , {"restrict_distribution_1d_p8", 1, {8, 1}}
          , {"restrict_distribution_1d_p16", 1, {16, 1}}
          , {"restrict_distribution_1d_p32", 1, {32, 1}}
          , {"restrict_distribution_1d_p56", 1, {56, 1}}
          , {"restrict_distribution_1d_p64", 1, {64, 1}}
          , {"restrict_distribution_1d_p128", 1, {128, 1}}
          , {"restrict_distribution_1d_p192", 1, {192, 1}}
          , {"restrict_distribution_1d_p256", 1, {256, 1}}
          , {"restrict_distribution_1d_p512", 1, {512, 1}}
          , {"restrict_distribution_1d_p1024", 1, {1024, 1}}
          , {"restrict_distribution_2d_p4_2x2", 2, {2, 2}}
          , {"restrict_distribution_2d_p16_4x4", 2, {4, 4}}
          , {"restrict_distribution_2d_p56_7x8", 2, {7, 8}}
          , {"restrict_distribution_2d_p64_8x8", 2, {8, 8}}
          , {"restrict_distribution_2d_p192_12x16", 2, {12, 16}}
          , {"restrict_distribution_2d_p256_16x16", 2, {16, 16}}
          , {"restrict_distribution_2d_p1024_32x32", 2, {32, 32}}
          }
        )
      };

    return specs;
  }

  [[nodiscard]] auto effective_partition_count
    ( Ruler const& ruler
    , std::size_t requested
    ) noexcept -> std::size_t
  {
    auto const available {static_cast<std::size_t> (ruler.size())};

    return std::min (requested, available);
  }

  [[nodiscard]] auto make_partition_ruler
    ( Ruler const& ruler
    , std::size_t part_count
    , std::size_t part_index
    ) -> Ruler
  {
    auto const total_count {static_cast<std::size_t> (ruler.size())};
    auto const begin_index {part_index * total_count / part_count};
    auto const end_index {(part_index + 1) * total_count / part_count};
    auto const begin
      { ruler.begin()
      + static_cast<Coordinate> (begin_index) * ruler.stride()
      };
    auto const end
      { ruler.begin()
      + static_cast<Coordinate> (end_index) * ruler.stride()
      };

    return Ruler {begin, ruler.stride(), end};
  }

  template<std::size_t D>
    [[nodiscard]] auto make_stencil_query
      ( Cuboid<D> const& world
      , Point<D> const& center
      , RestrictStencilSpec<D> const& spec
      ) -> Cuboid<D>
  {
    auto const rulers
      { std::invoke
          ( [&]<std::size_t... Dimensions>
              (std::index_sequence<Dimensions...>)
            {
              return std::array<Ruler, D>
                { [&] ()
                    {
                      auto const& ruler {world.ruler (Dimensions)};
                      auto const center_index
                        {ruler.pos (center[Dimensions])};
                      auto const left_radius
                        { std::min
                          ( spec.left_radii[Dimensions]
                          , center_index
                          )
                        };
                      auto const available_right
                        {ruler.size() - center_index - Index {1}};
                      auto const right_radius
                        { std::min
                          ( spec.right_radii[Dimensions]
                          , available_right
                          )
                        };
                      auto const begin_index {center_index - left_radius};
                      auto const end_index
                        {center_index + right_radius + Index {1}};
                      auto const begin
                        { ruler.begin()
                        + static_cast<Coordinate> (begin_index)
                          * ruler.stride()
                        };
                      auto const end
                        { ruler.begin()
                        + static_cast<Coordinate> (end_index)
                          * ruler.stride()
                        };

                      return Ruler {begin, ruler.stride(), end};
                    }()...
                };
            }
          , std::make_index_sequence<D> {}
          )
      };

    return Cuboid<D> {std::move (rulers)};
  }

  template<std::size_t D>
    [[nodiscard]] auto make_distribution_queries
      ( Cuboid<D> const& world
      , RestrictDistributionSpec const& spec
      ) -> std::vector<Cuboid<D>>
  {
    auto effective_units {std::array<std::size_t, 2> {1, 1}};

    effective_units[0]
      = effective_partition_count (world.ruler (0), spec.unit_counts[0]);

    if (spec.axis_count == 2)
    {
      effective_units[1]
        = effective_partition_count (world.ruler (1), spec.unit_counts[1]);
    }

    auto queries {std::vector<Cuboid<D>> {}};
    queries.reserve (effective_units[0] * effective_units[1]);

    for (auto part0 : std::views::iota (std::size_t {0}, effective_units[0]))
    {
      for (auto part1 : std::views::iota (std::size_t {0}, effective_units[1]))
      {
        auto const rulers
          { std::invoke
              ( [&]<std::size_t... Dimensions>
                  (std::index_sequence<Dimensions...>)
                {
                  return std::array<Ruler, D>
                    { [&] ()
                        {
                          if constexpr (Dimensions == 0)
                          {
                            return make_partition_ruler
                              (world.ruler (0), effective_units[0], part0);
                          }

                          if constexpr (Dimensions == 1)
                          {
                            if (spec.axis_count == 2)
                            {
                              return make_partition_ruler
                                ( world.ruler (1)
                                , effective_units[1]
                                , part1
                                );
                            }
                          }

                          return world.ruler (Dimensions);
                        }()...
                    };
                }
              , std::make_index_sequence<D> {}
              )
          };

        queries.push_back (Cuboid<D> {std::move (rulers)});
      }
    }

    return queries;
  }

  [[nodiscard]] constexpr auto restrict_small_sample
    ( std::size_t operation_count
    ) noexcept -> bool
  {
    return operation_count < 100;
  }

  template<std::size_t D>
    auto emit_restrict_measurement
      ( BenchmarkContext<D> const& context
      , std::string_view algorithm
      , std::string_view metric
      , Timing const& timing
      , std::size_t query_count
      , std::size_t nonempty_queries
      , Index total_points
      , std::size_t total_bytes
      ) -> void
  {
    auto const query_metric {std::format ("{}_queries", metric)};
    auto const nonempty_metric {std::format ("{}_nonempty", metric)};
    auto const points_metric {std::format ("{}_points", metric)};
    auto const bytes_metric {std::format ("{}_bytes", metric)};
    auto const total_ns_metric {std::format ("{}_total_ns", metric)};
    auto const total_nanoseconds
      { std::chrono::duration_cast<std::chrono::nanoseconds>
          (timing.duration())
          .count()
      };

    std::print
      ( "{} {}\n"
      , Measurement<D> {context, algorithm, metric}
      , timing
      );
    std::print
      ( "{} value={}\n"
      , Measurement<D> {context, algorithm, query_metric}
      , query_count
      );
    std::print
      ( "{} value={}\n"
      , Measurement<D> {context, algorithm, nonempty_metric}
      , nonempty_queries
      );
    std::print
      ( "{} value={}\n"
      , Measurement<D> {context, algorithm, points_metric}
      , total_points
      );
    std::print
      ( "{} value={}\n"
      , Measurement<D> {context, algorithm, bytes_metric}
      , total_bytes
      );
    std::print
      ( "{} value={}\n"
      , Measurement<D> {context, algorithm, total_ns_metric}
      , total_nanoseconds
      );
  }

  template<std::size_t D>
    struct RestrictTotals
  {
    std::size_t nonempty_queries;
    Index total_points;
    std::size_t total_bytes;
    std::uint64_t checksum;
  };

  template<std::size_t D>
    struct TimedRestrictMeasurement
  {
    Timing timing;
    RestrictTotals<D> totals;
  };

  template<std::size_t D>
    [[nodiscard]] constexpr auto point_checksum
      ( Point<D> const& point
      ) noexcept -> std::uint64_t
  {
    return std::ranges::fold_left
      ( std::views::iota (std::size_t {0}, D)
      , std::uint64_t {0}
      , [&] (std::uint64_t sum, std::size_t dimension) noexcept
        {
          return (sum * 0x9e3779b97f4a7c15ULL)
            + static_cast<std::uint64_t>
                (point[static_cast<Index> (dimension)]);
        }
      );
  }

  template<std::size_t D, typename Restricted>
    [[nodiscard]] auto full_materialization_checksum
      ( Restricted const& restricted
      ) -> std::uint64_t
  {
    return std::ranges::fold_left
      ( std::views::iota (Index {0}, restricted.size())
      , std::uint64_t {0}
      , [&] (std::uint64_t sum, Index index)
        {
          auto const point {restricted.at (index)};
          return sum + point_checksum<D> (point);
        }
      );
  }

  template<std::size_t D, typename Structure, std::ranges::input_range Queries>
    [[nodiscard]] auto collect_restrict_totals
      ( Structure const& structure
      , Queries&& queries
      , bool allow_empty
      , std::string_view empty_message
      , bool fully_materialize
      ) -> RestrictTotals<D>
  {
    auto totals
      { RestrictTotals<D>
        { .nonempty_queries = std::size_t {0}
        , .total_points = Index {0}
        , .total_bytes = std::size_t {0}
        , .checksum = std::uint64_t {0}
        }
      };

    std::ranges::for_each
      ( std::forward<Queries> (queries)
      , [&] (auto const& query)
        {
          auto const restricted {structure.restrict (query)};

          if (!restricted)
          {
            if (allow_empty)
            {
              return;
            }

            throw std::runtime_error {std::string {empty_message}};
          }

          totals.nonempty_queries += 1;
          totals.total_points += restricted->size();
          totals.total_bytes += restricted->bytes();
          totals.checksum += static_cast<std::uint64_t> (restricted->size());

          if (fully_materialize)
          {
            totals.checksum += full_materialization_checksum<D> (*restricted);
          }
        }
      );

    return totals;
  }

  template<std::size_t D, std::invocable Evaluate>
    [[nodiscard]] auto time_restrict_measurement
      ( std::uint64_t& sink
      , std::size_t query_count
      , Evaluate&& evaluate
      , bool warmup
      ) -> TimedRestrictMeasurement<D>
  {
    if (warmup)
    {
      auto const warmup_totals {std::invoke (evaluate)};
      sink ^= warmup_totals.checksum + 0x9e3779b97f4a7c15ULL;
    }

    auto const start {Clock::now()};
    auto const totals {std::invoke (evaluate)};
    auto const stop {Clock::now()};
    sink ^= totals.checksum + 0x9e3779b97f4a7c15ULL;

    return TimedRestrictMeasurement<D>
      { Timing {stop - start, query_count, "restrict"}
      , std::move (totals)
      };
  }

  template<std::size_t D>
    auto emit_timed_restrict_measurement
      ( BenchmarkContext<D> const& context
      , std::string_view algorithm
      , std::string_view metric
      , TimedRestrictMeasurement<D> const& measurement
      ) -> void
  {
    emit_restrict_measurement
      ( context
      , algorithm
      , metric
      , measurement.timing
      , measurement.timing.operation_count()
      , measurement.totals.nonempty_queries
      , measurement.totals.total_points
      , measurement.totals.total_bytes
      );
  }

  template<std::size_t D, typename Structure>
    auto run_restrict_stencil_workloads
      ( BenchmarkContext<D> const& context
      , std::string_view algorithm
      , Structure const& structure
      , std::span<Point<D> const> reference_points
      , std::uint64_t& sink
      ) -> void
  {
    if (reference_points.empty())
    {
      return;
    }

    auto const world {to_ipt_cuboid (context.grid)};

    std::ranges::for_each
      ( restrict_stencil_specs<D>()
      , [&] (auto const& spec)
        {
          auto const query_count {reference_points.size()};
          auto const empty_message
            { std::format
                ( "{}::restrict returned empty for stencil {}"
                , algorithm
                , spec.name
                )
            };
          auto const make_queries
            { [&]
                {
                  return std::views::transform
                    ( reference_points
                    , [&] (auto const& point)
                      {
                        return make_stencil_query<D> (world, point, spec);
                      }
                    );
                }
            };
          auto const first_query
            { std::array<Cuboid<D>, 1>
              { make_stencil_query<D>
                  (world, reference_points.front(), spec)
              }
            };
          auto const first_metric
            {std::format ("{}_first_response", spec.name)};
          auto const first_measurement
            { time_restrict_measurement<D>
              ( sink
              , std::size_t {1}
              , [&]
                {
                  return collect_restrict_totals<D>
                    ( structure
                    , first_query
                    , false
                    , empty_message
                    , false
                    );
                }
              , false
              )
            };
          auto const steady_measurement
            { time_restrict_measurement<D>
              ( sink
              , query_count
              , [&]
                {
                  return collect_restrict_totals<D>
                    ( structure
                    , make_queries()
                    , false
                    , empty_message
                    , false
                    );
                }
              , restrict_small_sample (query_count)
              )
            };
          auto const full_metric
            {std::format ("{}_full_materialization", spec.name)};
          auto const full_measurement
            { time_restrict_measurement<D>
              ( sink
              , query_count
              , [&]
                {
                  return collect_restrict_totals<D>
                    ( structure
                    , make_queries()
                    , false
                    , empty_message
                    , true
                    );
                }
              , restrict_small_sample (query_count)
              )
            };

          emit_timed_restrict_measurement
            (context, algorithm, first_metric, first_measurement);
          emit_timed_restrict_measurement
            (context, algorithm, spec.name, steady_measurement);
          emit_timed_restrict_measurement
            (context, algorithm, full_metric, full_measurement);
        }
      );
  }

  template<std::size_t D, typename Structure>
    auto run_restrict_distribution_workloads
      ( BenchmarkContext<D> const& context
      , std::string_view algorithm
      , Structure const& structure
      , std::span<Point<D> const> reference_points
      , std::uint64_t& sink
      ) -> void
  {
    if (reference_points.empty())
    {
      return;
    }

    auto const world {to_ipt_cuboid (context.grid)};

    std::ranges::for_each
      ( restrict_distribution_specs()
      , [&] (auto const& spec)
        {
          if (spec.axis_count == 2 && ! (D > 1))
          {
            return;
          }

          auto const queries {make_distribution_queries<D> (world, spec)};
          auto const query_count {queries.size()};
          auto const first_query_it
            { std::ranges::find_if
                ( queries
                , [&] (auto const& query)
                  {
                    return query.contains (reference_points.front());
                  }
                )
            };

          if (first_query_it == std::ranges::cend (queries))
          {
            throw std::runtime_error
              { std::format
                  ( "{}: no non-empty distribution query found for {}"
                  , algorithm
                  , spec.name
                  )
              };
          }

          auto const first_query {std::array<Cuboid<D>, 1> {*first_query_it}};
          auto const first_metric
            {std::format ("{}_first_response", spec.name)};
          auto const first_empty_message
            { std::format
                ( "{}::restrict returned empty for distribution {}"
                , algorithm
                , spec.name
                )
            };
          auto const first_measurement
            { time_restrict_measurement<D>
              ( sink
              , std::size_t {1}
              , [&]
                {
                  return collect_restrict_totals<D>
                    ( structure
                    , first_query
                    , false
                    , first_empty_message
                    , false
                    );
                }
              , false
              )
            };
          auto const steady_measurement
            { time_restrict_measurement<D>
              ( sink
              , query_count
              , [&]
                {
                  return collect_restrict_totals<D>
                    ( structure
                    , queries
                    , true
                    , {}
                    , false
                    );
                }
              , restrict_small_sample (query_count)
              )
            };
          auto const full_metric
            {std::format ("{}_full_materialization", spec.name)};
          auto const full_measurement
            { time_restrict_measurement<D>
              ( sink
              , query_count
              , [&]
                {
                  return collect_restrict_totals<D>
                    ( structure
                    , queries
                    , true
                    , {}
                    , true
                    );
                }
              , restrict_small_sample (query_count)
              )
            };

          emit_timed_restrict_measurement
            (context, algorithm, first_metric, first_measurement);
          emit_timed_restrict_measurement
            (context, algorithm, spec.name, steady_measurement);
          emit_timed_restrict_measurement
            (context, algorithm, full_metric, full_measurement);
        }
      );
  }

  template<std::size_t D>
    auto run_restrict_workloads_cache_dependent
      ( Data<D> const& data
      , BenchmarkContext<D> context
      , std::uint64_t& sink
      , bool include_stencil
      , bool include_distribution
      , bool benchmark_regular_baseline
      ) -> void
  {
    context.point_count = data.size();
    context.query_count = 0;
    context.all_query_count = 0;

    auto const points {std::span<Point<D> const> {data.points()}};
    auto const greedy_merge
      {TimedBuild<D, create::GreedyPlusMerge<D>> {points}};

    if (include_stencil)
    {
      run_restrict_stencil_workloads
        ( context
        , create::GreedyPlusMerge<D>::name()
        , greedy_merge.queryable
        , points
        , sink
        );
    }

    if (include_distribution)
    {
      run_restrict_distribution_workloads
        ( context
        , create::GreedyPlusMerge<D>::name()
        , greedy_merge.queryable
        , points
        , sink
        );
    }

    if (benchmark_regular_baseline)
    {
      auto regular_queryable
        {std::move (create::Regular<D> {to_ipt_cuboid (context.grid)}).build()};

      if (include_stencil)
      {
        run_restrict_stencil_workloads
          ( context
          , create::Regular<D>::name()
          , regular_queryable
          , points
          , sink
          );
      }

      if (include_distribution)
      {
        run_restrict_distribution_workloads
          ( context
          , create::Regular<D>::name()
          , regular_queryable
          , points
          , sink
          );
      }
    }
  }

  template<std::size_t D>
    auto run_restrict_workloads_cache_independent
      ( Data<D> const& data
      , BenchmarkContext<D> context
      , std::uint64_t& sink
      , bool include_stencil
      , bool include_distribution
      , bool include_light
      , bool include_heavy
      ) -> void
  {
    context.point_count = data.size();
    context.query_count = 0;
    context.all_query_count = 0;

    auto const points {std::span<Point<D> const> {data.points()}};

    if (include_light)
    {
      auto const sorted_points {TimedBuild<D, SortedPoints<D>> {points}};

      if (include_stencil)
      {
        run_restrict_stencil_workloads
          ( context
          , SortedPoints<D>::name()
          , sorted_points.queryable
          , points
          , sink
          );
      }

      if (include_distribution)
      {
        run_restrict_distribution_workloads
          ( context
          , SortedPoints<D>::name()
          , sorted_points.queryable
          , points
          , sink
          );
      }
    }

    if (include_light)
    {
      auto const lex_run {TimedBuild<D, LexRun<D>> {points}};

      if (include_stencil)
      {
        run_restrict_stencil_workloads
          ( context
          , LexRun<D>::name()
          , lex_run.queryable
          , points
          , sink
          );
      }

      if (include_distribution)
      {
        run_restrict_distribution_workloads
          ( context
          , LexRun<D>::name()
          , lex_run.queryable
          , points
          , sink
          );
      }
    }

    if (include_light)
    {
      auto const row_csr_k1 {TimedBuild<D, RowCSR<D, 1>> {points}};

      if (include_stencil)
      {
        run_restrict_stencil_workloads
          ( context
          , RowCSR<D, 1>::name()
          , row_csr_k1.queryable
          , points
          , sink
          );
      }

      if (include_distribution)
      {
        run_restrict_distribution_workloads
          ( context
          , RowCSR<D, 1>::name()
          , row_csr_k1.queryable
          , points
          , sink
          );
      }
    }

    if constexpr (D > 1)
    {
      if (include_light)
      {
        auto const row_csr_km
          {TimedBuild<D, RowCSR<D, D - 1>> {points}};

        if (include_stencil)
        {
          run_restrict_stencil_workloads
            ( context
            , RowCSR<D, D - 1>::name()
            , row_csr_km.queryable
            , points
            , sink
            );
        }

        if (include_distribution)
        {
          run_restrict_distribution_workloads
            ( context
            , RowCSR<D, D - 1>::name()
            , row_csr_km.queryable
            , points
            , sink
            );
        }
      }
    }

    if (include_heavy)
    {
      auto const dense_bitset
        { TimedBuild<D, DenseBitset<D>>
          { points
          , DenseBitset<D> {context.grid}
          }
        };

      if (include_stencil)
      {
        run_restrict_stencil_workloads
          ( context
          , DenseBitset<D>::name()
          , dense_bitset.queryable
          , points
          , sink
          );
      }

      if (include_distribution)
      {
        run_restrict_distribution_workloads
          ( context
          , DenseBitset<D>::name()
          , dense_bitset.queryable
          , points
          , sink
          );
      }
    }

    if (include_heavy)
    {
      auto const block_bitmap
        { TimedBuild<D, BlockBitmap<D>>
          { points
          , BlockBitmap<D> {context.grid}
          }
        };

      if (include_stencil)
      {
        run_restrict_stencil_workloads
          ( context
          , BlockBitmap<D>::name()
          , block_bitmap.queryable
          , points
          , sink
          );
      }

      if (include_distribution)
      {
        run_restrict_distribution_workloads
          ( context
          , BlockBitmap<D>::name()
          , block_bitmap.queryable
          , points
          , sink
          );
      }
    }

    if (include_heavy)
    {
      auto const roaring_baseline
        { TimedBuild<D, Roaring<D>>
          { points
          , Roaring<D> {context.grid}
          }
        };

      if (include_stencil)
      {
        run_restrict_stencil_workloads
          ( context
          , Roaring<D>::name()
          , roaring_baseline.queryable
          , points
          , sink
          );
      }

      if (include_distribution)
      {
        run_restrict_distribution_workloads
          ( context
          , Roaring<D>::name()
          , roaring_baseline.queryable
          , points
          , sink
          );
      }
    }
  }


  // ---- run_scenario_cache_dependent / _cache_independent ------------
  //
  // The original env-driven run_scenario<D> has been split into two
  // entry points so that each per-scenario binary can pick exactly
  // the algorithm class it needs without runtime selection.  The
  // bodies are otherwise unchanged from the original.
  //
  template<std::size_t D>
    auto run_scenario_cache_dependent
      ( Data<D> const&        data
      , BenchmarkContext<D>   context
      , std::uint64_t&        sink
      , bool                  benchmark_regular_baseline
      , std::size_t           all_query_cap
      ) -> void
  {
    context.point_count = data.size();
    context.query_count = data.query_count();

    auto const point_sample
      {data.sample ({context.query_count, random_seed()})};
    auto const index_sample
      {data.index_sample ({context.query_count, random_seed()})};
    auto const all_point_sample
      { evenly_spaced_sample
          ( std::span<Point<D> const> {data.points()}
          , all_query_cap
          )
      };
    auto const all_index_sample
      { evenly_spaced_index_sample
          ( data.points().size()
          , all_query_cap
          )
      };
    context.all_query_count = all_index_sample.size();

    auto const greedy_merge
      {TimedBuild<D, create::GreedyPlusMerge<D>> {data.points()}};

    {
      std::print ("{} {}\n", Measurement<D> {context, create::GreedyPlusMerge<D>::name(), "construct"}, greedy_merge.construction);
      std::print ("{} value={}\n", Measurement<D> {context, create::GreedyPlusMerge<D>::name(), "ipt_bytes"}, greedy_merge.queryable.bytes());
      std::print ("{} value={}\n", Measurement<D> {context, create::GreedyPlusMerge<D>::name(), "cuboids"}, greedy_merge.queryable.number_of_entries());
      std::print
        ( "{}"
        , TimedQueries
          { context, create::GreedyPlusMerge<D>::name(), sink
          , greedy_merge.queryable, data.points()
          , point_sample, all_point_sample
          , index_sample, all_index_sample
          }
        );
    }

    if (benchmark_regular_baseline)
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

      assert (greedy_merge.queryable == regular_queryable);

      std::print ("{} {}\n", Measurement<D> {context, create::Regular<D>::name(), "construct"}, regular_construction);
      std::print ("{} value={}\n", Measurement<D> {context, create::Regular<D>::name(), "ipt_bytes"}, regular_queryable.bytes());
      std::print ("{} value={}\n", Measurement<D> {context, create::Regular<D>::name(), "cuboids"}, regular_queryable.number_of_entries());
      std::print
        ( "{}"
        , TimedQueries
          { context, create::Regular<D>::name(), sink
          , regular_queryable, data.points()
          , point_sample, all_point_sample
          , index_sample, all_index_sample
          }
        );
    }
  }

  template<std::size_t D>
    auto run_scenario_cache_independent
      ( Data<D> const&        data
      , BenchmarkContext<D>   context
      , std::uint64_t&        sink
      , bool                  benchmark_regular_baseline
      , std::size_t           all_query_cap
      , bool                  include_light
      , bool                  include_heavy
      ) -> void
  {
    context.point_count = data.size();
    context.query_count = data.query_count();

    auto const point_sample
      {data.sample ({context.query_count, random_seed()})};
    auto const index_sample
      {data.index_sample ({context.query_count, random_seed()})};
    auto const all_point_sample
      { evenly_spaced_sample
          ( std::span<Point<D> const> {data.points()}
          , all_query_cap
          )
      };
    auto const all_index_sample
      { evenly_spaced_index_sample
          ( data.points().size()
          , all_query_cap
          )
      };
    context.all_query_count = all_index_sample.size();

    if (include_light)
    {
      auto const sorted_points
        {TimedBuild<D, SortedPoints<D>> {data.points()}};

      std::print ("{} {}\n", Measurement<D> {context, SortedPoints<D>::name(), "construct"}, sorted_points.construction);
      std::print ("{} value={}\n", Measurement<D> {context, SortedPoints<D>::name(), "point_bytes"}, sorted_points.queryable.bytes());
      std::print
        ( "{}"
        , TimedQueries
          { context, SortedPoints<D>::name(), sink
          , sorted_points.queryable, data.points()
          , point_sample, all_point_sample
          , index_sample, all_index_sample
          }
        );
    }

    if (include_light)
    {
      auto const lex_run
        {TimedBuild<D, LexRun<D>> {data.points()}};

      std::print ("{} {}\n", Measurement<D> {context, LexRun<D>::name(), "construct"}, lex_run.construction);
      std::print ("{} value={}\n", Measurement<D> {context, LexRun<D>::name(), "lexrun_bytes"}, lex_run.queryable.bytes());
      std::print
        ( "{}"
        , TimedQueries
          { context, LexRun<D>::name(), sink
          , lex_run.queryable, data.points()
          , point_sample, all_point_sample
          , index_sample, all_index_sample
          }
        );
    }

    if (include_light)
    {
      auto const row_csr_k1
        {TimedBuild<D, RowCSR<D, 1>> {data.points()}};

      std::print ("{} {}\n", Measurement<D> {context, RowCSR<D, 1>::name(), "construct"}, row_csr_k1.construction);
      std::print ("{} value={}\n", Measurement<D> {context, RowCSR<D, 1>::name(), "rowcsrk1_bytes"}, row_csr_k1.queryable.bytes());
      std::print
        ( "{}"
        , TimedQueries
          { context, RowCSR<D, 1>::name(), sink
          , row_csr_k1.queryable, data.points()
          , point_sample, all_point_sample
          , index_sample, all_index_sample
          }
        );
    }

    if (include_light)
    {
      if constexpr (D > 1)
      {
        auto const row_csr_km
        {TimedBuild<D, RowCSR<D, D - 1>> {data.points()}};

      std::print ("{} {}\n", Measurement<D> {context, RowCSR<D, D - 1>::name(), "construct"}, row_csr_km.construction);
      std::print ("{} value={}\n", Measurement<D> {context, RowCSR<D, D - 1>::name(), "rowcsrkm_bytes"}, row_csr_km.queryable.bytes());
      std::print
        ( "{}"
        , TimedQueries
          { context, RowCSR<D, D - 1>::name(), sink
          , row_csr_km.queryable, data.points()
          , point_sample, all_point_sample
          , index_sample, all_index_sample
          }
        );
      }
    }

    if (include_heavy)
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
          { context, DenseBitset<D>::name(), sink
          , dense_bitset.queryable, data.points()
          , point_sample, all_point_sample
          , index_sample, all_index_sample
          }
        );
    }

    if (include_heavy)
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
          { context, BlockBitmap<D>::name(), sink
          , block_bitmap.queryable, data.points()
          , point_sample, all_point_sample
          , index_sample, all_index_sample
          }
        );
    }

    if (include_heavy)
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
          { context, Roaring<D>::name(), sink
          , roaring_baseline.queryable, data.points()
          , point_sample, all_point_sample
          , index_sample, all_index_sample
          }
        );
    }

    if (include_light && benchmark_regular_baseline)
    {
      auto const grid_construction
        { Timing
          { sink, context.point_count, "point"
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
          { context, Grid<D>::name(), sink
          , context.grid, data.points()
          , point_sample, all_point_sample
          , index_sample, all_index_sample
          }
        );
    }
  }

  template
    < std::size_t D
    , typename Loaded
    , typename WriteFn
    , typename LoadFn
    >
    auto measure_storage_one
      ( BenchmarkContext<D> const& context
      , std::string_view           algorithm_name
      , std::filesystem::path const& path
      , std::uint64_t&             sink
      , std::span<Point<D> const>  point_sample
      , std::span<Point<D> const>  all_point_sample
      , std::span<Index const>     index_sample
      , std::span<Index const>     all_index_sample
      , std::span<Point<D> const>  reference_points
      , WriteFn&&                  write_fn
      , LoadFn&&                   load_fn
      ) -> void
  {
    auto const algorithm
      {std::format ("{}-mmap", algorithm_name)};
    constexpr std::string_view description {"storage-bench"};

    // 1. serialize
    auto const serialize_duration
      { time_once
          ([&] { std::forward<WriteFn> (write_fn) (path, description); })
      };
    std::print
      ( "{} algorithm={} metric=serialize {}\n"
      , context, algorithm
      , Timing {serialize_duration, context.point_count, "point"}
      );

    // 2. bytes on disk
    auto const file_bytes {std::filesystem::file_size (path)};
    std::print
      ( "{} algorithm={} metric=bytes_on_disk value={}\n"
      , context, algorithm, file_bytes
      );

    // 3. drop page cache (no-op on tmpfs)
    drop_page_cache (path);

    // 4. mmap-load (cold).  We use std::optional<Loaded> + an
    //    in-place load lambda so that non-movable Loaded types
    //    (e.g. anything containing an MMap region) work.
    auto loaded {std::optional<Loaded> {}};
    auto const load_duration
      { time_once
          ( [&]
            {
              std::forward<LoadFn> (load_fn) (loaded, path);
            }
          )
      };
    std::print
      ( "{} algorithm={} metric=load {}\n"
      , context, algorithm
      , Timing {load_duration, std::size_t {1}, "load"}
      );

    auto const& mmap_instance {*loaded};

    // Sanity / observability: file-backed bytes() reading.
    std::print
      ( "{} algorithm={} metric=mmap_bytes value={}\n"
      , context, algorithm, mmap_instance.bytes()
      );

    // 5. first responses (single calls, before any warmup pass).
    if (!point_sample.empty())
    {
      auto const first_pos_duration
        { time_once
            ( [&]
              {
                auto const i {mmap_instance.pos (point_sample[0])};
                sink += static_cast<std::uint64_t> (i);
              }
            )
        };
      std::print
        ( "{} algorithm={} metric=first_pos {}\n"
        , context, algorithm
        , Timing {first_pos_duration, std::size_t {1}, "pos"}
        );
    }

    if (!index_sample.empty())
    {
      auto const first_at_duration
        { time_once
            ( [&]
              {
                auto const p {mmap_instance.at (index_sample[0])};
                sink += static_cast<std::uint64_t> (p[0]);
              }
            )
        };
      std::print
        ( "{} algorithm={} metric=first_at {}\n"
        , context, algorithm
        , Timing {first_at_duration, std::size_t {1}, "at"}
        );
    }

    {
      auto const world {to_ipt_cuboid (context.grid)};
      auto const first_center
        { point_sample.empty()
            ? reference_points.front()
            : point_sample.front()
        };
      auto const first_query
        { make_stencil_query<D>
          ( world
          , first_center
          , restrict_stencil_specs<D>().front()
          )
        };
      auto const first_restrict_duration
        { time_once
            ( [&]
              {
                auto const restricted {mmap_instance.restrict (first_query)};

                if (!restricted)
                {
                  throw std::runtime_error
                    { std::format
                        ( "{}::restrict returned empty for a sampled non-empty query"
                        , algorithm
                        )
                    };
                }

                sink += static_cast<std::uint64_t> (restricted->size());
              }
            )
        };
      std::print
        ( "{} algorithm={} metric=first_restrict {}\n"
        , context, algorithm
        , Timing {first_restrict_duration, std::size_t {1}, "restrict"}
        );
    }

    // 6. steady-state pos/at on the mmap'd instance.
    std::print
      ( "{}"
      , TimedQueries
        { context
        , algorithm
        , sink
        , mmap_instance
        , {} // unused 'points' span; TimedQueries reads only samples
        , point_sample
        , all_point_sample
        , index_sample
        , all_index_sample
        }
      );

    auto ec {std::error_code {}};
    std::filesystem::remove (path, ec);
  }

  // ---- pos-mix sample factories ------------------------------------
  //
  // Build samples for the hit-vs-miss `pos` benchmark.  Hits are drawn
  // from the existing Data<D>::sample() pool.  Misses come in two
  // flavours, both producing points that no algorithm will find:
  //
  //   * out_of_grid -- a stored point with one coordinate perturbed to
  //     one stride outside the bounding grid.  Tests the cheap reject
  //     path: a single bounds check is enough for grid-based bitmaps,
  //     and the lex-ordered structures fall off the end of their
  //     entry/run/row-key array.
  //
  //   * in_grid -- uniformly drawn from the lattice spanned by the
  //     bounding grid, rejection-sampled to be absent from the data.
  //     Tests the expensive miss path: the candidate satisfies all
  //     coarse range checks and only the innermost lookup eliminates
  //     it.  Aborts with a diagnostic if the lattice is so dense that
  //     a non-stored point cannot be found within 100*k attempts.
  //
  template<std::size_t D, typename RandomEngine>
    [[nodiscard]] auto make_out_of_grid_miss_sample
      ( Data<D> const& data
      , std::size_t    k
      , RandomEngine&  engine
      ) -> std::vector<Point<D>>
  {
    assert (k > 0);
    assert (!data.points().empty());

    auto const& points {data.points()};
    auto const  grid   {data.bounding_grid()};

    auto point_dist
      { std::uniform_int_distribution<std::size_t>
          {0, points.size() - 1}
      };
    auto dim_dist
      { std::uniform_int_distribution<std::size_t> {0, D - 1} };
    auto side_dist
      { std::uniform_int_distribution<int> {0, 1} };

    auto result {std::vector<Point<D>> {}};
    result.reserve (k);

    std::generate_n
      ( std::back_inserter (result)
      , k
      , [&] () -> Point<D>
        {
          auto const& source {points[point_dist (engine)]};
          auto coords {std::array<Coordinate, D> {}};
          std::ranges::for_each
            ( std::views::iota (std::size_t {0}, D)
            , [&] (auto i) noexcept
              {
                coords[i] = source[i];
              }
            );

          auto const d      {dim_dist (engine)};
          auto const stride {grid.strides()[d]};
          auto const lo     {grid.origin()[d]};
          auto const hi
            { static_cast<Coordinate>
                ( lo
                + static_cast<Coordinate> (grid.extents()[d]) * stride
                )
            };

          coords[d] = (side_dist (engine) == 0)
            ? static_cast<Coordinate> (lo - stride)
            : static_cast<Coordinate> (hi);

          return Point<D> {coords};
        }
      );

    return result;
  }

  template<std::size_t D, typename RandomEngine>
    [[nodiscard]] auto make_in_grid_miss_sample
      ( Data<D> const&    data
      , std::size_t       k
      , RandomEngine&     engine
      , std::string_view  scenario
      ) -> std::vector<Point<D>>
  {
    assert (k > 0);
    assert (!data.points().empty());

    auto const& points {data.points()};
    auto const  grid   {data.bounding_grid()};

    auto coordinate_distributions {std::array<std::uniform_int_distribution<Index>, D> {}};
    std::ranges::for_each
      ( std::views::iota (std::size_t {0}, D)
      , [&] (auto d)
        {
          coordinate_distributions[d]
            = std::uniform_int_distribution<Index>
                { Index {0}
                , static_cast<Index> (grid.extents()[d]) - 1
                };
        }
      );

    auto const draw_candidate
      { [&] () -> Point<D>
        {
          auto coords {std::array<Coordinate, D> {}};
          std::ranges::for_each
            ( std::views::iota (std::size_t {0}, D)
            , [&] (auto d)
              {
                auto const i {coordinate_distributions[d] (engine)};
                coords[d]
                  = static_cast<Coordinate>
                      ( grid.origin()[d]
                      + static_cast<Coordinate> (i) * grid.strides()[d]
                      );
              }
            );
          return Point<D> {coords};
        }
      };

    auto result {std::vector<Point<D>> {}};
    result.reserve (k);

    auto const max_attempts {std::size_t {100} * k};
    auto       attempts     {std::size_t {0}};

    while (result.size() < k)
    {
      if (attempts++ >= max_attempts)
      {
        throw std::runtime_error
          { std::format
              ( "make_in_grid_miss_sample: scenario={} dimension={}"
                " grid_size={} stored_points={} could not find {} misses"
                " in {} attempts (lattice is too dense)"
              , scenario, D, grid.size(), points.size(), k, max_attempts
              )
          };
      }

      auto const candidate {draw_candidate()};
      if (!std::ranges::binary_search (points, candidate))
      {
        result.push_back (candidate);
      }
    }

    return result;
  }

  // Deterministically interleave hits and misses to reach the target
  // miss_ratio.  Both pools must already be sized to k.  The output is
  // a fixed-length sample of size k whose miss positions are spread by
  // a Bresenham-style stride so that timing variance is dominated by
  // the algorithm and not by clumping.
  //
  template<std::size_t D>
    [[nodiscard]] auto make_mixed_sample
      ( std::span<Point<D> const> hit_pool
      , std::span<Point<D> const> miss_pool
      , double                    miss_ratio
      ) -> std::vector<Point<D>>
  {
    assert (hit_pool.size() == miss_pool.size());
    assert (0.0 <= miss_ratio && miss_ratio <= 1.0);

    auto const k {hit_pool.size()};

    auto const target_miss_count
      { std::clamp
        ( static_cast<std::size_t>
            (miss_ratio * static_cast<double> (k) + 0.5)
        , std::size_t {0}
        , k
        )
      };

    auto result {std::vector<Point<D>> {}};
    result.reserve (k);

    auto hit_index  {std::size_t {0}};
    auto miss_index {std::size_t {0}};
    auto miss_acc   {std::size_t {0}};

    std::generate_n
      ( std::back_inserter (result)
      , k
      , [&] () -> Point<D>
        {
          // Bresenham step: emit a miss whenever the running counter
          // would overflow k.
          miss_acc += target_miss_count;
          if (miss_acc >= k && miss_index < target_miss_count)
          {
            miss_acc -= k;
            return miss_pool[miss_index++];
          }

          return hit_pool[hit_index++ % std::max (hit_pool.size(), std::size_t {1})];
        }
      );

    return result;
  }

  inline constexpr auto pos_mix_miss_ratios
    { std::array<double, 5> {0.0, 0.01, 0.10, 0.50, 1.00} };

  inline constexpr auto pos_mix_miss_ratio_tags
    { std::array<std::string_view, 5>
      {"0.000", "0.010", "0.100", "0.500", "1.000"}
    };

  // ---- run_pos_mix_scenario ----------------------------------------
  //
  // Hit-vs-miss `pos` benchmark.  Builds each of the eight
  // storage-capable algorithms once and times `try_pos` over the
  // cross product
  //
  //     {in_grid, out_of_grid} x pos_mix_miss_ratios.
  //
  // Each measurement emits one line in the
  //
  //   {context} algorithm=NAME metric=pos_mix
  //   miss_kind=KIND miss_ratio=R ticks=... ns_per_pos=... Mpos_per_sec=...
  //
  // format that results/plot/IPTPlot.cpp parses as a generic key-value
  // record.
  //
  template<std::size_t D, typename Structure>
    auto measure_pos_mix_one
      ( BenchmarkContext<D> const&  context
      , std::string_view            algorithm
      , Structure const&            structure
      , std::span<Point<D> const>   hit_pool
      , std::span<Point<D> const>   in_grid_miss_pool
      , std::span<Point<D> const>   out_of_grid_miss_pool
      , std::uint64_t&              sink
      ) -> void
  {
    auto const time_one
      { [&] ( std::string_view             miss_kind
            , std::string_view             miss_ratio_tag
            , std::vector<Point<D>> const& sample
            )
        {
          auto timing
            { Timing
              { sink
              , sample.size()
              , "pos"
              , [&]
                {
                  auto checksum {std::uint64_t {0}};
                  std::ranges::for_each
                    ( sample
                    , [&] (auto const& point) noexcept
                      {
                        auto const found {structure.try_pos (point)};
                        if (found)
                        {
                          checksum += static_cast<std::uint64_t> (*found);
                        }
                        else
                        {
                          checksum += 0x9e3779b97f4a7c15ULL;
                        }
                      }
                    );
                  return checksum;
                }
              , true // warmup; sample size is ~1000 which is borderline
              }
            };
          std::print
            ( "{} algorithm={} metric=pos_mix miss_kind={} miss_ratio={} {}\n"
            , context, algorithm, miss_kind, miss_ratio_tag, timing
            );
        }
      };

    std::ranges::for_each
      ( std::views::iota (std::size_t {0}, pos_mix_miss_ratios.size())
      , [&] (auto i)
        {
          auto const ratio {pos_mix_miss_ratios[i]};
          auto const tag   {pos_mix_miss_ratio_tags[i]};

          time_one
            ( "in_grid", tag
            , make_mixed_sample<D> (hit_pool, in_grid_miss_pool, ratio)
            );
          time_one
            ( "out_of_grid", tag
            , make_mixed_sample<D> (hit_pool, out_of_grid_miss_pool, ratio)
            );
        }
      );
  }

  template<std::size_t D>
    auto run_pos_mix_scenario
      ( Data<D> const&      data
      , BenchmarkContext<D> context
      , std::uint64_t&      sink
      ) -> void
  {
    context.point_count = data.size();
    context.query_count = data.query_count();
    context.all_query_count = 0;

    auto const k {context.query_count};
    assert (k > 0);

    auto hit_engine          {std::mt19937_64 {context.seed ^ 0x484954ULL}};
    auto in_grid_miss_engine {std::mt19937_64 {context.seed ^ 0x494e4752ULL}};
    auto out_grid_miss_engine{std::mt19937_64 {context.seed ^ 0x4f55544fULL}};

    auto const hit_pool
      {data.sample ({k, std::uniform_int_distribution<std::uint64_t>{} (hit_engine)})};
    auto const in_grid_miss_pool
      {make_in_grid_miss_sample<D> (data, k, in_grid_miss_engine, context.scenario)};
    auto const out_of_grid_miss_pool
      {make_out_of_grid_miss_sample<D> (data, k, out_grid_miss_engine)};

    auto const hit_span         {std::span<Point<D> const> {hit_pool}};
    auto const in_grid_span     {std::span<Point<D> const> {in_grid_miss_pool}};
    auto const out_of_grid_span {std::span<Point<D> const> {out_of_grid_miss_pool}};

    {
      auto built {TimedBuild<D, create::GreedyPlusMerge<D>> {data.points()}};
      measure_pos_mix_one
        ( context, create::GreedyPlusMerge<D>::name()
        , built.queryable
        , hit_span, in_grid_span, out_of_grid_span, sink
        );
    }

    {
      auto built {TimedBuild<D, SortedPoints<D>> {data.points()}};
      measure_pos_mix_one
        ( context, SortedPoints<D>::name()
        , built.queryable
        , hit_span, in_grid_span, out_of_grid_span, sink
        );
    }

    {
      auto built
        { TimedBuild<D, DenseBitset<D>>
            {data.points(), DenseBitset<D> {context.grid}}
        };
      measure_pos_mix_one
        ( context, DenseBitset<D>::name()
        , built.queryable
        , hit_span, in_grid_span, out_of_grid_span, sink
        );
    }

    {
      auto built
        { TimedBuild<D, BlockBitmap<D>>
            {data.points(), BlockBitmap<D> {context.grid}}
        };
      measure_pos_mix_one
        ( context, BlockBitmap<D>::name()
        , built.queryable
        , hit_span, in_grid_span, out_of_grid_span, sink
        );
    }

    {
      auto built
        { TimedBuild<D, Roaring<D>>
            {data.points(), Roaring<D> {context.grid}}
        };
      measure_pos_mix_one
        ( context, Roaring<D>::name()
        , built.queryable
        , hit_span, in_grid_span, out_of_grid_span, sink
        );
    }

    {
      auto built {TimedBuild<D, LexRun<D>> {data.points()}};
      measure_pos_mix_one
        ( context, LexRun<D>::name()
        , built.queryable
        , hit_span, in_grid_span, out_of_grid_span, sink
        );
    }

    {
      auto built {TimedBuild<D, RowCSR<D, 1>> {data.points()}};
      measure_pos_mix_one
        ( context, RowCSR<D, 1>::name()
        , built.queryable
        , hit_span, in_grid_span, out_of_grid_span, sink
        );
    }

    if constexpr (D > 1)
    {
      auto built {TimedBuild<D, RowCSR<D, D - 1>> {data.points()}};
      measure_pos_mix_one
        ( context, RowCSR<D, D - 1>::name()
        , built.queryable
        , hit_span, in_grid_span, out_of_grid_span, sink
        );
    }
  }

  // ---- run_storage_scenario (always-on, no environment-variable gating) -------------
  //
  // Always runs all 8 algorithms on the supplied data; the storage
  // binary is the only caller and always wants the full set.
  //
  template<std::size_t D>
    auto run_storage_scenario
      ( Data<D> const&             data
      , BenchmarkContext<D>        context
      , std::uint64_t&             sink
      , std::size_t                all_query_cap
      , std::filesystem::path const& storage_directory
          = ensure_storage_directory (default_storage_directory)
      ) -> void
  {
    context.point_count = data.size();
    context.query_count = data.query_count();

    auto const point_sample
      {data.sample ({context.query_count, random_seed()})};
    auto const index_sample
      {data.index_sample ({context.query_count, random_seed()})};
    auto const all_point_sample
      { evenly_spaced_sample
          ( std::span<Point<D> const> {data.points()}
          , all_query_cap
          )
      };
    auto const all_index_sample
      { evenly_spaced_index_sample
          ( data.points().size()
          , all_query_cap
          )
      };
    context.all_query_count = all_index_sample.size();

    auto const pid {static_cast<unsigned> (::getpid())};
    auto const path_for
      { [&] (std::string_view tag) -> std::filesystem::path
        {
          return storage_directory
               / std::format ("{}-{}-{}.bin", tag, context.scenario, pid);
        }
      };

    {
      auto built {TimedBuild<D, create::GreedyPlusMerge<D>> {data.points()}};
      measure_storage_one<D, ipt::IPT<D, ipt::storage::MMap<D>>>
        ( context, create::GreedyPlusMerge<D>::name()
        , path_for ("ipt"), sink
        , point_sample, all_point_sample
        , index_sample, all_index_sample
        , std::span<Point<D> const> {data.points()}
        , [&] (auto const& path, auto descriptor)
          { ipt::storage::write (path, built.queryable, descriptor); }
        , [] (auto& slot, auto const& path)
          { slot.emplace (std::in_place, path); }
        );
    }

    {
      auto built {TimedBuild<D, SortedPoints<D>> {data.points()}};
      measure_storage_one
        < D, SortedPoints<D, ipt::baseline::sorted_points::storage::MMap<D>>>
        ( context, SortedPoints<D>::name()
        , path_for ("sorted-points"), sink
        , point_sample, all_point_sample
        , index_sample, all_index_sample
        , std::span<Point<D> const> {data.points()}
        , [&] (auto const& path, auto descriptor)
          { ipt::baseline::sorted_points::write<D>
              (path, built.queryable.storage(), descriptor); }
        , [] (auto& slot, auto const& path)
          { slot.emplace (std::in_place, path); }
        );
    }

    {
      auto built
        { TimedBuild<D, DenseBitset<D>>
            {data.points(), DenseBitset<D> {context.grid}}
        };
      measure_storage_one
        < D, DenseBitset<D, ipt::baseline::dense_bitset::storage::MMap<D>>>
        ( context, DenseBitset<D>::name()
        , path_for ("dense-bitset"), sink
        , point_sample, all_point_sample
        , index_sample, all_index_sample
        , std::span<Point<D> const> {data.points()}
        , [&] (auto const& path, auto descriptor)
          { ipt::baseline::dense_bitset::write<D>
              (path, built.queryable.storage(), descriptor); }
        , [] (auto& slot, auto const& path)
          { slot.emplace (std::in_place, path); }
        );
    }

    {
      auto built
        { TimedBuild<D, BlockBitmap<D>>
            {data.points(), BlockBitmap<D> {context.grid}}
        };
      measure_storage_one
        < D, BlockBitmap<D, ipt::baseline::block_bitmap::storage::MMap<D>>>
        ( context, BlockBitmap<D>::name()
        , path_for ("block-bitmap"), sink
        , point_sample, all_point_sample
        , index_sample, all_index_sample
        , std::span<Point<D> const> {data.points()}
        , [&] (auto const& path, auto descriptor)
          { ipt::baseline::block_bitmap::write<D>
              (path, built.queryable.storage(), descriptor); }
        , [] (auto& slot, auto const& path)
          { slot.emplace (std::in_place, path); }
        );
    }

    {
      auto built
        { TimedBuild<D, Roaring<D>>
            {data.points(), Roaring<D> {context.grid}}
        };
      measure_storage_one
        < D, Roaring<D, ipt::baseline::roaring::storage::MMap<D>>>
        ( context, Roaring<D>::name()
        , path_for ("roaring"), sink
        , point_sample, all_point_sample
        , index_sample, all_index_sample
        , std::span<Point<D> const> {data.points()}
        , [&] (auto const& path, auto descriptor)
          { ipt::baseline::roaring::write<D>
              (path, built.queryable.storage(), descriptor); }
        , [] (auto& slot, auto const& path)
          { slot.emplace (std::in_place, path); }
        );
    }

    {
      auto built {TimedBuild<D, LexRun<D>> {data.points()}};
      measure_storage_one
        < D, LexRun<D, ipt::baseline::lex_run::storage::MMap<D>>>
        ( context, LexRun<D>::name()
        , path_for ("lex-run"), sink
        , point_sample, all_point_sample
        , index_sample, all_index_sample
        , std::span<Point<D> const> {data.points()}
        , [&] (auto const& path, auto descriptor)
          { ipt::baseline::lex_run::write<D>
              (path, built.queryable.storage(), descriptor); }
        , [] (auto& slot, auto const& path)
          { slot.emplace (std::in_place, path); }
        );
    }

    {
      auto built {TimedBuild<D, RowCSR<D, 1>> {data.points()}};
      measure_storage_one
        < D, RowCSR<D, 1, ipt::baseline::row_csr::storage::MMap<D, 1>>>
        ( context, RowCSR<D, 1>::name()
        , path_for ("row-csr-k1"), sink
        , point_sample, all_point_sample
        , index_sample, all_index_sample
        , std::span<Point<D> const> {data.points()}
        , [&] (auto const& path, auto descriptor)
          { ipt::baseline::row_csr::write<D, 1>
              (path, built.queryable.storage(), descriptor); }
        , [] (auto& slot, auto const& path)
          { slot.emplace (std::in_place, path); }
        );
    }

    if constexpr (D > 1)
    {
      auto built {TimedBuild<D, RowCSR<D, D - 1>> {data.points()}};
      measure_storage_one
        < D, RowCSR<D, D - 1, ipt::baseline::row_csr::storage::MMap<D, D - 1>>>
        ( context, RowCSR<D, D - 1>::name()
        , path_for ("row-csr-km"), sink
        , point_sample, all_point_sample
        , index_sample, all_index_sample
        , std::span<Point<D> const> {data.points()}
        , [&] (auto const& path, auto descriptor)
          { ipt::baseline::row_csr::write<D, D - 1>
              (path, built.queryable.storage(), descriptor); }
        , [] (auto& slot, auto const& path)
          { slot.emplace (std::in_place, path); }
        );
    }
  }

  // ---- per-scenario seed loop helpers ------------------------------
  //
  // Each per-scenario binary calls one of these with hard-coded
  // seed_count, all_query_cap and benchmark_regular_baseline.  These
  // are thin wrappers around the run_*_scenario functions that
  // generate seeds and synthesize a BenchmarkContext for each.
  //
  template<std::size_t D>
    auto run_seeds_cache_dependent
      ( Data<D> const&    data
      , std::string_view  scenario
      , std::size_t       seed_count
      , std::size_t       all_query_cap
      , bool              benchmark_regular_baseline
      , std::size_t       point_percentage = 100
      ) -> std::uint64_t
  {
    auto sink {std::uint64_t {0}};
    for (auto i {std::size_t {0}}; i < seed_count; ++i)
    {
      run_scenario_cache_dependent
        ( data
        , BenchmarkContext<D>
          { .grid = data.bounding_grid()
          , .seed = random_seed()
          , .scenario = scenario
          , .point_percentage = point_percentage
          }
        , sink
        , benchmark_regular_baseline
        , all_query_cap
        );
    }
    return sink;
  }

  template<std::size_t D>
    auto run_seeds_cache_independent
      ( Data<D> const&    data
      , std::string_view  scenario
      , std::size_t       seed_count
      , std::size_t       all_query_cap
      , bool              benchmark_regular_baseline
      , bool              include_light
      , bool              include_heavy
      , std::size_t       point_percentage = 100
      ) -> std::uint64_t
  {
    auto sink {std::uint64_t {0}};
    for (auto i {std::size_t {0}}; i < seed_count; ++i)
    {
      run_scenario_cache_independent
        ( data
        , BenchmarkContext<D>
          { .grid = data.bounding_grid()
          , .seed = random_seed()
          , .scenario = scenario
          , .point_percentage = point_percentage
          }
        , sink
        , benchmark_regular_baseline
        , all_query_cap
        , include_light
        , include_heavy
        );
    }
    return sink;
  }

  template<std::size_t D>
    auto run_seeds_storage
      ( Data<D> const&    data
      , std::string_view  scenario
      , std::size_t       seed_count
      , std::size_t       all_query_cap
      ) -> std::uint64_t
  {
    auto sink {std::uint64_t {0}};
    for (auto i {std::size_t {0}}; i < seed_count; ++i)
    {
      run_storage_scenario
        ( data
        , BenchmarkContext<D>
          { .grid = data.bounding_grid()
          , .seed = random_seed()
          , .scenario = scenario
          , .point_percentage = 100
          }
        , sink
        , all_query_cap
        );
    }
    return sink;
  }

  template<std::size_t D>
    auto run_seeds_pos_mix
      ( Data<D> const&    data
      , std::string_view  scenario
      , std::size_t       seed_count
      ) -> std::uint64_t
  {
    auto sink {std::uint64_t {0}};
    for (auto i {std::size_t {0}}; i < seed_count; ++i)
    {
      run_pos_mix_scenario
        ( data
        , BenchmarkContext<D>
          { .grid = data.bounding_grid()
          , .seed = random_seed()
          , .scenario = scenario
          , .point_percentage = 100
          }
        , sink
        );
    }
    return sink;
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
