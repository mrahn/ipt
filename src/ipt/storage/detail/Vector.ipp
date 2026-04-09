#include <utility>

namespace ipt::storage
{
  template<std::size_t D>
    Vector<D>::Vector (std::vector<Entry<D>> entries) noexcept
      : _entries {std::move (entries)}
  {}

  template<std::size_t D>
    auto Vector<D>::entries() const noexcept -> std::span<Entry<D> const>
  {
    return {_entries.data(), _entries.size()};
  }

  template<std::size_t D>
    auto Vector<D>::operator==
      ( Vector const& other
      ) const noexcept -> bool
  {
    return _entries == other._entries;
  }
}
