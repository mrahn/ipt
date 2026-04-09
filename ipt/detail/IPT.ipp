#include <algorithm>
#include <cassert>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <utility>

namespace ipt
{
  template<std::size_t D>
    constexpr IPT<D>::IPT (std::vector<Entry<D>> entries)
      : _entries {std::move (entries)}
  {
    if (_entries.empty())
    {
      throw std::invalid_argument {"IPT::IPT: entries must not be empty"};
    }

    assert
      ( std::ranges::all_of
        ( std::views::iota (std::size_t {0}, _entries.size())
        , [this] (std::size_t index) noexcept
          {
            if (index == 0)
            {
              return _entries.front().begin() == Index {0};
            }

            return _entries[index].begin() == _entries[index - 1].end();
          }
        )
      );
  }

  template<std::size_t D>
    constexpr auto IPT<D>::size() const noexcept -> Index
  {
    return _entries.back().end();
  }

  template<std::size_t D>
    constexpr auto IPT<D>::at (Index index) const -> Point<D>
  {
    if (_entries.size() == 1)
    {
      if (! (index < size()))
      {
        throw std::out_of_range {"IPT::at: index out of range"};
      }

      return _entries.front().UNCHECKED_at (index);
    }

    auto const entry
      { std::ranges::upper_bound
        ( _entries
        , index
        , {}
        , [] (Entry<D> const& entry) noexcept
          {
            return entry.end();
          }
        )
      };

    if (entry == std::ranges::end (_entries))
    {
      throw std::out_of_range {"IPT::at: index out of range"};
    }

    return entry->UNCHECKED_at (index);
  }

  template<std::size_t D>
    constexpr auto IPT<D>::pos (Point<D> point) const -> Index
  {
    if (_entries.size() == 1)
    {
      return _entries.front().pos (point);
    }

    auto const entry
      { std::ranges::lower_bound
        ( _entries
        , point
        , std::ranges::less{}
        , [] (Entry<D> const& entry) noexcept
          {
            return entry.lub();
          }
        )
      };

    if (entry == std::ranges::end (_entries))
    {
      throw std::out_of_range {"IPT::pos: point out of range"};
    }

    return entry->pos (point);
  }

  template<std::size_t D>
    constexpr auto IPT<D>::bytes() const noexcept -> std::size_t
  {
    return number_of_entries() * sizeof (Entry<D>);
  }

  template<std::size_t D>
    constexpr auto IPT<D>::number_of_entries() const noexcept -> std::size_t
  {
    return _entries.size();
  }

  template<std::size_t D>
    template<std::ranges::input_range R>
      requires std::same_as<std::ranges::range_value_t<R>, Cuboid<D>>
    constexpr IPT<D>::IPT (std::from_range_t, R&& cuboids)
      : _entries
        { std::forward<R> (cuboids)
        | std::views::transform
          ( [running = Index {0}] (auto&& cuboid) mutable
            {
              auto const cuboid_size {cuboid.size()};
              return Entry<D>
                { std::forward<decltype (cuboid)> (cuboid)
                , std::exchange (running, running + cuboid_size)
                };
            }
          )
        | std::ranges::to<std::vector<Entry<D>>>()
        }
  {
    if (_entries.empty())
    {
      throw std::invalid_argument
        {"IPT::IPT(from_range): cuboid range must not be empty"};
    }

    assert
      ( std::ranges::is_sorted
        ( _entries
        , std::ranges::less{}
        , [] (Entry<D> const& entry) noexcept
          {
            return entry.cuboid().glb();
          }
        )
      );
  }

  template<std::size_t D>
    constexpr auto IPT<D>::select
      ( Cuboid<D> query
      ) const noexcept -> SelectView<D>
  {
    if (_entries.empty())
    {
      return {};
    }

    auto const query_glb {query.glb()};
    auto const query_lub {query.lub()};

    // First entry whose lub() is not strictly lex-less than query.glb():
    // any earlier entry sits entirely lex-before the query and cannot
    // overlap.
    auto const first
      { std::ranges::lower_bound
        ( _entries
        , query_glb
        , std::ranges::less{}
        , [] (Entry<D> const& entry) noexcept
          {
            return entry.lub();
          }
        )
      };

    // First entry whose glb() is lex-strictly-greater than query.lub(): any
    // such entry sits entirely lex-after the query.  Note that
    // query.lub() is the inclusive lex-maximum (last contained point),
    // so an entry whose glb() equals query.lub() may still overlap the
    // query and must be kept in range.
    auto const last
      { std::ranges::upper_bound
        ( _entries
        , query_lub
        , std::ranges::less{}
        , [] (Entry<D> const& entry) noexcept
          {
            return entry.cuboid().glb();
          }
        )
      };

    return SelectView<D>
      { std::to_address (first)
      , std::to_address (last)
      , std::move (query)
      };
  }
}
