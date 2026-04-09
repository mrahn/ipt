#include <ipt/create/add_cuboid_and_merge.hpp>
#include <utility>

namespace ipt::create
{
  template<std::size_t D>
    constexpr auto GreedyPlusMerge<D>::name() noexcept
  {
    return "greedy-plus-merge";
  }

  template<std::size_t D>
    auto GreedyPlusMerge<D>::add (Point<D> const& point) -> void
  {
    add_cuboid_and_merge
      ( _entries
      , Cuboid<D> {typename Cuboid<D>::Singleton {point}}
      );
  }

  template<std::size_t D>
    auto GreedyPlusMerge<D>::build() && -> IPT<D>
  {
    return IPT<D> {std::move (_entries)};
  }
}
