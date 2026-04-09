namespace ipt::baseline::lex_run::storage
{
  template<std::size_t D>
    auto Vector<D>::runs() const noexcept -> std::span<Run<D> const>
  {
    return std::span<Run<D> const> {_runs};
  }

  template<std::size_t D>
    auto Vector<D>::total_points() const noexcept -> ipt::Index
  {
    return _total_points;
  }

  template<std::size_t D>
    auto Vector<D>::push_back (Run<D> const& run) -> void
  {
    _runs.push_back (run);
  }

  template<std::size_t D>
    auto Vector<D>::back() noexcept -> Run<D>&
  {
    return _runs.back();
  }

  template<std::size_t D>
    auto Vector<D>::empty() const noexcept -> bool
  {
    return _runs.empty();
  }

  template<std::size_t D>
    auto Vector<D>::bump_total_points() noexcept -> void
  {
    _total_points += 1;
  }
}
