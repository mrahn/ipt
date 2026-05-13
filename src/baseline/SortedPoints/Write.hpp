#pragma once

// File layout:
//   [Header (4 KiB)]
//   [Point<D> array]               (section_bytes[0])
// element_count = number of points; sub_dimension = 0; kind = SortedPoints.

#include <baseline/SortedPoints/Concepts.hpp>
#include <baseline/storage/Header.hpp>
#include <baseline/storage/Write.hpp>
#include <cstddef>
#include <filesystem>
#include <string_view>

namespace ipt::baseline::sorted_points
{
  // Write a SortedPoints storage to disk. Storage is templated so
  // any backend that exposes points() is supported.
  template<std::size_t D, Storage<D> S>
    auto write
      ( std::filesystem::path const&
      , S const&
      , std::string_view = {}
      ) -> void
      ;
}

#include "detail/Write.ipp"
