// Test binary for Common.hpp + Verify.hpp.
//
// Compiles in debug mode (no -DNDEBUG) so that the verify::* helpers
// in Verify.hpp are active.  No env vars; runs every verify pass.
//
#include <cstdint>
#include <exception>
#include <print>

#include <benchmark/Common.hpp>
#include <benchmark/Verify.hpp>

auto main() noexcept -> int try
{
  verify::run_intersect();
  verify::run_corner_steal();
  verify::run_select();
  verify::run_storage_roundtrip();
  verify::run_baseline_storage_roundtrip();
  verify::run_pos_miss();

  return 0;
}
catch (std::exception const& exception)
{
  std::println (stderr, "verify failed: {}", exception.what());
  return 1;
}
catch (...)
{
  std::println (stderr, "verify failed: unknown exception");
  return 1;
}
