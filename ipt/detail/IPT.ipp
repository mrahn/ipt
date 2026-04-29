#include <algorithm>
#include <cassert>
#include <concepts>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <utility>

namespace ipt
{
  namespace detail
  {
    template<std::size_t D>
      inline auto validate_entries (std::span<Entry<D> const> entries) -> void
    {
      if (entries.empty())
      {
        throw std::invalid_argument {"IPT::IPT: entries must not be empty"};
      }

      assert
        ( std::ranges::all_of
          ( std::views::iota (std::size_t {0}, entries.size())
          , [entries] (std::size_t index) noexcept
            {
              if (index == 0)
              {
                return entries.front().begin() == Index {0};
              }

              return entries[index].begin() == entries[index - 1].end();
            }
          )
        );
    }
  }

  template<std::size_t D, Storage<D> Storage>
    constexpr IPT<D, Storage>::IPT (Storage storage)
      : _storage {std::move (storage)}
  {
    detail::validate_entries<D> (_storage.entries());
  }

  template<std::size_t D, Storage<D> Storage>
    template<typename... Args>
      requires std::constructible_from<Storage, Args&&...>
    constexpr IPT<D, Storage>::IPT (std::in_place_t, Args&&... args)
      : _storage {std::forward<Args> (args)...}
  {
    detail::validate_entries<D> (_storage.entries());
  }

  template<std::size_t D, Storage<D> Storage>
    constexpr IPT<D, Storage>::IPT (std::vector<Entry<D>> entries)
      requires std::same_as<Storage, storage::Vector<D>>
        : IPT {Storage {std::move (entries)}}
  {}

  template<std::size_t D, Storage<D> Storage>
    constexpr auto IPT<D, Storage>::size() const noexcept -> Index
  {
    return _storage.entries().back().end();
  }

  template<std::size_t D, Storage<D> Storage>
    constexpr auto IPT<D, Storage>::at (Index index) const -> Point<D>
  {
    auto const entries {_storage.entries()};

    if (entries.size() == 1)
    {
      if (! (index < size()))
      {
        throw std::out_of_range {"IPT::at: index out of range"};
      }

      return entries.front().UNCHECKED_at (index);
    }

    auto const entry
      { std::ranges::upper_bound
        ( entries
        , index
        , {}
        , [] (Entry<D> const& entry) noexcept
          {
            return entry.end();
          }
        )
      };

    if (entry == std::ranges::end (entries))
    {
      throw std::out_of_range {"IPT::at: index out of range"};
    }

    return entry->UNCHECKED_at (index);
  }

  template<std::size_t D, Storage<D> Storage>
    constexpr auto IPT<D, Storage>::pos (Point<D> point) const -> Index
  {
    auto const entries {_storage.entries()};

    if (entries.size() == 1)
    {
      return entries.front().pos (point);
    }

    auto const entry
      { std::ranges::lower_bound
        ( entries
        , point
        , std::ranges::less{}
        , [] (Entry<D> const& entry) noexcept
          {
            return entry.lub();
          }
        )
      };

    if (entry == std::ranges::end (entries))
    {
      throw std::out_of_range {"IPT::pos: point out of range"};
    }

    return entry->pos (point);
  }

  template<std::size_t D, Storage<D> Storage>
    constexpr auto IPT<D, Storage>::try_pos
      ( Point<D> point
      ) const -> std::optional<Index>
  {
    auto const entries {_storage.entries()};

    if (entries.size() == 1)
    {
      return entries.front().try_pos (point);
    }

    auto const entry
      { std::ranges::lower_bound
        ( entries
        , point
        , std::ranges::less{}
        , [] (Entry<D> const& entry) noexcept
          {
            return entry.lub();
          }
        )
      };

    if (entry == std::ranges::end (entries))
    {
      return std::nullopt;
    }

    return entry->try_pos (point);
  }

  template<std::size_t D, Storage<D> Storage>
    constexpr auto IPT<D, Storage>::bytes() const noexcept -> std::size_t
  {
    return number_of_entries() * sizeof (Entry<D>);
  }

  template<std::size_t D, Storage<D> Storage>
    constexpr auto IPT<D, Storage>::number_of_entries
      (
      ) const noexcept -> std::size_t
  {
    return _storage.entries().size();
  }

  template<std::size_t D, Storage<D> Storage>
    template<std::ranges::input_range R>
      requires std::same_as<Storage, storage::Vector<D>>
            && std::same_as<std::ranges::range_value_t<R>, Cuboid<D>>
    constexpr IPT<D, Storage>::IPT (std::from_range_t, R&& cuboids)
      : IPT
        { Storage
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
        }
  {
    auto const entries {_storage.entries()};

    if (entries.empty())
    {
      throw std::invalid_argument
        {"IPT::IPT(from_range): cuboid range must not be empty"};
    }

    assert
      ( std::ranges::is_sorted
        ( entries
        , std::ranges::less{}
        , [] (Entry<D> const& entry) noexcept
          {
            return entry.cuboid().glb();
          }
        )
      );
  }

  template<std::size_t D, Storage<D> Storage>
    constexpr auto IPT<D, Storage>::select
      ( Cuboid<D> query
      ) const noexcept -> SelectView<D>
  {
    auto const entries {_storage.entries()};

    if (entries.empty())
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
        ( entries
        , query_glb
        , std::ranges::less{}
        , [] (Entry<D> const& entry) noexcept
          {
            return entry.lub();
          }
        )
      };

    // First entry whose glb() is lex-strictly-greater than query.lub():
    // any such entry sits entirely lex-after the query.  Note that
    // query.lub() is the inclusive lex-maximum (last contained point),
    // so an entry whose glb() equals query.lub() may still overlap the
    // query and must be kept in range.
    auto const last
      { std::ranges::upper_bound
        ( entries
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

  template<std::size_t D, Storage<D> Storage>
    constexpr auto IPT<D, Storage>::operator==
      ( IPT const& other
      ) const noexcept -> bool
  {
    return _storage == other._storage;
  }

  template<std::size_t D, Storage<D> Storage>
    constexpr auto IPT<D, Storage>::entries_view
      (
      ) const noexcept -> std::span<Entry<D> const>
  {
    return _storage.entries();
  }
}
