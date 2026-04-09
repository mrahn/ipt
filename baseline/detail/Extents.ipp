#include <algorithm>
#include <cassert>
#include <functional>
#include <iterator>
#include <ranges>

namespace ipt::baseline
{
  template<std::size_t D>
    constexpr Extents<D>::Extents (std::array<Coordinate, D> extents)
      : _extents {extents}
  {
    assert
      (std::ranges::all_of (_extents, [] (auto n) { return n > 0; }));
  }

  template<std::size_t D>
    template<typename... Coordinates>
      requires (  sizeof... (Coordinates) == D
               && (std::convertible_to<Coordinate, Coordinates> && ...)
               )
    constexpr Extents<D>::Extents (Coordinates... ns)
      : Extents {std::array<Coordinate, D> {ns...}}
  {}

  template<std::size_t D>
    constexpr auto Extents<D>::operator[]
      ( Index index
      ) const noexcept -> Coordinate
  {
    assert (index < D);

    return _extents[index];
  }

  template<std::size_t D>
    constexpr auto Extents<D>::size() const noexcept -> Coordinate
  {
    return std::ranges::fold_left
      ( _extents
      , Coordinate {1}
      , std::multiplies<Coordinate>{}
      );
  }

  template<std::size_t D>
    constexpr auto Extents<D>::begin() const noexcept
  {
    return std::begin (_extents);
  }

  template<std::size_t D>
    constexpr auto Extents<D>::end() const noexcept
  {
    return std::end (_extents);
  }

  template<std::size_t D>
    constexpr auto Extents<D>::rbegin() const noexcept
  {
    return std::rbegin (_extents);
  }

  template<std::size_t D>
    constexpr auto Extents<D>::rend() const noexcept
  {
    return std::rend (_extents);
  }
}
