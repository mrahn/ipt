#include <ipt/Index.hpp>
#include <ranges>
#include <utility>

namespace ipt::baseline
{
  template<std::size_t D>
    constexpr auto enumerate (Cuboid<D> cuboid) noexcept
  {
    auto const total {cuboid.size()};

    return std::views::iota (Index {0}, total)
         | std::views::transform
           ( [cuboid {std::move (cuboid)}] (auto index)
             {
               return cuboid.at (index);
             }
           );
  }

  template<std::size_t D>
    auto singleton_cuboid
      ( Point<D> const& point
      ) -> Cuboid<D>
  {
    return Cuboid<D> {typename Cuboid<D>::Singleton {point}};
  }
}
