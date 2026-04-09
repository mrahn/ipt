// 5D restrict workloads, cache-dependent algorithms.
//
// The 5D case runs the distribution workloads only. A full stencil
// sweep over every point would be prohibitively large on this corpus.

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <print>

#include <benchmark/Common.hpp>
#include <benchmark/Scenarios.hpp>

auto main() noexcept -> int try
{
  auto sink {std::uint64_t {0}};
  auto const data {multiple_survey_9_5d()};

  run_restrict_workloads_cache_dependent
    ( data
    , BenchmarkContext<5>
      { .grid = data.bounding_grid()
      , .seed = std::uint64_t {0}
      , .scenario = "multiple-survey-9-5d"
      , .point_percentage = 100
      }
    , sink
    , false
    , true
    , false
    );

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