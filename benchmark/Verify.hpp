// Verify driver functions: round-trip, intersect, select equivalence
// across all backends.  Include this from a debug build only (i.e.
// without -DNDEBUG); the verify::* helpers are also gated by
// #ifndef NDEBUG so that releasing -DNDEBUG drops them entirely.
//
#pragma once

#include <benchmark/Common.hpp>

#ifndef NDEBUG
namespace verify
{
  using namespace ipt;
  using namespace ipt::baseline;

  // ---- ruler / cuboid intersect ------------------------------------

  inline auto enumerate_ruler
    ( Ruler const& ruler
    ) -> std::vector<Coordinate>
  {
    auto out {std::vector<Coordinate> {}};
    for (auto value {ruler.begin()}; value < ruler.end(); value += ruler.stride())
    {
      out.push_back (value);
    }
    return out;
  }

  inline auto reference_intersect
    ( Ruler const& a
    , Ruler const& b
    ) -> std::vector<Coordinate>
  {
    auto va {enumerate_ruler (a)};
    auto vb {enumerate_ruler (b)};
    auto out {std::vector<Coordinate> {}};
    std::ranges::set_intersection (va, vb, std::back_inserter (out));
    return out;
  }

  inline auto check
    ( bool ok
    , char const* message
    ) -> void
  {
    if (!ok)
    {
      throw std::runtime_error
        {std::format ("verify intersect_test FAIL: {}", message)};
    }
  }

  inline auto check_intersect_matches_reference
    ( Ruler const& a
    , Ruler const& b
    , char const* label
    ) -> void
  {
    auto const reference {reference_intersect (a, b)};
    auto const result {a.intersect (b)};

    if (reference.empty())
    {
      check (!result.has_value(), label);
      return;
    }

    if (!result.has_value())
    {
      throw std::runtime_error
        { std::format
            ( "verify intersect_test FAIL: {} expected non-empty result of size {}"
            , label
            , reference.size()
            )
        };
    }

    auto const actual {enumerate_ruler (*result)};
    check (actual == reference, label);
  }

  inline auto run_intersect() -> void
  {
    // Ruler::intersect cases
    check_intersect_matches_reference
      (Ruler {0, 1, 10}, Ruler {3, 1, 7}, "stride-1 overlap");
    check_intersect_matches_reference
      (Ruler {0, 1, 5}, Ruler {7, 1, 10}, "disjoint");
    check_intersect_matches_reference
      (Ruler {0, 2, 10}, Ruler {1, 2, 11}, "stride-2 lattice-disjoint");
    check_intersect_matches_reference
      (Ruler {0, 2, 20}, Ruler {4, 2, 16}, "stride-2 aligned");
    check_intersect_matches_reference
      (Ruler {0, 2, 30}, Ruler {0, 3, 30}, "stride 2 vs 3 from 0");
    check_intersect_matches_reference
      (Ruler {1, 2, 31}, Ruler {0, 3, 30}, "stride 2 vs 3 offset");
    check_intersect_matches_reference
      ( Ruler {Ruler::Singleton {5}}
      , Ruler {Ruler::Singleton {5}}
      , "singleton match"
      );
    check_intersect_matches_reference
      ( Ruler {Ruler::Singleton {5}}
      , Ruler {Ruler::Singleton {6}}
      , "singleton mismatch"
      );
    check_intersect_matches_reference
      (Ruler {0, 4, 100}, Ruler {6, 6, 102}, "stride 4 vs 6");
    check_intersect_matches_reference
      (Ruler {-10, 3, 20}, Ruler {-7, 5, 18}, "negative coordinates");

    // Cuboid::intersect
    {
      auto const c1 {Cuboid<2> {std::array<Ruler, 2> {Ruler {0, 1, 10}, Ruler {0, 1, 10}}}};
      auto const c2 {Cuboid<2> {std::array<Ruler, 2> {Ruler {3, 1, 8}, Ruler {2, 1, 7}}}};
      auto const r {c1.intersect (c2)};
      check (r.has_value(), "cuboid 2D intersect non-empty");
      check (r->ruler (0).begin() == 3, "cuboid 2D ruler0 begin");
      check (r->ruler (0).end() == 8, "cuboid 2D ruler0 end");
      check (r->ruler (1).begin() == 2, "cuboid 2D ruler1 begin");
      check (r->ruler (1).end() == 7, "cuboid 2D ruler1 end");
      check (r->size() == Index {25}, "cuboid 2D size");
    }
    {
      auto const c1 {Cuboid<2> {std::array<Ruler, 2> {Ruler {0, 1, 10}, Ruler {0, 1, 10}}}};
      auto const c2 {Cuboid<2> {std::array<Ruler, 2> {Ruler {0, 1, 10}, Ruler {20, 1, 30}}}};
      check (!c1.intersect (c2).has_value(), "cuboid 2D empty axis");
    }

    // Cuboid::contains
    {
      auto const c {Cuboid<2> {std::array<Ruler, 2> {Ruler {0, 2, 10}, Ruler {1, 3, 10}}}};
      check ( c.contains (Point<2> {std::array<Coordinate, 2> {4, 7}}), "contains hit");
      check (!c.contains (Point<2> {std::array<Coordinate, 2> {3, 7}}), "contains stride miss");
      check (!c.contains (Point<2> {std::array<Coordinate, 2> {4, 8}}), "contains stride miss 2");
      check (!c.contains (Point<2> {std::array<Coordinate, 2> {12, 7}}), "contains bbox miss");
    }

    // enumerate
    {
      auto const cuboid
        { Cuboid<2> {std::array<Ruler, 2> {Ruler {0, 2, 6}, Ruler {1, 1, 4}}}
        };
      auto points {std::vector<Point<2>> {}};
      for (auto const& point : enumerate (cuboid))
      {
        points.push_back (point);
      }
      check (points.size() == cuboid.size(), "enumerate size");
      check (points.front() == cuboid.glb(), "enumerate first");
      check (points.back() == cuboid.lub(), "enumerate last");
      check (std::ranges::is_sorted (points), "enumerate sorted");
    }

    // IPT::select on a small synthetic IPT.
    {
      auto cuboids {std::vector<Cuboid<2>>
        { Cuboid<2> {std::array<Ruler, 2> {Ruler {0, 1, 3}, Ruler {0, 1, 5}}}
        , Cuboid<2> {std::array<Ruler, 2> {Ruler {3, 1, 7}, Ruler {0, 1, 5}}}
        , Cuboid<2> {std::array<Ruler, 2> {Ruler {7, 1, 10}, Ruler {0, 1, 5}}}
        }};

      auto const ipt
        { IPT<2>
          { std::from_range
          , std::vector<Cuboid<2>> {cuboids}
          }
        };

      check (ipt.size() == Index {50}, "from_range size");
      check (ipt.number_of_entries() == 3, "from_range entry count");

      auto const query
        { Cuboid<2> {std::array<Ruler, 2> {Ruler {2, 1, 8}, Ruler {1, 1, 3}}} };

      auto results {std::vector<Cuboid<2>> {}};
      for (auto const& cuboid : ipt.select (query))
      {
        results.push_back (cuboid);
      }
      check (results.size() == 3, "select yields 3 sub-cuboids");

      auto const expected {std::vector<Cuboid<2>>
        { Cuboid<2> {std::array<Ruler, 2> {Ruler {2, 1, 3}, Ruler {1, 1, 3}}}
        , Cuboid<2> {std::array<Ruler, 2> {Ruler {3, 1, 7}, Ruler {1, 1, 3}}}
        , Cuboid<2> {std::array<Ruler, 2> {Ruler {7, 1, 8}, Ruler {1, 1, 3}}}
        }};
      check (results == expected, "select sub-cuboids match expected");

      // Round-trip: from_range with the select view yields a sub-IPT
      auto const sub {IPT<2> {std::from_range, ipt.select (query)}};
      check (sub.number_of_entries() == 3, "round-trip entry count");
      check (sub.size() == Index {12}, "round-trip size");

      auto round_trip_points {std::vector<Point<2>> {}};
      for (auto i {Index {0}}; i < sub.size(); ++i)
      {
        round_trip_points.push_back (sub.at (i));
      }

      auto selected_points {std::vector<Point<2>> {}};
      for (auto const& cuboid : ipt.select (query))
      {
        for (auto const& point : enumerate (cuboid))
        {
          selected_points.push_back (point);
        }
      }
      std::ranges::sort (selected_points);
      auto round_trip_points_sorted {round_trip_points};
      std::ranges::sort (round_trip_points_sorted);
      check
        ( round_trip_points_sorted == selected_points
        , "round-trip points match selection"
        );

      auto const empty_query
        { Cuboid<2> {std::array<Ruler, 2> {Ruler {100, 1, 110}, Ruler {0, 1, 5}}} };
      auto const empty_view {ipt.select (empty_query)};
      check (std::begin (empty_view) == std::default_sentinel, "empty query yields empty view");
    }

    std::print ("verify intersect_test: OK\n");
  }

