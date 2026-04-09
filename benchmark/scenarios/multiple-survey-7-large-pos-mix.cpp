// multiple-survey-7-large (pos-mix).
//
// Hit-vs-miss pos benchmark.  No environment variables; SEED_COUNT
// hard-coded.  Output lines are aggregated by plot/pos-mix-summary.cpp.
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
}

auto main() noexcept -> int try
{
  auto const data {multiple_survey_7_large()};
  auto const sink {run_seeds_pos_mix (data, "multiple-survey-7-large", SEED_COUNT)};

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
