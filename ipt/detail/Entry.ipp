#include <utility>

namespace ipt
{
  template<std::size_t D>
    constexpr Entry<D>::Entry (Cuboid<D> cuboid, Index begin) noexcept
      : _cuboid {std::move (cuboid)}
      , _begin {begin}
#if IPT_BENCHMARK_CACHE_ENTRY_END
      , _end {compute_end()}
#endif
#if IPT_BENCHMARK_CACHE_ENTRY_LUB
      , _lub {compute_lub()}
#endif
    {}

  template<std::size_t D>
    constexpr auto Entry<D>::end() const noexcept -> Index
  {
#if IPT_BENCHMARK_CACHE_ENTRY_END
    return _end;
#else
    return _begin + _cuboid.size();
#endif
  }

  template<std::size_t D>
    constexpr auto Entry<D>::lub() const noexcept -> Point<D>
  {
#if IPT_BENCHMARK_CACHE_ENTRY_LUB
    return _lub;
#else
    return _cuboid.lub();
#endif
  }

  template<std::size_t D>
    constexpr auto Entry<D>::pos (Point<D> point) const -> Index
  {
    return _begin + _cuboid.pos (point);
  }

  template<std::size_t D>
    constexpr auto Entry<D>::try_pos
      ( Point<D> point
      ) const noexcept -> std::optional<Index>
  {
    auto const offset {_cuboid.try_pos (point)};

    if (!offset)
    {
      return std::nullopt;
    }

    return _begin + *offset;
  }

  template<std::size_t D>
    constexpr auto Entry<D>::UNCHECKED_at
      ( Index index
      ) const noexcept -> Point<D>
  {
    return _cuboid.UNCHECKED_at (index - _begin);
  }

  template<std::size_t D>
    constexpr auto Entry<D>::merge_if_mergeable
      ( Entry const& other
      ) noexcept -> bool
  {
    if (!_cuboid.merge_if_mergeable (other._cuboid))
    {
      return false;
    }

#if IPT_BENCHMARK_CACHE_ENTRY_END
    _end = compute_end();
#endif
#if IPT_BENCHMARK_CACHE_ENTRY_LUB
    _lub = compute_lub();
#endif

    return true;
  }

  template<std::size_t D>
    constexpr auto Entry<D>::begin() const noexcept -> Index
  {
    return _begin;
  }

  template<std::size_t D>
    constexpr auto Entry<D>::size() const noexcept -> std::size_t
  {
    return _cuboid.size();
  }

  template<std::size_t D>
    constexpr auto Entry<D>::cuboid() const noexcept -> Cuboid<D> const&
  {
    return _cuboid;
  }

  template<std::size_t D>
    constexpr auto Entry<D>::compute_end() const noexcept -> Index
  {
    return _begin + _cuboid.size();
  }

  template<std::size_t D>
    constexpr auto Entry<D>::compute_lub() const noexcept -> Point<D>
  {
    return _cuboid.lub();
  }
}
