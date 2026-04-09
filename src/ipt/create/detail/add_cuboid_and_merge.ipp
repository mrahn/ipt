namespace ipt::create
{
  namespace
  {
    template<std::size_t D>
      auto collapse_tail (std::vector<Entry<D>>& entries) -> void
    {
      while (  entries.size() > 1
            && entries[entries.size() - 2]
               .merge_if_mergeable (entries[entries.size() - 1])
            )
      {
        entries.pop_back();
      }
    }
  }

  template<std::size_t D>
    auto add_cuboid_and_merge
      ( std::vector<Entry<D>>& entries
      , Cuboid<D> const& cuboid
      ) -> void
  {
    entries.emplace_back
      ( Entry<D> {cuboid, entries.empty() ? 0 : entries.back().end()}
      );

    collapse_tail (entries);
  }

  template<std::size_t D>
    auto add_point_and_merge
      ( std::vector<Entry<D>>& entries
      , Point<D> const& point
      ) -> void
  {
    if (!entries.empty() && entries.back().append_if_mergeable (point))
    {
      collapse_tail (entries);

      return;
    }

    add_cuboid_and_merge
      ( entries
      , Cuboid<D> {typename Cuboid<D>::Singleton {point}}
      );
  }
}
