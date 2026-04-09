#include <array>
#include <functional>
#include <ipt/Ruler.hpp>
#include <utility>

namespace ipt::baseline
{
  template<std::size_t D>
    constexpr auto make_cuboid
      ( Origin<D> origin
      , Extents<D> extents
      , Strides<D> strides
      ) -> Cuboid<D>
  {
    return Cuboid<D>
      { std::invoke
        ( [&]<std::size_t... Dimensions>
            ( std::index_sequence<Dimensions...>
            )
          {
            return std::array<Ruler, D>
              { Ruler
                { origin[Dimensions]
                , strides[Dimensions]
                , extents[Dimensions]
                }...
              }
              ;
          }
        , std::make_index_sequence<D>{}
        )
      };
  }
}
