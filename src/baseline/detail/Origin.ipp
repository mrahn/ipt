#include <cassert>

namespace ipt::baseline
{
  template<std::size_t D>
    constexpr Origin<D>::Origin
      ( std::array<Coordinate, D> origin
      ) noexcept
        : _origin {origin}
  {}

  template<std::size_t D>
    template<typename... Coordinates>
      requires (  sizeof... (Coordinates) == D
               && (std::convertible_to<Coordinate, Coordinates> && ...)
               )
    constexpr Origin<D>::Origin (Coordinates... os) noexcept
      : Origin {std::array<Coordinate, D> {os...}}
  {}

  template<std::size_t D>
    constexpr auto Origin<D>::operator[]
      ( Index index
      ) const noexcept -> Coordinate
  {
    assert (index < D);

    return _origin[index];
  }
}
