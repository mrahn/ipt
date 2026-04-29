#include <algorithm>
#include <cassert>
#include <ranges>

namespace ipt::baseline
{
  template<std::size_t D>
    constexpr Strides<D>::Strides (std::array<Coordinate, D> strides)
      : _strides {strides}
  {
    assert
      (std::ranges::all_of (_strides, [] (auto s) { return s > 0; }));
  }

  template<std::size_t D>
    template<typename... Coordinates>
      requires (  sizeof... (Coordinates) == D
               && (std::convertible_to<Coordinate, Coordinates> && ...)
               )
    constexpr Strides<D>::Strides (Coordinates... ss)
      : Strides {std::array<Coordinate, D> {ss...}}
  {}

  template<std::size_t D>
    constexpr auto Strides<D>::operator[]
      ( Index index
      ) const noexcept -> Coordinate
  {
    assert (index < D);

    return _strides[index];
  }
}
