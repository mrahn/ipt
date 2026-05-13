#pragma once

#include <baseline/Grid/Header.hpp>
#include <baseline/storage/Header.hpp>
#include <baseline/storage/Write.hpp>
#include <cstddef>
#include <filesystem>
#include <string_view>

namespace ipt::baseline::grid
{
  // G must expose origin()/extents()/strides() returning indexable
  // ranges of D Coordinates each.
  template<std::size_t D, typename G>
    auto write
      ( std::filesystem::path const&
      , G const&
      , std::string_view = {}
      ) -> void
      ;
}

#include "detail/Write.ipp"
