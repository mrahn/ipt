#pragma once

#include <baseline/RowCSR/Concepts.hpp>
#include <baseline/storage/Header.hpp>
#include <baseline/storage/Write.hpp>
#include <cstddef>
#include <filesystem>
#include <string_view>

namespace ipt::baseline::row_csr
{
  template<std::size_t D, std::size_t K, Storage<D, K> S>
    auto write
      ( std::filesystem::path const&
      , S const&
      , std::string_view = {}
      ) -> void
      ;
}

#include "detail/Write.ipp"
