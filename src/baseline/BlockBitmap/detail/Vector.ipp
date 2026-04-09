namespace ipt::baseline::block_bitmap::storage
{
  template<std::size_t D>
    Vector<D>::Vector
      ( ipt::baseline::grid::ReservedLayout<D> layout
      , std::size_t tile_count
      )
        : _layout {layout}
        , _tiles (tile_count, Word {0})
  {}

  template<std::size_t D>
    auto Vector<D>::tiles() const noexcept -> std::span<Word const>
  {
    return std::span<Word const> {_tiles};
  }

  template<std::size_t D>
    auto Vector<D>::tiles_mutable() noexcept -> std::span<Word>
  {
    return std::span<Word> {_tiles};
  }

  template<std::size_t D>
    auto Vector<D>::point_count() const noexcept -> ipt::Index
  {
    return _point_count;
  }

  template<std::size_t D>
    auto Vector<D>::bump_point_count() noexcept -> void
  {
    _point_count += 1;
  }

  template<std::size_t D>
    auto Vector<D>::layout
      (
      ) const noexcept -> ipt::baseline::grid::ReservedLayout<D> const&
  {
    return _layout;
  }
}
