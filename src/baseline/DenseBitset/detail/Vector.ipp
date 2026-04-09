#include <utility>

namespace ipt::baseline::dense_bitset::storage
{
  template<std::size_t D>
    Vector<D>::Vector
      ( ipt::baseline::grid::ReservedLayout<D> layout
      , std::size_t word_count
      )
        : _layout {layout}
        , _bits (word_count, Word {0})
  {}

  template<std::size_t D>
    auto Vector<D>::bits() const noexcept -> std::span<Word const>
  {
    return std::span<Word const> {_bits};
  }

  template<std::size_t D>
    auto Vector<D>::bits_mutable() noexcept -> std::span<Word>
  {
    return std::span<Word> {_bits};
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
