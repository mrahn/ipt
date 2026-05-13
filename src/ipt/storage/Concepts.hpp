#pragma once

#include <concepts>
#include <cstddef>
#include <span>

namespace ipt
{
  template<std::size_t D>
    struct Entry;

  // Read-only storage backend for IPT<D>. Provides a span over the
  // entry array and an equality operator that compares span content.
  //
  // Same-build / same-platform contract: there is no endianness or ABI
  // portability guarantee; on-disk storage formats are only valid for
  // binaries produced from the same source tree with the same compiler
  // and the same IPT_BENCHMARK_CACHE_* flags.
  //
  template<typename S, std::size_t D>
    concept Storage =
      requires (S const& s, S const& t)
    {
      { s.entries() } noexcept -> std::same_as<std::span<Entry<D> const>>;
      { s == t } -> std::same_as<bool>;
    };
}
