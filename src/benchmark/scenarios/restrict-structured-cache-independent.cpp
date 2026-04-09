// Structured restrict workloads, cache-independent algorithms.
//
// Distribution and stencil workloads run on a representative subset
// of the structured multiple-survey scenarios so the aggregate
// restrict benchmark stays tractable.

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <print>

#include <benchmark/Common.hpp>
#include <benchmark/Scenarios.hpp>

namespace
{
  auto run_one
    ( Data<3> const& data
    , std::string_view scenario
    , bool include_stencil
    , std::uint64_t& sink
    ) -> void
  {
    run_restrict_workloads_cache_independent
      ( data
      , BenchmarkContext<3>
        { .grid = data.bounding_grid()
        , .seed = std::uint64_t {0}
        , .scenario = scenario
        , .point_percentage = 100
        }
      , sink
      , include_stencil
      , true
      , true
      , true
      );
  }
}

auto main() noexcept -> int try
{
  auto sink {std::uint64_t {0}};

  run_one (multiple_survey_2_l(), "multiple-survey-2-l", true, sink);
  run_one
    (multiple_survey_4_overlap(), "multiple-survey-4-overlap", true, sink);
  run_one (multiple_survey_5_mixed(), "multiple-survey-5-mixed", true, sink);
  run_one (multiple_survey_8_threed(), "multiple-survey-8-threed", true, sink);

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