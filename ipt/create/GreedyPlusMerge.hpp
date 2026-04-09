#pragma once

#include <ipt/Entry.hpp>
#include <ipt/IPT.hpp>
#include <ipt/Point.hpp>
#include <vector>

namespace ipt::create
{
  template<std::size_t D>
    struct GreedyPlusMerge
  {
    [[nodiscard]] static constexpr auto name() noexcept;
    auto add (Point<D> const&) -> void;
    [[nodiscard]] auto build() && -> IPT<D>;

  protected:
    std::vector<Entry<D>> _entries{};
  };
}

#include "detail/GreedyPlusMerge.ipp"
