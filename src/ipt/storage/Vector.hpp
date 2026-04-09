#pragma once

#include <cstddef>
#include <ipt/Entry.hpp>
#include <span>
#include <vector>

namespace ipt::storage
{
  // Owning, in-memory IPT storage backed by std::vector<Entry<D>>.
  // Default Storage for IPT<D>; semantically identical to the
  // pre-Storage IPT layout.
  //
  template<std::size_t D>
    struct Vector
  {
    [[nodiscard]] explicit Vector (std::vector<Entry<D>>) noexcept;

    [[nodiscard]] auto entries() const noexcept -> std::span<Entry<D> const>;

    [[nodiscard]] auto operator==
      ( Vector const&
      ) const noexcept -> bool
      ;

  private:
    std::vector<Entry<D>> _entries;
  };
}

#include "detail/Vector.ipp"
