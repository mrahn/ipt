#include <cassert>
#include <utility>

namespace ipt
{
  template<std::size_t D>
    constexpr Point<D>::Point (std::array<Coordinate, D> coordinates) noexcept
      : _coordinates {std::move (coordinates)}
  {}

  template<std::size_t D>
    constexpr auto Point<D>::operator[]
      ( Index index
      ) const noexcept -> Coordinate
  {
    assert (index < D);

    return _coordinates[index];
  }
}
