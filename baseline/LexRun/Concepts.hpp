#pragma once

#include <concepts>
#include <cstddef>
#include <ipt/Coordinate.hpp>
#include <ipt/Index.hpp>
#include <ipt/Point.hpp>
#include <span>
#include <type_traits>

namespace ipt::baseline::lex_run
{
  template<std::size_t D>
    struct Run
  {
    ipt::Point<D> start;
    ipt::Coordinate stride; // step in coord D-1; 0 if length == 1
    ipt::Index length;
    ipt::Index begin; // cumulative point count preceding this run
  };

  template<std::size_t D>
    inline constexpr bool run_is_trivially_copyable
      = std::is_trivially_copyable_v<Run<D>>;

  template<typename S, std::size_t D>
    concept Storage
      = requires (S const& s)
        {
          { s.runs() } -> std::same_as<std::span<Run<D> const>>;
          { s.total_points() } -> std::same_as<ipt::Index>;
        };
}