  // ---- corner-steal ------------------------------------------------

  template<std::size_t D>
    class CornerSteal
  {
    static_assert (D >= 2);

  public:
    [[nodiscard]] explicit CornerSteal
      ( std::size_t copies
      , Coordinate extent = Coordinate {2}
      )
      : _copies {copies}
      , _extent {extent}
      , _points {make_points()}
    {
      if (_copies == 0)
      {
        throw std::invalid_argument
          {"CornerSteal: copies must be positive"};
      }
      if (!(_extent > 1))
      {
        throw std::invalid_argument
          {"CornerSteal: extent must be at least two"};
      }
      if (!std::ranges::is_sorted (_points))
      {
        throw std::logic_error
          {"CornerSteal: generated points must be sorted"};
      }
      if (std::ranges::adjacent_find (_points) != std::ranges::end (_points))
      {
        throw std::logic_error
          {"CornerSteal: generated points must be unique"};
      }
    }

    [[nodiscard]] constexpr auto points
      (
      ) const noexcept -> std::vector<Point<D>> const&
    {
      return _points;
    }

    [[nodiscard]] constexpr auto copies() const noexcept -> std::size_t
    {
      return _copies;
    }

    [[nodiscard]] constexpr auto optimal_entry_count
      (
      ) const noexcept -> std::size_t
    {
      return 2 * _copies;
    }

    [[nodiscard]] constexpr auto greedy_plus_merge_entry_count
      (
      ) const noexcept -> std::size_t
    {
      return _copies * (D + 1);
    }

    [[nodiscard]] auto optimal_ipt() const -> IPT<D>
    {
      auto entries {std::vector<Entry<D>> {}};
      entries.reserve (optimal_entry_count());

      auto begin {Index {0}};
      for (auto copy_index : std::views::iota (std::size_t {0}, _copies))
      {
        entries.emplace_back
          ( Cuboid<D>
            { typename Cuboid<D>::Singleton
              {corner_point (copy_index)}
            }
          , begin
          );
        begin = entries.back().end();

        entries.emplace_back (block_cuboid (copy_index), begin);
        begin = entries.back().end();
      }

      return IPT<D> {std::move (entries)};
    }

  private:
    [[nodiscard]] constexpr auto copy_offset
      ( std::size_t copy_index
      ) const noexcept -> Coordinate
    {
      return static_cast<Coordinate> (copy_index) * copy_stride();
    }

    [[nodiscard]] constexpr auto copy_stride() const noexcept -> Coordinate
    {
      return _extent + Coordinate {2};
    }

    [[nodiscard]] auto corner_point
      ( std::size_t copy_index
      ) const noexcept -> Point<D>
    {
      auto coordinates {std::array<Coordinate, D> {}};
      std::ranges::fill (coordinates, Coordinate {1});
      coordinates[0] = copy_offset (copy_index);
      return Point<D> {coordinates};
    }

