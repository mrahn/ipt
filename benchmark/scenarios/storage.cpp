// Storage benchmark.
//
// Runs serialize/load/first-response timings for the structured
// multiple-survey scenarios that have substantial IPT structure.
// Always exercises all 8 storage-capable algorithms (see
// run_storage_scenario in benchmark/Common.hpp).  Output paths and
// the storage directory are taken from the environment-free defaults in
// Common.hpp; the cache combination is selected at compile time.
//
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <print>

#include <benchmark/Common.hpp>
#include <benchmark/Scenarios.hpp>

namespace
{
  inline constexpr std::size_t SEED_COUNT {3};
  inline constexpr std::size_t QUERY_CAP  {10000};
}

auto main() noexcept -> int try
{
  auto sink {std::uint64_t {0}};

  {
    auto const data {multiple_survey_2_l()};
    for (auto i {std::size_t {0}}; i < SEED_COUNT; ++i)
    {
      run_storage_scenario
        ( data
        , BenchmarkContext<3>
          { .grid = data.bounding_grid()
          , .seed = random_seed()
          , .scenario = "multiple-survey-2-l"
          , .point_percentage = 100
          }
        , sink
        , QUERY_CAP
        );
    }
  }

  {
    auto const data {multiple_survey_3_steps()};
    for (auto i {std::size_t {0}}; i < SEED_COUNT; ++i)
    {
      run_storage_scenario
        ( data
        , BenchmarkContext<3>
          { .grid = data.bounding_grid()
          , .seed = random_seed()
          , .scenario = "multiple-survey-3-steps"
          , .point_percentage = 100
          }
        , sink
        , QUERY_CAP
        );
    }
  }

  {
    auto const data {multiple_survey_4_overlap()};
    for (auto i {std::size_t {0}}; i < SEED_COUNT; ++i)
    {
      run_storage_scenario
        ( data
        , BenchmarkContext<3>
          { .grid = data.bounding_grid()
          , .seed = random_seed()
          , .scenario = "multiple-survey-4-overlap"
          , .point_percentage = 100
          }
        , sink
        , QUERY_CAP
        );
    }
  }

  {
    auto const data {multiple_survey_5_mixed()};
    for (auto i {std::size_t {0}}; i < SEED_COUNT; ++i)
    {
      run_storage_scenario
        ( data
        , BenchmarkContext<3>
          { .grid = data.bounding_grid()
          , .seed = random_seed()
          , .scenario = "multiple-survey-5-mixed"
          , .point_percentage = 100
          }
        , sink
        , QUERY_CAP
        );
    }
  }

  {
    auto const data {multiple_survey_6_bands()};
    for (auto i {std::size_t {0}}; i < SEED_COUNT; ++i)
    {
      run_storage_scenario
        ( data
        , BenchmarkContext<3>
          { .grid = data.bounding_grid()
          , .seed = random_seed()
          , .scenario = "multiple-survey-6-bands"
          , .point_percentage = 100
          }
        , sink
        , QUERY_CAP
        );
    }
  }

  {
    auto const data {multiple_survey_7_large()};
    for (auto i {std::size_t {0}}; i < SEED_COUNT; ++i)
    {
      run_storage_scenario
        ( data
        , BenchmarkContext<3>
          { .grid = data.bounding_grid()
          , .seed = random_seed()
          , .scenario = "multiple-survey-7-large"
          , .point_percentage = 100
          }
        , sink
        , QUERY_CAP
        );
    }
  }

  {
    auto const data {multiple_survey_8_threed()};
    for (auto i {std::size_t {0}}; i < SEED_COUNT; ++i)
    {
      run_storage_scenario
        ( data
        , BenchmarkContext<3>
          { .grid = data.bounding_grid()
          , .seed = random_seed()
          , .scenario = "multiple-survey-8-threed"
          , .point_percentage = 100
          }
        , sink
        , QUERY_CAP
        );
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
