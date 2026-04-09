#include <utility>

namespace ipt::baseline::sorted_points::storage
{
  template<std::size_t D>
    Vector<D>::Vector (std::vector<ipt::Point<D>> points) noexcept
      : _points {std::move (points)}
  {}

  template<std::size_t D>
    auto Vector<D>::points
      (
      ) const noexcept -> std::span<ipt::Point<D> const>
  {
    return std::span<ipt::Point<D> const> {_points};
  }

  template<std::size_t D>
    auto Vector<D>::push_back (ipt::Point<D> const& p) -> void
  {
    _points.push_back (p);
  }

  template<std::size_t D>
    auto Vector<D>::last() const noexcept -> ipt::Point<D> const&
  {
    return _points.back();
  }

  template<std::size_t D>
    auto Vector<D>::empty() const noexcept -> bool
  {
    return _points.empty();
  }
}