    [[nodiscard]] auto block_cuboid
      ( std::size_t copy_index
      ) const noexcept -> Cuboid<D>
    {
      auto rulers
        { std::invoke
          ( [&]<std::size_t... Dimensions>
              ( std::index_sequence<Dimensions...>
              )
            {
              return std::array<Ruler, D>
                { ( static_cast<void> (Dimensions)
                  , Ruler {Coordinate {1}, Coordinate {1}, _extent + Coordinate {1}}
                  )...
                };
            }
          , std::make_index_sequence<D> {}
          )
        };
      rulers[0]
        = Ruler
          { copy_offset (copy_index) + Coordinate {1}
          , Coordinate {1}
          , copy_offset (copy_index) + _extent + Coordinate {1}
          };

      return Cuboid<D> {rulers};
    }

    [[nodiscard]] constexpr auto reserve_point_count
      (
      ) const noexcept -> std::size_t
    {
      auto block_points {std::size_t {1}};
      for ([[maybe_unused]] auto dimension : std::views::iota (std::size_t {0}, D))
      {
        block_points *= static_cast<std::size_t> (_extent);
      }
      return _copies * (block_points + 1);
    }

    auto append_block_points
      ( std::size_t copy_index
      , std::size_t dimension
      , std::array<Coordinate, D>& coordinates
      , std::vector<Point<D>>& points
      ) const -> void
    {
      if (dimension == D)
      {
        points.emplace_back (coordinates);
        return;
      }

      auto const begin
        { dimension == 0
            ? copy_offset (copy_index) + Coordinate {1}
            : Coordinate {1}
        };
      auto const end
        { dimension == 0
            ? copy_offset (copy_index) + _extent + Coordinate {1}
            : _extent + Coordinate {1}
        };

      for (auto coordinate {begin}; coordinate < end; ++coordinate)
      {
        coordinates[dimension] = coordinate;
        append_block_points (copy_index, dimension + 1, coordinates, points);
      }
    }

    [[nodiscard]] auto make_points() const -> std::vector<Point<D>>
    {
      auto points {std::vector<Point<D>> {}};
      points.reserve (reserve_point_count());

      for (auto copy_index : std::views::iota (std::size_t {0}, _copies))
      {
        points.push_back (corner_point (copy_index));
        auto coordinates {std::array<Coordinate, D> {}};
        append_block_points (copy_index, 0, coordinates, points);
      }

      return points;
    }

    std::size_t _copies;
    Coordinate _extent;
    std::vector<Point<D>> _points;
  };

  template<std::size_t D, typename Builder>
    [[nodiscard]] auto build_ipt
      ( std::span<Point<D> const> points
      ) -> IPT<D>
  {
    auto builder {Builder {}};
    std::ranges::for_each
      ( points
      , [&builder] (auto const& point) { builder.add (point); }
      );
    return std::move (builder).build();
  }

  template<std::size_t D>
    auto verify_corner_steal_case
      ( std::size_t copies
      ) -> void
  {
    auto const source {CornerSteal<D> {copies}};
    auto const expected {source.optimal_ipt()};
    auto const greedy_merge
      { build_ipt<D, create::GreedyPlusMerge<D>>
          (std::span<Point<D> const> {source.points()})
      };
    auto const greedy_combine
      { build_ipt<D, create::GreedyPlusCombine<D>>
          (std::span<Point<D> const> {source.points()})
      };

    auto const label
      { std::format
          ("corner-steal D={} copies={}", D, source.copies())
      };

    if (!(greedy_combine == expected))
    {
      throw std::runtime_error
        { std::format
            ( "{}: GreedyPlusCombine did not recover the expected optimal IPT"
            , label
            )
        };
    }
    if (greedy_combine.number_of_entries() != source.optimal_entry_count())
    {
      throw std::runtime_error
        { std::format
            ( "{}: expected {} GreedyPlusCombine entries, got {}"
            , label
            , source.optimal_entry_count()
            , greedy_combine.number_of_entries()
            )
        };
    }
    if (greedy_merge.number_of_entries() != source.greedy_plus_merge_entry_count())
    {
      throw std::runtime_error
        { std::format
            ( "{}: expected {} GreedyPlusMerge entries, got {}"
            , label
            , source.greedy_plus_merge_entry_count()
            , greedy_merge.number_of_entries()
            )
        };
    }
    if ( 2 * greedy_merge.number_of_entries()
       != (D + 1) * greedy_combine.number_of_entries()
       )
    {
      throw std::runtime_error
        {std::format ("{}: GreedyPlusMerge ratio is not (D+1)/2", label)};
    }

    std::print
      ( "verify corner_steal D={} copies={} points={} optimum={} "
        "greedy-plus-merge={} greedy-plus-combine={}\n"
      , D
      , source.copies()
      , source.points().size()
      , expected.number_of_entries()
      , greedy_merge.number_of_entries()
      , greedy_combine.number_of_entries()
      );
  }

  inline auto run_corner_steal() -> void
  {
    verify_corner_steal_case<2> (1);
    verify_corner_steal_case<2> (4);
    verify_corner_steal_case<3> (1);
    verify_corner_steal_case<3> (3);
    verify_corner_steal_case<4> (1);
    verify_corner_steal_case<4> (3);
    verify_corner_steal_case<5> (1);
    verify_corner_steal_case<5> (2);
    verify_corner_steal_case<6> (1);
    verify_corner_steal_case<6> (2);
    verify_corner_steal_case<7> (1);
    verify_corner_steal_case<8> (1);
    std::print ("verify corner_steal: OK 12 cases\n");
  }

  // ---- cross-implementation select verification --------------------

  template<std::size_t D, typename Implementation>
    [[nodiscard]] auto materialize_points
      ( Implementation const& implementation
      , Cuboid<D> const& query
      ) -> std::vector<Point<D>>
  {
    auto points {std::vector<Point<D>> {}};
    for (auto const& cuboid : implementation.select (query))
    {
      for (auto const& point : enumerate (cuboid))
      {
        points.push_back (point);
      }
    }
    std::ranges::sort (points);
    auto duplicate_points {std::ranges::unique (points)};
    points.erase
      (std::begin (duplicate_points), std::end (points));
    return points;
  }

