namespace ipt::create
{
  template<std::size_t D>
    auto add_cuboid_and_merge
      ( std::vector<Entry<D>>& entries
      , Cuboid<D> const& cuboid
      ) -> void
  {
    entries.push_back
      ( Entry<D> {cuboid, entries.empty() ? 0 : entries.back().end()}
      );

    while (  entries.size() > 1
          && entries[entries.size() - 2]
             .merge_if_mergeable (entries[entries.size() - 1])
          )
    {
      entries.pop_back();
    }
  }
}
