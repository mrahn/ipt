#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <ipt/Entry.hpp>
#include <type_traits>

namespace ipt::storage
{
  // Magic value: ASCII bytes "IPTSTORE", little-endian-packed into a
  // u64 so that the first 8 bytes of a serialized file read as the
  // string "IPTSTORE" on the host.
  inline constexpr std::uint64_t magic_value
    { (std::uint64_t {'I'} << 0)
    | (std::uint64_t {'P'} << 8)
    | (std::uint64_t {'T'} << 16)
    | (std::uint64_t {'S'} << 24)
    | (std::uint64_t {'T'} << 32)
    | (std::uint64_t {'O'} << 40)
    | (std::uint64_t {'R'} << 48)
    | (std::uint64_t {'E'} << 56)
    };

  // Format version: YYYYMMDD of the format introduction.
  inline constexpr std::uint32_t format_version {20260429u};

  // Page-aligned offset at which the entry array starts. Picked as a
  // 4 KiB constant so that the entry array begins at a mapped-page
  // boundary on the platforms this benchmark targets.
  inline constexpr std::size_t entries_offset {4096};

  // Description capacity in bytes (1 KiB). The last byte is reserved
  // for a terminating NUL so the description can always be exposed as
  // a bounded std::string_view.
  inline constexpr std::size_t description_capacity {1024};
  inline constexpr std::size_t description_max {description_capacity - 1};

  // Sizes of the fixed (named) header fields.
  inline constexpr std::size_t header_fixed_size
    { sizeof (std::uint64_t)   // magic
    + sizeof (std::uint32_t)   // version
    + sizeof (std::uint32_t)   // dimension
    + sizeof (std::uint32_t)   // entry_size
    + sizeof (std::uint32_t)   // _pad0
    + sizeof (std::uint64_t)   // entry_count
    + sizeof (std::uint32_t)   // cache_cuboid_size
    + sizeof (std::uint32_t)   // cache_ruler_size
    + sizeof (std::uint32_t)   // cache_entry_end
    + sizeof (std::uint32_t)   // cache_entry_lub
    };

  // Reserved bytes between the fixed fields and the description, to
  // leave room for future named fields without shifting either the
  // description offset or the entry-array offset.
  inline constexpr std::size_t header_reserved_size
    {entries_offset - header_fixed_size - description_capacity};

  struct Header
  {
    std::uint64_t magic;
    std::uint32_t version;
    std::uint32_t dimension;
    std::uint32_t entry_size;
    std::uint32_t _pad0; // alignment padding for entry_count
    std::uint64_t entry_count;

    // One field per IPT_BENCHMARK_CACHE_* macro, stored as 0 / 1.
    // Kept unpacked so the header is self-describing in a hex dump.
    std::uint32_t cache_cuboid_size;
    std::uint32_t cache_ruler_size;
    std::uint32_t cache_entry_end;
    std::uint32_t cache_entry_lub;

    std::array<std::byte, header_reserved_size> reserved;
    std::array<char, description_capacity> description;
  };

  static_assert (sizeof (Header) == entries_offset);
  static_assert (std::is_trivially_copyable_v<Header>);
  static_assert (std::is_standard_layout_v<Header>);

  // Compile-time values for the four cache flags, used for header
  // population on write and equality checking on load.
  inline constexpr std::uint32_t cache_cuboid_size_value
    {IPT_BENCHMARK_CACHE_CUBOID_SIZE != 0 ? 1u : 0u};
  inline constexpr std::uint32_t cache_ruler_size_value
    {IPT_BENCHMARK_CACHE_RULER_SIZE != 0 ? 1u : 0u};
  inline constexpr std::uint32_t cache_entry_end_value
    {IPT_BENCHMARK_CACHE_ENTRY_END != 0 ? 1u : 0u};
  inline constexpr std::uint32_t cache_entry_lub_value
    {IPT_BENCHMARK_CACHE_ENTRY_LUB != 0 ? 1u : 0u};
}
