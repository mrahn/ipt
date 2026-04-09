#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iostream>
#include <ipt/IPT.hpp>
#include <ipt/Point.hpp>
#include <ipt/create/DynamicProgramming.hpp>
#include <ipt/create/GreedyPlusMerge.hpp>
#include <random>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace
{
  using ipt::Coordinate;
  using ipt::Index;
  using ipt::IPT;
  using ipt::Point;

  struct Summary
  {
    std::string_view family;
    std::size_t instances{};
    std::size_t optimal_instances{};
    std::size_t minimum_points{};
    std::size_t maximum_points{};
    long double ratio_sum{};
    std::size_t worst_greedy_entries{};
    std::size_t worst_optimal_entries{1};
  };

  template<std::size_t D, typename Builder>
    [[nodiscard]] auto build_from_points
      ( std::span<Point<D> const> points
      ) -> IPT<D>
  {
    auto builder {Builder {}};
    for (auto const& point : points)
    {
      builder.add (point);
    }
    return std::move (builder).build();
  }

  template<std::size_t D>
    auto check_ipt
      ( IPT<D> const& ipt
      , std::span<Point<D> const> points
      , std::string_view label
      ) -> void
  {
    if (ipt.size() != static_cast<Index> (points.size()))
    {
      throw std::runtime_error
        {std::format ("{}: IPT size mismatch", label)};
    }

    for (auto offset : std::views::iota (std::size_t {0}, points.size()))
    {
      auto const index {static_cast<Index> (offset)};
      if (ipt.at (index) != points[offset])
      {
        throw std::runtime_error
          {std::format ("{}: IPT at mismatch", label)};
      }

      if (ipt.pos (points[offset]) != index)
      {
        throw std::runtime_error
          {std::format ("{}: IPT pos mismatch", label)};
      }

      auto const maybe_position {ipt.try_pos (points[offset])};
      if (!maybe_position || *maybe_position != index)
      {
        throw std::runtime_error
          {std::format ("{}: IPT try_pos mismatch", label)};
      }
    }
  }

  template<std::size_t D>
    auto add_instance
      ( Summary& summary
      , std::span<Point<D> const> points
      ) -> void
  {
    auto const greedy
      {build_from_points<D, ipt::create::GreedyPlusMerge<D>> (points)};
    auto const optimum
      {build_from_points<D, ipt::create::DynamicProgramming<D>> (points)};

    check_ipt (greedy, points, "greedy-plus-merge");
    check_ipt (optimum, points, "dynamic-programming");

    auto const greedy_entries {greedy.number_of_entries()};
    auto const optimal_entries {optimum.number_of_entries()};
    if (greedy_entries < optimal_entries)
    {
      throw std::runtime_error
        {"greedy-plus-merge beat dynamic-programming optimum"};
    }

    summary.instances += 1;
    summary.optimal_instances += greedy_entries == optimal_entries ? 1 : 0;
    summary.minimum_points
      = summary.instances == 1
          ? points.size()
          : std::min (summary.minimum_points, points.size());
    summary.maximum_points = std::max (summary.maximum_points, points.size());
    summary.ratio_sum
      += static_cast<long double> (greedy_entries)
       / static_cast<long double> (optimal_entries);

    if ( greedy_entries * summary.worst_optimal_entries
       > summary.worst_greedy_entries * optimal_entries
       )
    {
      summary.worst_greedy_entries = greedy_entries;
      summary.worst_optimal_entries = optimal_entries;
    }
  }

  template<std::size_t D>
    auto append_grid_points
      ( std::array<Coordinate, D> const& extents
      , std::size_t dimension
      , std::array<Coordinate, D>& coordinates
      , std::vector<Point<D>>& points
      ) -> void
  {
    if (dimension == D)
    {
      points.emplace_back (coordinates);
      return;
    }

    for (auto coordinate
      : std::views::iota (Coordinate {0}, extents[dimension])
      )
    {
      coordinates[dimension] = coordinate;
      append_grid_points (extents, dimension + 1, coordinates, points);
    }
  }

  template<std::size_t D>
    [[nodiscard]] auto make_grid
      ( std::array<Coordinate, D> const& extents
      ) -> std::vector<Point<D>>
  {
    auto points {std::vector<Point<D>> {}};
    auto coordinates {std::array<Coordinate, D> {}};
    append_grid_points (extents, 0, coordinates, points);
    return points;
  }

  template<std::size_t D>
    [[nodiscard]] auto make_grid (Coordinate extent) -> std::vector<Point<D>>
  {
    auto extents {std::array<Coordinate, D> {}};
    extents.fill (extent);
    return make_grid<D> (extents);
  }

  template<std::size_t D>
    [[nodiscard]] auto points_from_mask
      ( std::span<Point<D> const> grid
      , std::uint64_t mask
      ) -> std::vector<Point<D>>
  {
    auto points {std::vector<Point<D>> {}};
    points.reserve (grid.size());

    for (auto index : std::views::iota (std::size_t {0}, grid.size()))
    {
      if ((mask & (std::uint64_t {1} << index)) != 0)
      {
        points.push_back (grid[index]);
      }
    }

    return points;
  }

  template<std::size_t D>
    [[nodiscard]] auto exhaustive_summary
      ( std::string_view family
      , std::span<Point<D> const> grid
      ) -> Summary
  {
    auto summary {Summary {.family = family}};
    auto const limit {std::uint64_t {1} << grid.size()};

    for (auto mask : std::views::iota (std::uint64_t {1}, limit))
    {
      auto const points {points_from_mask (grid, mask)};
      add_instance (summary, std::span<Point<D> const> {points});
    }

    return summary;
  }

  template<std::size_t D>
    [[nodiscard]] auto sampled_summary
      ( std::string_view family
      , std::span<Point<D> const> grid
      , std::size_t sample_count
      , double probability
      , std::uint64_t seed
      ) -> Summary
  {
    auto summary {Summary {.family = family}};
    auto random_engine {std::mt19937_64 {seed}};
    auto include_point {std::bernoulli_distribution {probability}};

    for ( [[maybe_unused]] auto sample
      : std::views::iota (std::size_t {0}, sample_count)
      )
    {
      auto points {std::vector<Point<D>> {}};
      while (points.empty())
      {
        points.clear();
        for (auto const& point : grid)
        {
          if (include_point (random_engine))
          {
            points.push_back (point);
          }
        }
      }
      add_instance (summary, std::span<Point<D> const> {points});
    }

    return summary;
  }

  auto print_summary_row (Summary const& summary) -> void
  {
    auto const optimal_percent
      { 100.0L * static_cast<long double> (summary.optimal_instances)
       / static_cast<long double> (summary.instances)
      };
    auto const mean_overhead_percent
      { 100.0L
      * ( summary.ratio_sum / static_cast<long double> (summary.instances)
        - 1.0L
        )
      };
    auto const worst_overhead_percent
      { 100.0L
      * ( static_cast<long double> (summary.worst_greedy_entries)
        / static_cast<long double> (summary.worst_optimal_entries)
        - 1.0L
        )
      };

    std::cout
      << std::format
           ( R"({} & {} & {}--{} & {:.1Lf}\% & {:.2Lf}\% & {:.2Lf}\% \\)"
           , summary.family
           , summary.instances
           , summary.minimum_points
           , summary.maximum_points
           , optimal_percent
           , mean_overhead_percent
           , worst_overhead_percent
           )
      << '\n';
  }
}

