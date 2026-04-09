#include <stdexcept>
#include <utility>
#include <vector>

namespace ipt::create
{
  template<std::size_t D>
    constexpr Regular<D>::Regular (Cuboid<D> cuboid) noexcept
      : _cuboid {std::move (cuboid)}
  {}

  template<std::size_t D>
    constexpr auto Regular<D>::name() noexcept
  {
    return "regular-ipt";
  }

  template<std::size_t D>
    auto Regular<D>::add (Point<D> const&) -> void
  {
    throw std::logic_error
      { "Regular::add: Regular must be constructed directly"
      };
  }

  template<std::size_t D>
    auto Regular<D>::build() && -> IPT<D>
  {
    return IPT<D>
      { std::vector<Entry<D>> {Entry<D> {std::move (_cuboid), Index {0}}}
      };
  }
}