  template<std::size_t D, typename Implementation>
    auto check_one
      ( std::string_view name
      , Implementation const& implementation
      , Cuboid<D> const& query
      , std::vector<Point<D>> const& reference
      , std::size_t query_index
      ) -> bool
  {
    auto const actual {materialize_points (implementation, query)};
    if (actual != reference)
    {
      std::print
        ( stderr
        , "MISMATCH query={} implementation={} reference_size={} got_size={}\n"
        , query_index
        , name
        , reference.size()
        , actual.size()
        );
      return false;
    }
    return true;
  }

  inline auto run_select() -> void
  {
    auto const grid
      { Grid<3>
        { Origin<3> {std::array<Coordinate, 3> {0, 0, 0}}
        , Extents<3> {std::array<Coordinate, 3> {16, 16, 16}}
        , Strides<3> {std::array<Coordinate, 3> {1, 1, 1}}
        }
      };
    auto const world {to_ipt_cuboid (grid)};
    auto data {Data<3> {grid}};
    auto const& points {data.points()};

    auto sorted_points_builder {SortedPoints<3> {}};
    for (auto const& point : points) { sorted_points_builder.add (point); }
    auto sorted_points_built {std::move (sorted_points_builder).build()};

    auto dense_bitset_builder {DenseBitset<3> {grid}};
    for (auto const& point : points) { dense_bitset_builder.add (point); }
    auto dense_bitset_built {std::move (dense_bitset_builder).build()};

    auto block_bitmap_builder {BlockBitmap<3> {grid}};
    for (auto const& point : points) { block_bitmap_builder.add (point); }
    auto block_bitmap_built {std::move (block_bitmap_builder).build()};

    auto roaring_builder {Roaring<3> {grid}};
    for (auto const& point : points) { roaring_builder.add (point); }
    auto roaring_built {std::move (roaring_builder).build()};

    auto lex_run_builder {LexRun<3> {}};
    for (auto const& point : points) { lex_run_builder.add (point); }
    auto lex_run_built {std::move (lex_run_builder).build()};

    auto row_csr_k1_builder {RowCSR<3, 1> {}};
    for (auto const& point : points) { row_csr_k1_builder.add (point); }
    auto row_csr_k1_built {std::move (row_csr_k1_builder).build()};

    auto row_csr_k2_builder {RowCSR<3, 2> {}};
    for (auto const& point : points) { row_csr_k2_builder.add (point); }
    auto row_csr_k2_built {std::move (row_csr_k2_builder).build()};

    auto regular_ipt_built
      {std::move (create::Regular<3> {world}).build()};

    auto greedy_plus_combine_builder {create::GreedyPlusCombine<3> {}};
    for (auto const& point : points) {greedy_plus_combine_builder.add (point); }
    auto greedy_plus_combine_built
      {std::move (greedy_plus_combine_builder).build()};

    auto greedy_plus_merge_builder {create::GreedyPlusMerge<3> {}};
    for (auto const& point : points) { greedy_plus_merge_builder.add (point); }
    auto greedy_plus_merge_built
      {std::move (greedy_plus_merge_builder).build()};

    auto random_engine {std::mt19937_64 {0xDEADBEEF'1234ULL}};
    auto any_failed {false};
    constexpr auto query_count {std::size_t {200}};

    for ( auto query_index {std::size_t {0}}
        ; query_index < query_count
        ; ++query_index
        )
    {
      auto build_one
        { [&] (auto d) -> Ruler
          {
            auto const& ruler {world.ruler (d)};
            auto const ruler_count
              { static_cast<Index>
                  ((ruler.end() - ruler.begin()) / ruler.stride())
              };
            auto target_count
              { std::uniform_int_distribution<Index>
                  {Index {1}, ruler_count} (random_engine)
              };
            auto const max_start_index {ruler_count - target_count};
            auto const start_index
              { (max_start_index == 0)
                  ? Index {0}
                  : std::uniform_int_distribution<Index>
                      {Index {0}, max_start_index} (random_engine)
              };
            auto const begin
              { static_cast<Coordinate>
                  ( ruler.begin()
                  + static_cast<Coordinate> (start_index) * ruler.stride()
                  )
              };
            auto const end
              { static_cast<Coordinate>
                  ( begin
                  + static_cast<Coordinate> (target_count) * ruler.stride()
                  )
              };
            return Ruler {begin, ruler.stride(), end};
          }
        };
      auto const query
        { Cuboid<3>
          { std::array<Ruler, 3>
            {build_one (0), build_one (1), build_one (2)}
          }
        };

      auto const reference {materialize_points (sorted_points_built, query)};

      any_failed |= !check_one ("dense-bitset", dense_bitset_built, query, reference, query_index);
      any_failed |= !check_one ("block-bitmap", block_bitmap_built, query, reference, query_index);
      any_failed |= !check_one ("roaring", roaring_built, query, reference, query_index);
      any_failed |= !check_one ("lex-run", lex_run_built, query, reference, query_index);
      any_failed |= !check_one ("row-csr-k1", row_csr_k1_built, query, reference, query_index);
      any_failed |= !check_one ("row-csr-k2", row_csr_k2_built, query, reference, query_index);
      any_failed |= !check_one ("regular-ipt", regular_ipt_built, query, reference, query_index);
      any_failed |= !check_one ("greedy-plus-combine", greedy_plus_combine_built, query, reference, query_index);
      any_failed |= !check_one ("greedy-plus-merge", greedy_plus_merge_built, query, reference, query_index);
      any_failed |= !check_one ("grid", grid, query, reference, query_index);
    }

    if (any_failed)
    {
      throw std::runtime_error {"verify select_verify: FAIL"};
    }

    std::print
      ( "verify select_verify: OK {} queries, all implementations agree\n"
      , query_count
      );
  }

  // Round-trip test for ipt::storage::write + ipt::storage::MMap.
  // Builds a Vector-backed IPT, serializes it to a temporary file,
  // re-loads it via MMap, then asserts that:
  //   * the entry arrays compare equal,
  //   * the description() round-trips,
  //   * 200 random pos/at queries return identical results.
  // Finally, hand-corrupts the magic byte of the on-disk header and
  // verifies that MMap rejects the file with std::runtime_error.
  //
  inline auto run_storage_roundtrip() -> void
  {
    auto const grid
      { Grid<3>
        { Origin<3>  {std::array<Coordinate, 3> {0, 0, 0}}
        , Extents<3> {std::array<Coordinate, 3> {16, 16, 16}}
        , Strides<3> {std::array<Coordinate, 3> {1, 1, 1}}
        }
      };
    auto const world {to_ipt_cuboid (grid)};

    auto reference
      {std::move (create::Regular<3> {world}).build()};

    auto const path
      { std::filesystem::temp_directory_path()
      / std::format
          ("ipt-storage-roundtrip-{}.bin", static_cast<unsigned> (::getpid()))
      };

    constexpr std::string_view description
      {"verify::run_storage_roundtrip"};

    ipt::storage::write (path, reference, description);

    {
      auto loaded
        { ipt::IPT<3, ipt::storage::MMap<3>>
            {std::in_place, path}
        };

      if (! (reference.entries_view().size()
             == loaded.entries_view().size()))
      {
        throw std::runtime_error
          {"verify storage_roundtrip FAIL: entry count differs"};
      }

      if (! std::ranges::equal
              (reference.entries_view(), loaded.entries_view()))
      {
        throw std::runtime_error
          {"verify storage_roundtrip FAIL: entry contents differ"};
      }

      // We cannot expose the storage directly from IPT, but the
      // operator== on IPT compares storages, which for MMap compares
      // the entry contents and for Vector compares the vector. Since
      // the two IPTs use different storage types, compare entries
      // directly above and the description below.

      auto random_engine {std::mt19937_64 {0xCAFEBABE'1234ULL}};
      auto const total_size {reference.size()};
      constexpr auto query_count {std::size_t {200}};

      for ( auto query_index {std::size_t {0}}
          ; query_index < query_count
          ; ++query_index
          )
      {
        auto const index
          { std::uniform_int_distribution<Index>
              {Index {0}, total_size - 1} (random_engine)
          };
        auto const ref_point  {reference.at (index)};
        auto const load_point {loaded.at (index)};

        if (! (ref_point == load_point))
        {
          throw std::runtime_error
            { std::format
                ( "verify storage_roundtrip FAIL: at({}) differs", index)
            };
        }

        auto const ref_pos  {reference.pos (ref_point)};
        auto const load_pos {loaded.pos (ref_point)};

        if (! (ref_pos == load_pos))
        {
          throw std::runtime_error
            { std::format
                ( "verify storage_roundtrip FAIL: pos differs at q{}"
                , query_index
                )
            };
        }
      }

      // Open a separate MMap purely to check description() round-trip.
      auto const mmap_only {ipt::storage::MMap<3> {path}};

      if (! (mmap_only.description() == description))
      {
        throw std::runtime_error
          {"verify storage_roundtrip FAIL: description differs"};
      }
    }

    // Negative test: corrupt the magic byte and expect MMap to reject.
    {
      auto file
        { std::fstream
            {path, std::ios::binary | std::ios::in | std::ios::out}
        };
      if (!file)
      {
        throw std::runtime_error
          {"verify storage_roundtrip FAIL: cannot reopen for corruption"};
      }
      file.seekp (0);
      auto const bogus {'X'};
      file.write (&bogus, 1);
      file.close();

      auto rejected {false};
      try
      {
        auto const bad {ipt::storage::MMap<3> {path}};
        (void) bad;
      }
      catch (std::runtime_error const&)
      {
        rejected = true;
      }

      if (!rejected)
      {
        std::filesystem::remove (path);
        throw std::runtime_error
          {"verify storage_roundtrip FAIL: corrupted magic was accepted"};
      }
    }

    std::filesystem::remove (path);

    std::print
      ("verify storage_roundtrip: OK round-trip + magic-corruption\n");
  }

  // Round-trip test for the seven baselines.  For each baseline we
  //   * build a Vector-backed instance over a 16^3 reference grid,
  //   * serialize it via its bench::<baseline>::write() free function,
  //   * load it via the corresponding MMap (or read_layout for Grid),
  //   * compare 200 random at()/pos() against the SortedPoints reference,
  //   * round-trip the description() string,
  //   * corrupt the magic byte and assert std::runtime_error on reload.
  //
  inline auto run_baseline_storage_roundtrip() -> void
  {
    auto const grid
      { Grid<3>
        { Origin<3>  {std::array<Coordinate, 3> {0, 0, 0}}
        , Extents<3> {std::array<Coordinate, 3> {16, 16, 16}}
        , Strides<3> {std::array<Coordinate, 3> {1, 1, 1}}
        }
      };
    auto const world {to_ipt_cuboid (grid)};

    // Reference: SortedPoints<3> Vector with all points of the grid.
    auto const reference
      {std::move (create::Regular<3> {world}).build()};

    auto random_engine {std::mt19937_64 {0xDEADBEEF'5678ULL}};
    constexpr auto query_count {std::size_t {200}};
    auto const total_size {reference.size()};

    constexpr std::string_view description
      {"verify::run_baseline_storage_roundtrip"};

    auto const tmpdir {std::filesystem::temp_directory_path()};
    auto const pid    {static_cast<unsigned> (::getpid())};

    auto check_against_reference
      = [&] (auto const& vector_built
            , auto const& loaded
            , std::string_view label
            ) -> void
        {
          if (! (loaded.size() == vector_built.size()))
          {
            throw std::runtime_error
              { std::format
                  ( "verify baseline_roundtrip FAIL [{}]: size differs"
                    " ({} vs {})"
                  , label, loaded.size(), vector_built.size())
              };
          }

          if (! (loaded.size() == reference.size()))
          {
            throw std::runtime_error
              { std::format
                  ( "verify baseline_roundtrip FAIL [{}]:"
                    " size != reference ({} vs {})"
                  , label, loaded.size(), reference.size())
              };
          }

          for ( auto query_index {std::size_t {0}}
              ; query_index < query_count
              ; ++query_index
              )
          {
            auto const index
              { std::uniform_int_distribution<Index>
                  {Index {0}, total_size - 1} (random_engine)
              };
            auto const vec_point  {vector_built.at (index)};
            auto const load_point {loaded.at (index)};

            if (! (vec_point == load_point))
            {
              throw std::runtime_error
                { std::format
                    ( "verify baseline_roundtrip FAIL [{}]: at({}) differs"
                    , label, index)
                };
            }

            auto const load_pos {loaded.pos (vec_point)};

            if (! (load_pos == index))
            {
              throw std::runtime_error
                { std::format
                    ( "verify baseline_roundtrip FAIL [{}]:"
                      " pos != index ({} vs {})"
                    , label, load_pos, index)
                };
            }
          }
        };

    auto corrupt_and_expect_throw
      = [] (std::filesystem::path const& path
          , auto load_again
          , std::string_view label
          ) -> void
        {
          auto file
            { std::fstream
                {path, std::ios::binary | std::ios::in | std::ios::out}
            };
          if (!file)
          {
            throw std::runtime_error
              { std::format
                  ( "verify baseline_roundtrip FAIL [{}]:"
                    " cannot reopen for corruption"
                  , label)
              };
          }
          file.seekp (0);
          auto const bogus {'X'};
          file.write (&bogus, 1);
          file.close();

          auto rejected {false};
          try
          {
            load_again();
          }
          catch (std::runtime_error const&)
          {
            rejected = true;
          }

          if (!rejected)
          {
            std::filesystem::remove (path);
            throw std::runtime_error
              { std::format
                  ( "verify baseline_roundtrip FAIL [{}]:"
                    " corrupted magic was accepted"
                  , label)
              };
          }
        };

    // ---- SortedPoints --------------------------------------------------
    {
      auto built {SortedPoints<3> {}};
      for (auto const& point : enumerate (world))
      {
        built.add (point);
      }
      auto built2 {std::move (built).build()};

      auto const path
        { tmpdir / std::format ("bench-sorted-points-{}.bin", pid) };

      ipt::baseline::sorted_points::write<3> (path, built2.storage(), description);

      {
        auto const loaded
          { SortedPoints<3, ipt::baseline::sorted_points::storage::MMap<3>>
              {std::in_place, path}
          };
        check_against_reference (built2, loaded, "sorted-points");

        auto const mmap_only
          {ipt::baseline::sorted_points::storage::MMap<3> {path}};
        if (! (mmap_only.description() == description))
        {
          throw std::runtime_error
            {"verify baseline_roundtrip FAIL [sorted-points]: description"};
        }
      }

      corrupt_and_expect_throw
        ( path
        , [&]()
          {
            auto const bad
              { SortedPoints<3, ipt::baseline::sorted_points::storage::MMap<3>>
                  {std::in_place, path}
              };
            (void) bad;
          }
        , "sorted-points"
        );

      std::filesystem::remove (path);
    }

    // ---- DenseBitset ---------------------------------------------------
    {
      auto built {DenseBitset<3> {grid}};
      for (auto const& point : enumerate (world))
      {
        built.add (point);
      }
      auto built2 {std::move (built).build()};

      auto const path
        { tmpdir / std::format ("bench-dense-bitset-{}.bin", pid) };

      ipt::baseline::dense_bitset::write<3> (path, built2.storage(), description);

      {
        auto const loaded
          { DenseBitset<3, ipt::baseline::dense_bitset::storage::MMap<3>>
              {std::in_place, path}
          };
        check_against_reference (built2, loaded, "dense-bitset");

        auto const mmap_only
          {ipt::baseline::dense_bitset::storage::MMap<3> {path}};
        if (! (mmap_only.description() == description))
        {
          throw std::runtime_error
            {"verify baseline_roundtrip FAIL [dense-bitset]: description"};
        }
      }

      corrupt_and_expect_throw
        ( path
        , [&]()
          {
            auto const bad
              { DenseBitset<3, ipt::baseline::dense_bitset::storage::MMap<3>>
                  {std::in_place, path}
              };
            (void) bad;
          }
        , "dense-bitset"
        );

      std::filesystem::remove (path);
    }

    // ---- BlockBitmap ---------------------------------------------------
    {
      auto built {BlockBitmap<3> {grid}};
      for (auto const& point : enumerate (world))
      {
        built.add (point);
      }
      auto built2 {std::move (built).build()};

      

      auto const path
        { tmpdir / std::format ("bench-block-bitmap-{}.bin", pid) };

      ipt::baseline::block_bitmap::write<3> (path, built2.storage(), description);

      {
        auto const loaded
          { BlockBitmap<3, ipt::baseline::block_bitmap::storage::MMap<3>>
              {std::in_place, path}
          };
        check_against_reference (built2, loaded, "block-bitmap");

        auto const mmap_only
          {ipt::baseline::block_bitmap::storage::MMap<3> {path}};
        if (! (mmap_only.description() == description))
        {
          throw std::runtime_error
            {"verify baseline_roundtrip FAIL [block-bitmap]: description"};
        }
      }

      corrupt_and_expect_throw
        ( path
        , [&]()
          {
            auto const bad
              { BlockBitmap<3, ipt::baseline::block_bitmap::storage::MMap<3>>
                  {std::in_place, path}
              };
            (void) bad;
          }
        , "block-bitmap"
        );

      std::filesystem::remove (path);
    }

    // ---- LexRun --------------------------------------------------------
    {
      auto built {LexRun<3> {}};
      for (auto const& point : enumerate (world))
      {
        built.add (point);
      }
      auto built2 {std::move (built).build()};

      auto const path
        { tmpdir / std::format ("bench-lex-run-{}.bin", pid) };

      ipt::baseline::lex_run::write<3> (path, built2.storage(), description);

      {
        auto const loaded
          { LexRun<3, ipt::baseline::lex_run::storage::MMap<3>>
              {std::in_place, path}
          };
        check_against_reference (built2, loaded, "lex-run");

        auto const mmap_only
          {ipt::baseline::lex_run::storage::MMap<3> {path}};
        if (! (mmap_only.description() == description))
        {
          throw std::runtime_error
            {"verify baseline_roundtrip FAIL [lex-run]: description"};
        }
      }

      corrupt_and_expect_throw
        ( path
        , [&]()
          {
            auto const bad
              { LexRun<3, ipt::baseline::lex_run::storage::MMap<3>>
                  {std::in_place, path}
              };
            (void) bad;
          }
        , "lex-run"
        );

      std::filesystem::remove (path);
    }

    // ---- RowCSR<3,1> ---------------------------------------------------
    {
      auto built {RowCSR<3, 1> {}};
      for (auto const& point : enumerate (world))
      {
        built.add (point);
      }
      auto built2 {std::move (built).build()};

      auto const path
        { tmpdir / std::format ("bench-row-csr-k1-{}.bin", pid) };

      ipt::baseline::row_csr::write<3, 1> (path, built2.storage(), description);

      {
        auto const loaded
          { RowCSR<3, 1, ipt::baseline::row_csr::storage::MMap<3, 1>>
              {std::in_place, path}
          };
        check_against_reference (built2, loaded, "row-csr-k1");
      }

      corrupt_and_expect_throw
        ( path
        , [&]()
          {
            auto const bad
              { ipt::baseline::row_csr::storage::MMap<3, 1> {path} };
            (void) bad;
          }
        , "row-csr-k1"
        );

      std::filesystem::remove (path);
    }

    // ---- RowCSR<3,2> ---------------------------------------------------
    {
      auto built {RowCSR<3, 2> {}};
      for (auto const& point : enumerate (world))
      {
        built.add (point);
      }
      auto built2 {std::move (built).build()};

      auto const path
        { tmpdir / std::format ("bench-row-csr-k2-{}.bin", pid) };

      ipt::baseline::row_csr::write<3, 2> (path, built2.storage(), description);

      {
        auto const loaded
          { RowCSR<3, 2, ipt::baseline::row_csr::storage::MMap<3, 2>>
              {std::in_place, path}
          };
        check_against_reference (built2, loaded, "row-csr-k2");
      }

      corrupt_and_expect_throw
        ( path
        , [&]()
          {
            auto const bad
              { ipt::baseline::row_csr::storage::MMap<3, 2> {path} };
            (void) bad;
          }
        , "row-csr-k2"
        );

      std::filesystem::remove (path);
    }

    // ---- Roaring -------------------------------------------------------
    {
      auto built {Roaring<3> {grid}};
      for (auto const& point : enumerate (world))
      {
        built.add (point);
      }
      auto built2 {std::move (built).build()};

      auto const path
        { tmpdir / std::format ("bench-roaring-{}.bin", pid) };

      ipt::baseline::roaring::write<3> (path, built2.storage(), description);

      {
        auto const loaded
          { Roaring<3, ipt::baseline::roaring::storage::MMap<3>>
              {std::in_place, path}
          };
        check_against_reference (built2, loaded, "roaring");

        auto const mmap_only
          {ipt::baseline::roaring::storage::MMap<3> {path}};
        if (! (mmap_only.description() == description))
        {
          throw std::runtime_error
            {"verify baseline_roundtrip FAIL [roaring]: description"};
        }
      }

      corrupt_and_expect_throw
        ( path
        , [&]()
          {
            auto const bad
              { Roaring<3, ipt::baseline::roaring::storage::MMap<3>>
                  {std::in_place, path}
              };
            (void) bad;
          }
        , "roaring"
        );

      std::filesystem::remove (path);
    }

    // ---- Grid ----------------------------------------------------------
    {
      auto const path
        { tmpdir / std::format ("bench-grid-{}.bin", pid) };

      ipt::baseline::grid::write<3> (path, grid, description);

      {
        auto const layout {ipt::baseline::grid::read_layout<3> (path)};

        auto const o {grid.origin()};
        auto const e {grid.extents()};
        auto const s {grid.strides()};

        for (auto d {std::size_t {0}}; d < 3; ++d)
        {
          if (! (layout.origin[d] == o[d])
           || ! (layout.extents[d] == static_cast<Coordinate> (e[d]))
           || ! (layout.strides[d] == s[d]))
          {
            throw std::runtime_error
              {"verify baseline_roundtrip FAIL [grid]: layout differs"};
          }
        }

        auto const mmap_only {ipt::baseline::storage::MappedFile {path}};
        if (! (mmap_only.description() == description))
        {
          throw std::runtime_error
            {"verify baseline_roundtrip FAIL [grid]: description"};
        }
      }

      corrupt_and_expect_throw
        ( path
        , [&]()
          {
            auto const bad {ipt::baseline::grid::read_layout<3> (path)};
            (void) bad;
          }
        , "grid"
        );

      std::filesystem::remove (path);
    }

    std::print
      ("verify baseline_storage_roundtrip:"
       " OK round-trip + magic-corruption (8 baselines)\n");
  }

  // ---- run_pos_miss ------------------------------------------------
  //
  // For every storage-capable algorithm: every stored point must
  // satisfy try_pos(p) == pos(p), and every miss point (both in_grid
  // and out_of_grid flavours) must satisfy try_pos(p) ==
  // std::nullopt.  Run on a tractable 8x8x8 reference grid populated
  // by a single survey scenario so the assertions are cheap and the
  // failure messages name the algorithm.
  //
  inline auto run_pos_miss() -> void
  {
    constexpr std::size_t D {3};

    auto const grid
      { Grid<D>
        { Origin<D>  {std::array<Coordinate, D> {0, 0, 0}}
        , Extents<D> {std::array<Coordinate, D> {8, 8, 8}}
        , Strides<D> {std::array<Coordinate, D> {1, 1, 1}}
        }
      };

    // Use a partially-populated lattice so misses exist inside the
    // bounding grid: keep every other point in lex order.
    auto const all_points {Data<D> {grid}.points()};

    auto kept {std::vector<Point<D>> {}};
    kept.reserve (all_points.size() / 2);
    for (std::size_t i {0}; i < all_points.size(); i += 2)
    {
      kept.push_back (all_points[i]);
    }

    auto engine             {std::mt19937_64 {0xC0FFEEULL}};
    auto in_grid_engine     {std::mt19937_64 {0xCAFEULL}};
    auto out_of_grid_engine {std::mt19937_64 {0xBEEFULL}};

    auto const make_in_grid_misses
      { [&] (std::size_t k) -> std::vector<Point<D>>
        {
          auto cd
            { std::array<std::uniform_int_distribution<Index>, D> {} };
          for (std::size_t d {0}; d < D; ++d)
          {
            cd[d] = std::uniform_int_distribution<Index>
                      {Index {0}, Index {7}};
          }
          auto out {std::vector<Point<D>> {}};
          while (out.size() < k)
          {
            auto coords {std::array<Coordinate, D> {}};
            for (std::size_t d {0}; d < D; ++d)
            {
              coords[d] = static_cast<Coordinate> (cd[d] (in_grid_engine));
            }
            auto const p {Point<D> {coords}};
            if (!std::ranges::binary_search (kept, p))
            {
              out.push_back (p);
            }
          }
          return out;
        }
      };

    auto const make_out_of_grid_misses
      { [&] (std::size_t k) -> std::vector<Point<D>>
        {
          auto out {std::vector<Point<D>> {}};
          out.reserve (k);
          auto pd
            { std::uniform_int_distribution<std::size_t>
                {0, kept.size() - 1}
            };
          auto dd
            { std::uniform_int_distribution<std::size_t> {0, D - 1} };
          for (std::size_t i {0}; i < k; ++i)
          {
            auto coords {std::array<Coordinate, D> {}};
            auto const& src {kept[pd (out_of_grid_engine)]};
            for (std::size_t d {0}; d < D; ++d)
            {
              coords[d] = src[d];
            }
            coords[dd (out_of_grid_engine)] = Coordinate {-1};
            out.push_back (Point<D> {coords});
          }
          return out;
        }
      };

    auto const in_grid_misses     {make_in_grid_misses (200)};
    auto const out_of_grid_misses {make_out_of_grid_misses (200)};

    auto hits {std::vector<Point<D>> {}};
    hits.reserve (200);
    {
      auto pd
        { std::uniform_int_distribution<std::size_t>
            {0, kept.size() - 1}
        };
      for (std::size_t i {0}; i < 200; ++i)
      {
        hits.push_back (kept[pd (engine)]);
      }
    }

    auto const check_one
      { [&]<typename Structure>
          (Structure const& s, char const* name) -> void
        {
          for (auto const& p : hits)
          {
            auto const found {s.try_pos (p)};
            if (!found)
            {
              throw std::runtime_error
                { std::format
                    ( "verify pos_miss FAIL: {} hit returned nullopt"
                    , name)
                };
            }
            auto const direct {s.pos (p)};
            if (*found != direct)
            {
              throw std::runtime_error
                { std::format
                    ( "verify pos_miss FAIL: {} try_pos != pos for hit"
                    , name)
                };
            }
          }
          for (auto const& p : in_grid_misses)
          {
            if (s.try_pos (p))
            {
              throw std::runtime_error
                { std::format
                    ( "verify pos_miss FAIL: {} in_grid miss returned hit"
                    , name)
                };
            }
          }
          for (auto const& p : out_of_grid_misses)
          {
            if (s.try_pos (p))
            {
              throw std::runtime_error
                { std::format
                    ( "verify pos_miss FAIL: {} out_of_grid miss returned hit"
                    , name)
                };
            }
          }
        }
      };

    {
      auto b {create::GreedyPlusMerge<D> {}};
      std::ranges::for_each (kept, [&] (auto const& p) { b.add (p); });
      auto built {std::move (b).build()};
      check_one (built, "ipt");
    }

    {
      auto b {SortedPoints<D> {}};
      std::ranges::for_each (kept, [&] (auto const& p) { b.add (p); });
      auto built {std::move (b).build()};
      check_one (built, "sorted-points");
    }

    {
      auto b {DenseBitset<D> {grid}};
      std::ranges::for_each (kept, [&] (auto const& p) { b.add (p); });
      auto built {std::move (b).build()};
      check_one (built, "dense-bitset");
    }

    {
      auto b {BlockBitmap<D> {grid}};
      std::ranges::for_each (kept, [&] (auto const& p) { b.add (p); });
      auto built {std::move (b).build()};
      check_one (built, "block-bitmap");
    }

    {
      auto b {Roaring<D> {grid}};
      std::ranges::for_each (kept, [&] (auto const& p) { b.add (p); });
      auto built {std::move (b).build()};
      check_one (built, "roaring");
    }

    {
      auto b {LexRun<D> {}};
      std::ranges::for_each (kept, [&] (auto const& p) { b.add (p); });
      auto built {std::move (b).build()};
      check_one (built, "lex-run");
    }

    {
      auto b {RowCSR<D, 1> {}};
      std::ranges::for_each (kept, [&] (auto const& p) { b.add (p); });
      auto built {std::move (b).build()};
      check_one (built, "row-csr-k1");
    }

    {
      auto b {RowCSR<D, 2> {}};
      std::ranges::for_each (kept, [&] (auto const& p) { b.add (p); });
      auto built {std::move (b).build()};
      check_one (built, "row-csr-k2");
    }

    std::print
      ("verify pos_miss: OK try_pos hit/in_grid/out_of_grid"
       " (8 algorithms)\n");
  }
}
#endif // !NDEBUG
