#include <cassert>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace ipt
{
  constexpr Ruler::Checked::Checked
    ( Coordinate begin
    , Coordinate stride
    , Coordinate end
    )
      : _begin {begin}
      , _stride {stride}
      , _end {end}
  {
    if (  ! (_stride > 0)
       || ! (_begin < _end)
       || ! (  _begin >= 0
            || _end <= std::numeric_limits<Coordinate>::max() + _begin
            )
       || ! ((_end - _begin) % _stride == 0)
       )
    {
      throw std::invalid_argument {"Ruler::Ruler"};
    }
  }

  constexpr Ruler::Ruler
    ( Coordinate begin
    , Coordinate stride
    , Coordinate end
    )
      : Ruler {Checked {begin, stride, end}}
  {}
  constexpr Ruler::Ruler
    ( Checked checked
    ) noexcept
      : Ruler {UNCHECKED{}, checked._begin, checked._stride, checked._end}
  {}
  constexpr Ruler::Ruler
    ( UNCHECKED
    , Coordinate begin
    , Coordinate stride
    , Coordinate end
    ) noexcept
      : _begin {begin}
      , _stride {stride}
      , _end {end}
#if IPT_BENCHMARK_CACHE_RULER_SIZE
      , _size {compute_size()}
#endif
  {
    assert (_stride > 0);
    assert (_begin < _end);
    assert (  _begin >= 0
           || _end <= std::numeric_limits<Coordinate>::max() + _begin
           );
    assert ((_end - _begin) % _stride == 0);
  }

  constexpr Ruler::Ruler (Singleton singleton) noexcept
    : _begin {singleton.coordinate}
    , _stride {1}
    , _end {singleton.coordinate + 1}
#if IPT_BENCHMARK_CACHE_RULER_SIZE
    , _size {1}
#endif
  {}

  constexpr auto Ruler::make_bounding_ruler
    ( Ruler const& other
    ) const noexcept -> Ruler
  {
    if (other._begin < _begin)
    {
      return other.make_bounding_ruler (*this);
    }
    assert (! (other._begin < _begin));

    auto stride {std::gcd (_stride, other._stride)};

    if (_begin != other._begin)
    {
      stride = std::gcd (stride, other._begin - _begin);
    }

    return Ruler
      { UNCHECKED{}
      , _begin
      , stride
      , std::max (lub(), other.lub()) + stride
      };
  }

  constexpr auto Ruler::pop_back
    (
    ) const noexcept -> Ruler
  {
    assert (!is_singleton());

    return Ruler {_begin, _stride, lub()};
  }

  constexpr auto Ruler::begin() const noexcept -> Coordinate
  {
    return _begin;
  }

  constexpr auto Ruler::stride() const noexcept -> Coordinate
  {
    return _stride;
  }

  constexpr auto Ruler::end() const noexcept -> Coordinate
  {
    return _end;
  }

  constexpr auto Ruler::size() const noexcept -> Index
  {
#if IPT_BENCHMARK_CACHE_RULER_SIZE
    return _size;
#else
    return compute_size();
#endif
  }

  constexpr auto Ruler::is_singleton() const noexcept -> bool
  {
    return _begin + _stride == _end;
  }

  constexpr auto Ruler::lub() const noexcept -> Coordinate
  {
    return _end - _stride;
  }

  constexpr auto Ruler::contains (Coordinate value) const noexcept -> bool
  {
    if (value < _begin || value >= _end)
    {
      return false;
    }

    return ((value - _begin) % _stride) == 0;
  }

  constexpr auto Ruler::pos (Coordinate value) const -> Index
  {
    if (!contains (value))
    {
      throw std::out_of_range {"Ruler::pos: coordinate out of range"};
    }

    return (value - _begin) / _stride;
  }

  constexpr auto Ruler::try_pos
    ( Coordinate value
    ) const noexcept -> std::optional<Index>
  {
    if (!contains (value))
    {
      return std::nullopt;
    }

    return (value - _begin) / _stride;
  }

  constexpr auto Ruler::UNCHECKED_at (Index index) const noexcept -> Coordinate
  {
    return _begin + static_cast<Coordinate> (index) * _stride;
  }

  constexpr auto Ruler::intersect
    ( Ruler const& other
    ) const noexcept -> std::optional<Ruler>
  {
    auto const lo {std::max (_begin, other._begin)};
    auto const hi {std::min (_end, other._end)};

    if (! (lo < hi))
    {
      return std::nullopt;
    }

    // Extended Euclid: returns gcd(a, b) and the Bezout coefficient x
    // with x*a + y*b == gcd(a, b) for some y. Pre: a > 0, b > 0.
    auto const extended_gcd
      { [] (Coordinate a, Coordinate b) noexcept
          -> std::tuple<Coordinate, Coordinate>
        {
          auto s {Coordinate {1}};
          auto u {Coordinate {0}};

          while (b != 0)
          {
            auto const q {a / b};

            a = std::exchange (b, a - q * b);
            s = std::exchange (u, s - q * u);
          }

          return {a, s};
        }
      };

    auto const [g, u] {extended_gcd (_stride, other._stride)};

    auto const delta {other._begin - _begin};

    if (delta % g != 0)
    {
      return std::nullopt;
    }

    auto const lcm {(_stride / g) * other._stride};
    auto const m {other._stride / g};

    // i0 = u * (delta / g) (mod m), normalized to [0, m).
    auto i0 {(u * (delta / g)) % m};
    if (i0 < 0)
    {
      i0 += m;
    }

    auto first {_begin + i0 * _stride};

    if (first < lo)
    {
      auto const need {lo - first};
      auto const k {(need + lcm - 1) / lcm};
      first += k * lcm;
    }

    if (! (first < hi))
    {
      return std::nullopt;
    }

    auto const last {first + ((hi - 1 - first) / lcm) * lcm};

    return Ruler {UNCHECKED{}, first, lcm, last + lcm};
  }

  constexpr auto Ruler::is_extended_by
    ( Ruler const& other
    ) const noexcept -> bool
  {
    assert (! (other.lub() < begin()));

    if (is_singleton())
    {
      if (other.is_singleton())
      {
        return true;
      }

      return lub() + other.stride() == other.begin();
    }

    if (other.is_singleton())
    {
      return end() == other.begin();
    }

    return other.stride() == stride() && end() == other.begin();
  }

  constexpr auto Ruler::extend_with (Ruler const& other) noexcept -> void
  {
    assert (is_extended_by (other));

    if (is_singleton())
    {
      _stride = other.begin() - begin();
    }

    _end = other.lub() + _stride;

#if IPT_BENCHMARK_CACHE_RULER_SIZE
    _size = compute_size();
#endif
  }

  constexpr auto Ruler::compute_size() const noexcept -> Index
  {
    assert (_stride > 0);

    return (_end - _begin) / _stride;
  }
}
