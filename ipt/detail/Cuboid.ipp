#include <algorithm>
#include <ranges>
#include <stdexcept>
#include <utility>

namespace ipt
{
  template<std::size_t D>
    constexpr Cuboid<D>::Cuboid (std::array<Ruler, D> rulers) noexcept
      : _rulers {std::move (rulers)}
#if IPT_BENCHMARK_CACHE_CUBOID_SIZE
    , _size {compute_size()}
#endif
  {}

  template<std::size_t D>
    constexpr Cuboid<D>::Cuboid (Singleton singleton) noexcept
      : _rulers
        { [&singleton]<std::size_t... Dimensions>
            ( std::index_sequence<Dimensions...>
            ) noexcept
          {
            return std::array<Ruler, D>
              { Ruler {Ruler::Singleton {singleton._point[Dimensions]}}...
              };
          } (std::make_index_sequence<D>{})
        }
#if IPT_BENCHMARK_CACHE_CUBOID_SIZE
    , _size {compute_size()}
#endif
  {}

  template<std::size_t D>
    constexpr auto Cuboid<D>::merge_if_mergeable
      ( Cuboid<D> const& other
      ) noexcept -> bool
  {
    auto differs_at {D};

    for (auto d {std::size_t {0}}; d < D; ++d)
    {
      if (_rulers[d] != other._rulers[d])
      {
        if (differs_at != D)
        {
          return false;
        }

        if (!_rulers[d].is_extended_by (other._rulers[d]))
        {
          return false;
        }

        differs_at = d;
      }
    }

    assert (differs_at < D);

    _rulers[differs_at].extend_with (other._rulers[differs_at]);

#if IPT_BENCHMARK_CACHE_CUBOID_SIZE
    _size = compute_size();
#endif

    return true;
  }

  template<std::size_t D>
    constexpr auto Cuboid<D>::size() const noexcept -> Index
  {
#if IPT_BENCHMARK_CACHE_CUBOID_SIZE
    return _size;
#else
    return compute_size();
#endif
  }

  template<std::size_t D>
    constexpr auto Cuboid<D>::lub() const noexcept -> Point<D>
  {
    auto coordinates {std::array<Coordinate, D>{}};

    std::ranges::transform
      ( _rulers
      , std::ranges::begin (coordinates)
      , [] (Ruler const& ruler) noexcept
        {
          return ruler.lub();
        }
      );

    return Point<D> {coordinates};
  }

  template<std::size_t D>
    constexpr auto Cuboid<D>::UNCHECKED_at
      ( Index index
      ) const noexcept -> Point<D>
  {
    auto coordinates {std::array<Coordinate, D>{}};

    std::ranges::transform
      ( _rulers | std::views::reverse
      , std::rbegin (coordinates)
      , [&index] (Ruler const& ruler) noexcept
        {
          auto const nd {ruler.size()};
          auto const ni {index % nd};

          index /= nd;
          return ruler.UNCHECKED_at (ni);
        }
      );

    return Point<D> {coordinates};
  }

  template<std::size_t D>
    constexpr auto Cuboid<D>::pos (Point<D> point) const -> Index
  {
    return std::ranges::fold_left
      ( _rulers
      , Index {0}
      , [&point, d {std::size_t {0}}]
          ( Index index
          , Ruler const& ruler
          ) mutable -> Index
        {
          return ruler.pos (point[d++]) + index * ruler.size();
        }
      );
  }

  template<std::size_t D>
    constexpr auto Cuboid<D>::try_pos
      ( Point<D> point
      ) const noexcept -> std::optional<Index>
  {
    auto index {Index {0}};
    auto d {std::size_t {0}};

    for (auto const& ruler : _rulers)
    {
      auto const offset {ruler.try_pos (point[d++])};

      if (!offset)
      {
        return std::nullopt;
      }

      index = *offset + index * ruler.size();
    }

    return index;
  }

  template<std::size_t D>
    constexpr auto Cuboid<D>::glb() const noexcept -> Point<D>
  {
    auto coordinates {std::array<Coordinate, D>{}};

    std::ranges::transform
      ( _rulers
      , std::ranges::begin (coordinates)
      , [] (Ruler const& ruler) noexcept
        {
          return ruler.begin();
        }
      );

    return Point<D> {coordinates};
  }

  template<std::size_t D>
    constexpr auto Cuboid<D>::ruler
    ( std::size_t d
    ) const noexcept -> Ruler const&
  {
    return _rulers[d];
  }

  template<std::size_t D>
    constexpr auto Cuboid<D>::contains
      ( Point<D> const& point
      ) const noexcept -> bool
  {
    return std::ranges::all_of
      ( std::views::iota (std::size_t {0}, D)
      , [this, &point] (std::size_t d) noexcept
        {
          return _rulers[d].contains (point[d]);
        }
      );
  }

  template<std::size_t D>
    constexpr auto Cuboid<D>::at (Index index) const -> Point<D>
  {
    if (! (index < size()))
    {
      throw std::out_of_range {"Cuboid::at: index out of range"};
    }

    return UNCHECKED_at (index);
  }

  template<std::size_t D>
    constexpr auto Cuboid<D>::intersect
      ( Cuboid<D> const& other
      ) const noexcept -> std::optional<Cuboid<D>>
  {
    auto rulers {std::array<std::optional<Ruler>, D> {}};

    auto const all_intersect
      { std::ranges::all_of
        ( std::views::iota (std::size_t {0}, D)
        , [&] (std::size_t d) noexcept
          {
            rulers[d] = _rulers[d].intersect (other._rulers[d]);
            return rulers[d].has_value();
          }
        )
      };

    if (!all_intersect)
    {
      return std::nullopt;
    }

    return Cuboid<D>
      { [&rulers]<std::size_t... Dimensions>
          ( std::index_sequence<Dimensions...>
          ) noexcept
        {
          return std::array<Ruler, D> {*std::move (rulers[Dimensions])...};
        } (std::make_index_sequence<D>{})
      };
  }

  template<std::size_t D>
    constexpr auto Cuboid<D>::compute_size() const noexcept -> Index
  {
    return std::ranges::fold_left
      ( _rulers
      , Index {1}
      , [] (Index size, Ruler const& ruler) noexcept
        {
          return size * ruler.size();
        }
      );
  }
}
