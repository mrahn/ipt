#include <algorithm>
#include <functional>
#include <ipt/create/add_cuboid_and_merge.hpp>
#include <ranges>
#include <utility>

namespace ipt::create
{
  namespace
  {
    template<std::size_t D>
      [[nodiscard]] auto bounding_box
        ( Cuboid<D> const& lhs
        , Cuboid<D> const& rhs
        ) noexcept -> Cuboid<D>
    {
      return Cuboid<D>
        { std::invoke
          ( [&]<std::size_t... Ds> (std::index_sequence<Ds...>) noexcept
            {
              return std::array<Ruler, D>
                { lhs.ruler (Ds).make_bounding_ruler (rhs.ruler (Ds))...
                };
            }
          , std::make_index_sequence<D>{}
          )
        };
    }

    template<std::size_t D>
      [[nodiscard]] auto is_popable_column_in_dim0
        ( Cuboid<D> const& cuboid
        ) noexcept -> bool
    {
      if constexpr (D == 0)
      {
        return false;
      }

      if (cuboid.ruler (0).is_singleton())
      {
        return false;
      }

      return std::ranges::all_of
        ( std::views::iota (std::size_t {1}, D)
        , [&cuboid] (std::size_t d)
          {
            return cuboid.ruler (d).is_singleton();
          }
        );
    }

    template<std::size_t D>
      [[nodiscard]] auto pop_back_dim0_column
        ( Cuboid<D> const& cuboid
        ) noexcept -> Cuboid<D>
    {
      return Cuboid<D>
        { std::invoke
          ( [&]<std::size_t... Ds> (std::index_sequence<Ds...>) noexcept
            {
              return std::array<Ruler, D>
                { (Ds == 0 ? cuboid.ruler (0).pop_back() : cuboid.ruler (Ds))...
                };
            }
          , std::make_index_sequence<D>{}
          )
        };
    }

    template<std::size_t D>
      [[nodiscard]] auto try_combine (std::vector<Entry<D>>& entries) -> bool
    {
      if (entries.size() < 3)
      {
        return false;
      }

      auto& a {entries[entries.size() - 3]};

      if (!is_popable_column_in_dim0 (a.cuboid()))
      {
        return false;
      }

      auto& b {entries[entries.size() - 2]};
      auto& c {entries[entries.size() - 1]};

      auto const k {bounding_box (b.cuboid(), c.cuboid())};

      if (k.size() != b.size() + c.size() + 1)
      {
        return false;
      }

      if (a.lub() != k.glb())
      {
        return false;
      }

      auto const a_shrunken_cuboid {pop_back_dim0_column (a.cuboid())};
      auto const a_shrunken_begin {a.begin()};

      entries.erase ( std::ranges::end (entries) - 3
                    , std::ranges::end (entries)
                    );

      entries.emplace_back
        ( k
        , entries.emplace_back (a_shrunken_cuboid, a_shrunken_begin).end()
        );

      return true;
    }
  }

  template<std::size_t D>
    constexpr auto GreedyPlusCombine<D>::name() noexcept
  {
    return "greedy-plus-combine";
  }

  template<std::size_t D>
    auto GreedyPlusCombine<D>::add (Point<D> const& point) -> void
  {
    add_cuboid_and_merge
      ( _entries
      , Cuboid<D> {typename Cuboid<D>::Singleton {point}}
      );

    while (try_combine (_entries))
    {
      auto const tail {_entries.back().cuboid()};
      _entries.pop_back();
      add_cuboid_and_merge (_entries, tail);
    }
  }

  template<std::size_t D>
    auto GreedyPlusCombine<D>::build() && -> IPT<D>
  {
    return IPT<D> {std::move (_entries)};
  }
}
