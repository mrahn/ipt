// Grid sweep, cache-dependent algorithms.
//
// Iterates 4 cube grids x {regular, 12 random densities} x 30 seeds.
// Runs the 3 IPT variants (greedy-plus-merge, greedy-plus-combine,
// regular-ipt).  No environment variables; cache-flag preset chosen at compile
// time via -DIPT_BENCHMARK_CACHE_*.
//
#include <array>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <print>

#include <benchmark/Common.hpp>

namespace
{
  inline constexpr std::size_t SEED_COUNT     {30};
  inline constexpr std::size_t QUERY_CAP      {100000};

  struct RandomScenario
  {
    char const*  name;
    std::size_t  point_percentage;
  };

  inline constexpr std::array RANDOM_SCENARIOS
    { RandomScenario {"random-1pct",   1}
    , RandomScenario {"random-2pct",   2}
    , RandomScenario {"random-5pct",   5}
    , RandomScenario {"random-10pct",  10}
    , RandomScenario {"random-25pct",  25}
    , RandomScenario {"random-50pct",  50}
    , RandomScenario {"random-75pct",  75}
    , RandomScenario {"random-90pct",  90}
    , RandomScenario {"random-95pct",  95}
    , RandomScenario {"random-98pct",  98}
    , RandomScenario {"random-99pct",  99}
    , RandomScenario {"random-100pct", 100}
    };

  inline constexpr std::array GRIDS
    { Grid<3> { Origin<3> {0, 0, 0}, Extents<3> { 10,  10,  10}, Strides<3> {1, 1, 1} }
    , Grid<3> { Origin<3> {0, 0, 0}, Extents<3> {100,  10,  10}, Strides<3> {1, 1, 1} }
    , Grid<3> { Origin<3> {0, 0, 0}, Extents<3> {100, 100,  10}, Strides<3> {1, 1, 1} }
    , Grid<3> { Origin<3> {0, 0, 0}, Extents<3> {100, 100, 100}, Strides<3> {1, 1, 1} }
    };
}

auto main() noexcept -> int try
{
  auto sink {std::uint64_t {0}};

  for (auto const& grid : GRIDS)
  {
    for (auto i {std::size_t {0}}; i < SEED_COUNT; ++i)
    {
      auto const seed {random_seed()};
      auto const regular_data {Data<3> {grid}};

      run_scenario_cache_dependent
        ( regular_data
        , BenchmarkContext<3>
          { .grid = grid
          , .seed = seed
          , .scenario = "regular"
          , .point_percentage = 100
          }
        , sink
        , true
        , QUERY_CAP
        );

      for (auto const& scenario : RANDOM_SCENARIOS)
      {
        auto const random_data
          { Data<3>
            { grid
            , RandomSample
              { std::clamp
                ( regular_data.size() * scenario.point_percentage / 100
                , std::size_t {1}
                , regular_data.size()
                )
              , random_seed()
              }
            }
          };
        run_scenario_cache_dependent
          ( random_data
          , BenchmarkContext<3>
            { .grid = grid
            , .seed = seed
            , .scenario = scenario.name
            , .point_percentage = scenario.point_percentage
            }
          , sink
          , false
          , QUERY_CAP
          );
      }
    }
  }

  return sink ? EXIT_SUCCESS : EXIT_SUCCESS;
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
