#pragma once

#include <ipt/IPT.hpp>
#include <ipt/Point.hpp>
#include <vector>

namespace ipt::create
{
  template<std::size_t D>
    struct DynamicProgramming
  {
    [[nodiscard]] static constexpr auto name() noexcept;
    auto add (Point<D> const&) -> void;
    [[nodiscard]] auto build() && -> IPT<D>;

  private:
    std::vector<Point<D>> _points{};
  };
}

#include "detail/DynamicProgramming.ipp"