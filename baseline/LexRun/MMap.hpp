#pragma once

#include <baseline/LexRun/Concepts.hpp>
#include <baseline/storage/MappedFile.hpp>
#include <cstddef>
#include <filesystem>
#include <ipt/Index.hpp>
#include <span>
#include <string_view>

namespace ipt::baseline::lex_run::storage
{
  template<std::size_t D>
    struct MMap
  {
    [[nodiscard]] explicit MMap (std::filesystem::path const&);

    MMap (MMap const&) = delete;
    MMap (MMap&&) = delete;
    auto operator= (MMap const&) -> MMap& = delete;
    auto operator= (MMap&&) -> MMap& = delete;
    ~MMap() = default;

    [[nodiscard]] auto runs
      (
      ) const noexcept -> std::span<Run<D> const>
      ;

    [[nodiscard]] auto total_points() const noexcept -> ipt::Index;

    [[nodiscard]] auto description
      (
      ) const noexcept -> std::string_view
      ;

  private:
    ipt::baseline::storage::MappedFile _file;
    std::span<Run<D> const> _runs{};
    ipt::Index _total_points {0};
  };
}

#include "detail/MMap.ipp"
