#pragma once

#include <cstddef>
#include <ipt/Cuboid.hpp>
#include <ipt/Entry.hpp>
#include <iterator>
#include <optional>
#include <ranges>

namespace ipt
{
  template<std::size_t D>
    class SelectView : public std::ranges::view_interface<SelectView<D>>
  {
  public:
    constexpr SelectView() noexcept = default;

    constexpr SelectView
      ( Entry<D> const*
      , Entry<D> const*
      , Cuboid<D>
      )
      ;

    class iterator
    {
    public:
      using value_type = Cuboid<D>;
      using difference_type = std::ptrdiff_t;
      using iterator_concept = std::input_iterator_tag;

      constexpr iterator() noexcept = default;

      constexpr iterator
        ( SelectView const*
        , Entry<D> const*
        ) noexcept
        ;

      [[nodiscard]] constexpr auto operator*
        (
        ) const noexcept -> Cuboid<D> const&
        ;

      constexpr auto operator++() noexcept -> iterator&;
      constexpr auto operator++ (int) noexcept -> void;

      [[nodiscard]] constexpr auto operator==
        ( std::default_sentinel_t
        ) const noexcept -> bool
        ;

    private:
      constexpr auto advance_to_overlap() noexcept -> void;

      SelectView const* _parent {nullptr};
      Entry<D> const* _current {nullptr};
      std::optional<Cuboid<D>> _intersection;
    };

    [[nodiscard]] constexpr auto begin() const noexcept -> iterator;

    [[nodiscard]] constexpr auto end
      (
      ) const noexcept -> std::default_sentinel_t
      ;

  private:
    Entry<D> const* _first {nullptr};
    Entry<D> const* _last {nullptr};
    std::optional<Cuboid<D>> _query;
  };
}

#include "detail/SelectView.ipp"