auto main() -> int
{
  auto const grid_2d_3 {make_grid<2> (Coordinate {3})};
  auto const grid_2d_4 {make_grid<2> (Coordinate {4})};
  auto const grid_3d_3 {make_grid<3> (Coordinate {3})};
  auto const grid_3d_10 {make_grid<3> (Coordinate {10})};
  auto const grid_3d_10_10_100
    {make_grid<3> (std::array<Coordinate, 3> {10, 10, 100})};
  auto const grid_3d_10_100_10
    {make_grid<3> (std::array<Coordinate, 3> {10, 100, 10})};
  auto const grid_3d_100_10_10
    {make_grid<3> (std::array<Coordinate, 3> {100, 10, 10})};

  auto const summaries
    { std::array
      { exhaustive_summary
          ( "2D $3\\times3$ exhaustive"
          , std::span<Point<2> const> {grid_2d_3}
          )
      , sampled_summary
          ( "2D $4\\times4$ sampled"
          , std::span<Point<2> const> {grid_2d_4}
          , std::size_t {4096}
          , 0.35
          , std::uint64_t {0x43524f53535f3244ULL}
          )
      , sampled_summary
          ( "3D $3\\times3\\times3$ sampled"
          , std::span<Point<3> const> {grid_3d_3}
          , std::size_t {512}
          , 0.22
          , std::uint64_t {0x43524f53535f3344ULL}
          )
      , sampled_summary
          ( "3D $10\\times10\\times10$ sampled (5\\%)"
          , std::span<Point<3> const> {grid_3d_10}
          , std::size_t {512}
          , 0.05
          , std::uint64_t {0x4752494431305f31ULL}
          )
      , sampled_summary
          ( "3D $10\\times10\\times10$ sampled (10\\%)"
          , std::span<Point<3> const> {grid_3d_10}
          , std::size_t {256}
          , 0.10
          , std::uint64_t {0x4752494431305f32ULL}
          )
      , sampled_summary
          ( "3D $10\\times10\\times10$ sampled (25\\%)"
          , std::span<Point<3> const> {grid_3d_10}
          , std::size_t {128}
          , 0.25
          , std::uint64_t {0x4752494431305f33ULL}
          )
      , sampled_summary
          ( "3D $10\\times10\\times10$ sampled (50\\%)"
          , std::span<Point<3> const> {grid_3d_10}
          , std::size_t {64}
          , 0.50
          , std::uint64_t {0x4752494431305f34ULL}
          )
      , sampled_summary
          ( "3D $10\\times10\\times100$ sampled (5\\%)"
          , std::span<Point<3> const> {grid_3d_10_10_100}
          , std::size_t {256}
          , 0.05
          , std::uint64_t {0x4752313031303031ULL}
          )
      , sampled_summary
          ( "3D $10\\times100\\times10$ sampled (5\\%)"
          , std::span<Point<3> const> {grid_3d_10_100_10}
          , std::size_t {256}
          , 0.05
          , std::uint64_t {0x4752313031303032ULL}
          )
      , sampled_summary
          ( "3D $100\\times10\\times10$ sampled (5\\%)"
          , std::span<Point<3> const> {grid_3d_100_10_10}
          , std::size_t {256}
          , 0.05
          , std::uint64_t {0x4752313031303033ULL}
          )
      }
    };

  std::cout
    << "% Auto-generated by src/benchmark/dynamic-programming-quality.cpp.\n"
    << "\\begin{tabular}{lrrrrr}\n"
    << "\\toprule\n"
    << R"(& & & & \multicolumn{2}{c}{Overhead} \\)" << '\n'
    << "\\cmidrule(lr){5-6}\n"
    << R"(Family & Instances & Points & Optimal & Mean & Worst \\)"
    << '\n'
    << "\\midrule\n";
  for (auto const& summary : summaries)
  {
    print_summary_row (summary);
  }
  std::cout << "\\bottomrule\n" << "\\end{tabular}\n";
}