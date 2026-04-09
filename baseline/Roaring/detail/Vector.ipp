namespace ipt::baseline::roaring::storage
{
  template<std::size_t D>
    Vector<D>::Vector
      ( ipt::baseline::grid::ReservedLayout<D> layout
      )
        : _layout {layout}
  {}

  template<std::size_t D>
    auto Vector<D>::bitmap
      (
      ) const noexcept -> ::roaring::Roaring64Map const&
  {
    return _bitmap;
  }

  template<std::size_t D>
    auto Vector<D>::bitmap_mutable
      (
      ) noexcept -> ::roaring::Roaring64Map&
  {
    return _bitmap;
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
