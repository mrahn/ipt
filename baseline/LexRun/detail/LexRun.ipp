#include <algorithm>
#include <array>
#include <baseline/enumerate.hpp>
#include <cassert>
#include <functional>
#include <ipt/Coordinate.hpp>
#include <ipt/Ruler.hpp>
#include <iterator>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <utility>

namespace ipt::baseline
{
  template<std::size_t D, lex_run::Storage<D> S>
    template<typename... Args>
      requires std::constructible_from<S, Args&&...>
    LexRun<D, S>::LexRun (std::in_place_t, Args&&... args)
      : _storage {std::forward<Args> (args)...}
  {}

  template<std::size_t D, lex_run::Storage<D> S>
    constexpr auto LexRun<D, S>::name() noexcept
  {
    return "lex-run";
  }

  template<std::size_t D, lex_run::Storage<D> S>
    auto LexRun<D, S>::add (Point<D> const& point) noexcept -> void
      requires std::same_as<S, lex_run::storage::Vector<D>>
  {
    assert (!_last_added || *_last_added < point);

    if (!_storage.empty())
    {
      auto& back {_storage.back()};

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
          _storage.bump_total_points();
          return;
        }

        // Extend an existing run if the point continues the progression.
        if ( point[D - 1]
           == back.start[D - 1]
              + static_cast<Coordinate> (back.length) * back.stride)
        {
          back.length += 1;
          _last_added = point;
          _storage.bump_total_points();
          return;
        }
      }
    }

    // Start a new run.
    _storage.push_back
      (Run {point, Coordinate {0}, Index {1}, _storage.total_points()});
    _last_added = point;
    _storage.bump_total_points();
  }

  template<std::size_t D, lex_run::Storage<D> S>
    auto LexRun<D, S>::build() && noexcept -> LexRun
  {
    return std::move (*this);
  }

  template<std::size_t D, lex_run::Storage<D> S>
    auto LexRun<D, S>::size() const noexcept -> Index
  {
    return _storage.total_points();
  }

  template<std::size_t D, lex_run::Storage<D> S>
    auto LexRun<D, S>::bytes() const noexcept -> std::size_t
  {
    return sizeof (*this) + _storage.runs().size_bytes();
  }

  template<std::size_t D, lex_run::Storage<D> S>
    auto LexRun<D, S>::at (Index index) const -> Point<D>
  {
    auto const runs {_storage.runs()};

    if (!(index < _storage.total_points()))
    {
      throw std::out_of_range {"LexRun::at: index out of range"};
    }

    // Find the last run whose begin index is <= index.
    auto const run_upper_bound
      { std::ranges::upper_bound (runs, index, {}, &Run::begin)
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

  template<std::size_t D, lex_run::Storage<D> S>
    auto LexRun<D, S>::pos (Point<D> point) const -> Index
  {
    auto const runs {_storage.runs()};

    // Find the last run whose start is <= point (lex order).
    auto const run_upper_bound
      { std::ranges::upper_bound (runs, point, {}, &Run::start)
      };

    if (run_upper_bound == std::cbegin (runs))
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

  template<std::size_t D, lex_run::Storage<D> S>
    auto LexRun<D, S>::try_pos
      ( Point<D> point
      ) const noexcept -> std::optional<Index>
  {
    auto const runs {_storage.runs()};

    auto const run_upper_bound
      { std::ranges::upper_bound (runs, point, {}, &Run::start)
      };

    if (run_upper_bound == std::cbegin (runs))
    {
      return std::nullopt;
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
      return std::nullopt;
    }

    auto const last_diff {point[D - 1] - run.start[D - 1]};

    if (last_diff < 0)
    {
      return std::nullopt;
    }

    if (run.length == 1)
    {
      if (last_diff != 0)
      {
        return std::nullopt;
      }

      return run.begin;
    }

    if (last_diff % run.stride != 0)
    {
      return std::nullopt;
    }

    auto const offset {static_cast<Index> (last_diff / run.stride)};

    if (!(offset < run.length))
    {
      return std::nullopt;
    }

    return run.begin + offset;
  }

  template<std::size_t D, lex_run::Storage<D> S>
    auto LexRun<D, S>::select
      ( Cuboid<D> query
      ) const -> std::generator<Cuboid<D>>
  {
    for (auto const& run : _storage.runs())
    {
      auto const run_cuboid {make_run_cuboid (run)};

      if (auto inter {run_cuboid.intersect (query)})
      {
        co_yield std::move (*inter);
      }
    }
  }

  template<std::size_t D, lex_run::Storage<D> S>
    auto LexRun<D, S>::storage() const noexcept -> S const&
  {
    return _storage;
  }

  template<std::size_t D, lex_run::Storage<D> S>
    auto LexRun<D, S>::make_run_cuboid
      ( Run const& run
      ) -> Cuboid<D>
  {
    auto last_ruler
      { run.length == 1
          ? Ruler {typename Ruler::Singleton {run.start[D - 1]}}
          : Ruler
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
            return std::array<Ruler, D>
              { Ruler
                  { typename Ruler::Singleton
                    {run.start[Dimensions]}
                  }...
              , std::move (last_ruler)
              };
          }
        , std::make_index_sequence<D - 1> {}
        )
      };

    return Cuboid<D> {std::move (rulers)};
  }
}
