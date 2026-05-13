// multiple-survey-7-large (cache-independent).
//
// Generated wrapper for the multiple-survey-7-large scenario.  No environment variables; constants
// hard-coded from the original runner-script defaults.
//
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <print>

#include <benchmark/Common.hpp>
#include <benchmark/Scenarios.hpp>

namespace
{
  inline constexpr std::size_t SEED_COUNT {5};
}

auto main() noexcept -> int try
{
  auto sink {std::uint64_t {0}};
  auto const data {multiple_survey_7_large()};

  // light cache-independent baselines
  for (auto i {std::size_t {0}}; i < SEED_COUNT; ++i)
  {
    run_scenario_cache_independent
      ( data
      , BenchmarkContext<3>
        { .grid = data.bounding_grid()
        , .seed = random_seed()
        , .scenario = "multiple-survey-7-large"
        , .point_percentage = 100
        }
      , sink
      , false
      , 100000
      , true
      , false
      );
  }

  // heavy cache-independent baselines
  for (auto i {std::size_t {0}}; i < SEED_COUNT; ++i)
  {
    run_scenario_cache_independent
      ( data
      , BenchmarkContext<3>
        { .grid = data.bounding_grid()
        , .seed = random_seed()
        , .scenario = "multiple-survey-7-large"
        , .point_percentage = 100
        }
      , sink
      , false
      , 10000
      , false
      , true
      );
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
