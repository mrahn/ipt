#include <utility>

namespace ipt
{
  template<std::size_t D>
    constexpr SelectView<D>::SelectView
      ( Entry<D> const* first
      , Entry<D> const* last
      , Cuboid<D> query
      )
        : _first {first}
        , _last {last}
        , _query {std::in_place, std::move (query)}
  {}

  template<std::size_t D>
    constexpr SelectView<D>::iterator::iterator
      ( SelectView const* parent
      , Entry<D> const* current
      ) noexcept
        : _parent {parent}
        , _current {current}
  {
    advance_to_overlap();
  }

  template<std::size_t D>
    constexpr auto SelectView<D>::iterator::operator*
      (
      ) const noexcept -> Cuboid<D> const&
  {
    return *_intersection;
  }

  template<std::size_t D>
    constexpr auto SelectView<D>::iterator::operator++
      (
      ) noexcept -> iterator&
  {
    ++_current;
    advance_to_overlap();
    return *this;
  }

  template<std::size_t D>
    constexpr auto SelectView<D>::iterator::operator++
      ( int
      ) noexcept -> void
  {
    ++*this;
  }

  template<std::size_t D>
    constexpr auto SelectView<D>::iterator::operator==
      ( std::default_sentinel_t
      ) const noexcept -> bool
  {
    return _current == _parent->_last;
  }

  template<std::size_t D>
    constexpr auto SelectView<D>::iterator::advance_to_overlap
      (
      ) noexcept -> void
  {
    while (_current != _parent->_last)
    {
      if ( _intersection = _current->cuboid().intersect (*_parent->_query)
         ; _intersection
         )
      {
        return;
      }

      ++_current;
    }

    _intersection.reset();
  }

  template<std::size_t D>
    constexpr auto SelectView<D>::begin() const noexcept -> iterator
  {
    return iterator {this, _first};
  }

  template<std::size_t D>
    constexpr auto SelectView<D>::end
      (
      ) const noexcept -> std::default_sentinel_t
  {
    return {};
  }
}
