#pragma once

#include <baseline/LexRun/Concepts.hpp>
#include <baseline/storage/Header.hpp>
#include <baseline/storage/Write.hpp>
#include <cstddef>
#include <filesystem>
#include <string_view>

namespace ipt::baseline::lex_run
{
  template<std::size_t D, Storage<D> S>
    auto write
      ( std::filesystem::path const&
      , S const&
      , std::string_view = {}
      ) -> void
      ;
}

#include "detail/Write.ipp"
