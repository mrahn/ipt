// multiple-survey-10-sixteen (cache-dependent).
//
// Generated wrapper for the multiple-survey-10-sixteen scenario.  No environment variables; constants
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
  auto const data {multiple_survey_10_sixteen()};

  for (auto i {std::size_t {0}}; i < SEED_COUNT; ++i)
  {
    run_scenario_cache_dependent
      ( data
      , BenchmarkContext<3>
        { .grid = data.bounding_grid()
        , .seed = random_seed()
        , .scenario = "multiple-survey-10-sixteen"
        , .point_percentage = 100
        }
      , sink
      , false
      , 100000
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
