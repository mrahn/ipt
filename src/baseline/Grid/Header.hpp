#pragma once

// (De)serialization for the Grid baseline.
//
// Grid carries no per-point data: it is fully described by its
// origin/extents/strides triple. The triple is packed into the
// header reserved area; the file payload is empty.
//
// File layout:
//   [Header (4 KiB)]    -- 3*D Coordinates packed into `reserved`
//   [empty payload]
// element_count = 0; sub_dimension = 0; kind = Grid.

#include <array>
#include <baseline/storage/MappedFile.hpp>
#include <cstddef>
#include <filesystem>
#include <ipt/Coordinate.hpp>

namespace ipt::baseline::grid
{
  template<std::size_t D>
    struct ReservedLayout
  {
    std::array<ipt::Coordinate, D> origin;
    std::array<ipt::Coordinate, D> extents;
    std::array<ipt::Coordinate, D> strides;
  };

  // Build a fully-populated ReservedLayout<D> from any object that
  // exposes origin()/extents()/strides() returning indexable ranges
  // of D Coordinates each.
  template<std::size_t D, typename G>
    [[nodiscard]] constexpr auto make_reserved_layout
      ( G const&
      ) -> ReservedLayout<D>
      ;

  template<std::size_t D>
    [[nodiscard]] auto read_layout
      ( std::filesystem::path const&
      ) -> ReservedLayout<D>
      ;
}

#include "detail/Header.ipp"
