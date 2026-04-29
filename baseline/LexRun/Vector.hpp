#pragma once

#include <baseline/LexRun/Concepts.hpp>
#include <cstddef>
#include <ipt/Index.hpp>
#include <span>
#include <vector>

namespace ipt::baseline::lex_run::storage
{
  template<std::size_t D>
    struct Vector
  {
    Vector() noexcept = default;

    [[nodiscard]] auto runs
      (
      ) const noexcept -> std::span<Run<D> const>
      ;

    [[nodiscard]] auto total_points() const noexcept -> ipt::Index;

    auto push_back (Run<D> const&) -> void;

    [[nodiscard]] auto back() noexcept -> Run<D>&;

    [[nodiscard]] auto empty() const noexcept -> bool;

    auto bump_total_points() noexcept -> void;

  private:
    std::vector<Run<D>> _runs;
    ipt::Index _total_points {0};
  };
}

#include "detail/Vector.ipp"
