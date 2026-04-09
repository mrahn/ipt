#pragma once

#include <cstddef>
#include <ipt/Cuboid.hpp>
#include <ipt/Entry.hpp>
#include <iterator>
#include <optional>
#include <ranges>
#include <utility>

namespace ipt
{
  template<std::size_t D>
    class SelectView : public std::ranges::view_interface<SelectView<D>>
  {
  public:
    constexpr SelectView() noexcept = default;

    constexpr SelectView
      ( Entry<D> const* first
      , Entry<D> const* last
      , Cuboid<D> query
      )
        : _first {first}
        , _last {last}
        , _query {std::in_place, std::move (query)}
    {}

    class iterator
    {
    public:
      using value_type = Cuboid<D>;
      using difference_type = std::ptrdiff_t;
      using iterator_concept = std::input_iterator_tag;

      constexpr iterator() noexcept = default;

      constexpr iterator
        ( SelectView const* parent
        , Entry<D> const* current
        ) noexcept
          : _parent {parent}
          , _current {current}
      {
        advance_to_overlap();
      }

      [[nodiscard]] constexpr auto operator*
        (
        ) const noexcept -> Cuboid<D> const&
      {
        return *_intersection;
      }

      constexpr auto operator++() noexcept -> iterator&
      {
        ++_current;
        advance_to_overlap();
        return *this;
      }

      constexpr auto operator++ (int) noexcept -> void
      {
        ++*this;
      }

      [[nodiscard]] constexpr auto operator==
        ( std::default_sentinel_t
        ) const noexcept -> bool
      {
        return _current == _parent->_last;
      }

    private:
      constexpr auto advance_to_overlap() noexcept -> void
      {
        while (_current != _parent->_last)
        {
          _intersection = _current->cuboid().intersect (*_parent->_query);

          if (_intersection.has_value())
          {
            return;
          }

          ++_current;
        }

        _intersection.reset();
      }

      SelectView const* _parent {nullptr};
      Entry<D> const* _current {nullptr};
      std::optional<Cuboid<D>> _intersection;
    };

    [[nodiscard]] constexpr auto begin() const noexcept -> iterator
    {
      return iterator {this, _first};
    }

    [[nodiscard]] constexpr auto end
      (
      ) const noexcept -> std::default_sentinel_t
    {
      return {};
    }

  private:
    Entry<D> const* _first {nullptr};
    Entry<D> const* _last {nullptr};
    std::optional<Cuboid<D>> _query;
  };
}
