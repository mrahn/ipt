#pragma once

#include <ipt/Cuboid.hpp>
#include <ipt/IPT.hpp>
#include <ipt/Point.hpp>

namespace ipt::create
{
  template<std::size_t D>
    struct Regular
  {
    [[nodiscard]] explicit constexpr Regular (Cuboid<D>) noexcept;

    [[nodiscard]] static constexpr auto name() noexcept;
    auto add (Point<D> const&) -> void;
    [[nodiscard]] auto build() && -> IPT<D>;

  private:
    Cuboid<D> _cuboid;
  };
}

#include "detail/Regular.ipp"