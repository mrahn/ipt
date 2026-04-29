#pragma once

#include <cstddef>
#include <filesystem>
#include <ipt/IPT.hpp>
#include <ipt/storage/Concepts.hpp>
#include <ipt/storage/Header.hpp>
#include <string_view>

namespace ipt::storage
{
  // Serialize an IPT to a file in the same-build / same-platform
  // binary format described in Header.hpp. The entry array is written
  // verbatim starting at entries_offset (4 KiB), with the header
  // taking up the entire first page.
  //
  // The optional description is copied into the trailing description
  // bytes of the header (NUL-padded). Throws std::length_error if the
  // description does not fit (description_max bytes, including the
  // terminating NUL).
  //
  template<std::size_t D, Storage<D> Storage>
    auto write
      ( std::filesystem::path const&
      , IPT<D, Storage> const&
      , std::string_view = {}
      ) -> void
      ;
}

#include "detail/Write.ipp"
