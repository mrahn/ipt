#pragma once

#include <cstddef>
#include <filesystem>
#include <ipt/Entry.hpp>
#include <ipt/storage/Concepts.hpp>
#include <ipt/storage/Header.hpp>
#include <span>
#include <string_view>

namespace ipt::storage
{
  // Read-only IPT storage backed by a POSIX mmap of a file written
  // with ipt::storage::write. The contract is same-build /
  // same-platform: load is rejected unless every header field
  // (magic, version, dimension, entry_size, cache_flags) matches the
  // current binary's compile-time values.
  //
  // MMap is non-copyable and non-movable. Hold it by reference or by
  // std::unique_ptr if ownership must be transferred.
  //
  template<std::size_t D>
    struct MMap
  {
    [[nodiscard]] explicit MMap (std::filesystem::path const&);

    MMap (MMap const&) = delete;
    MMap (MMap&&) = delete;
    auto operator= (MMap const&) -> MMap& = delete;
    auto operator= (MMap&&) -> MMap& = delete;

    ~MMap();

    [[nodiscard]] auto entries
      (
      ) const noexcept -> std::span<Entry<D> const>
      ;

    [[nodiscard]] auto description() const noexcept -> std::string_view;
    [[nodiscard]] auto operator== (MMap const& other) const noexcept -> bool;

  private:
    void* _base {nullptr};
    std::size_t _length {0};
    std::span<Entry<D> const> _entries{};
    Header _header{};
  };
}

#include "detail/MMap.ipp"
