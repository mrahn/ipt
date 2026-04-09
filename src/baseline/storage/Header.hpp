#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace ipt::baseline::storage
{
  // Magic value: ASCII bytes "BENCHSTO", little-endian-packed into a
  // u64 so that the first 8 bytes of a serialized file read as the
  // string "BENCHSTO" on the host. Distinct from the IPT magic
  // "IPTSTORE" so a baseline file cannot be mis-loaded as an IPT
  // file (or vice versa).
  inline constexpr std::uint64_t magic_value
    { (std::uint64_t {'B'} << 0)
    | (std::uint64_t {'E'} << 8)
    | (std::uint64_t {'N'} << 16)
    | (std::uint64_t {'C'} << 24)
    | (std::uint64_t {'H'} << 32)
    | (std::uint64_t {'S'} << 40)
    | (std::uint64_t {'T'} << 48)
    | (std::uint64_t {'O'} << 56)
    };

  // Format version: YYYYMMDD of the format introduction.
  inline constexpr std::uint32_t format_version {20260429u};

  // Page-aligned offset at which the baseline-specific payload
  // begins. Picked as 4 KiB so that the payload starts at a mapped
  // page boundary.
  inline constexpr std::size_t payload_offset {4096};

  // Description capacity in bytes (1 KiB). The last byte is reserved
  // for a terminating NUL so the description can always be exposed
  // as a bounded std::string_view.
  inline constexpr std::size_t description_capacity {1024};
  inline constexpr std::size_t description_max {description_capacity - 1};

  // One-octet identifier per supported baseline. Stored in the
  // header so a load-time check can refuse to mmap a file that was
  // written by a different baseline.
  enum class Kind : std::uint32_t
  {
    SortedPoints = 1,
    DenseBitset  = 2,
    BlockBitmap  = 3,
    LexRun       = 4,
    RowCSR       = 5,
    Roaring      = 6,
    Grid         = 7,
  };

  // Eight 64-bit slots reserved for baseline-specific section sizes
  // (in bytes). Layout is documented per baseline in its own
  // detail/MMap.ipp. Unused slots are written as zero.
  inline constexpr std::size_t section_count {8};

  inline constexpr std::size_t header_fixed_size
    { sizeof (std::uint64_t)                     // magic
    + sizeof (std::uint32_t)                     // version
    + sizeof (std::uint32_t)                     // kind
    + sizeof (std::uint32_t)                     // dimension
    + sizeof (std::uint32_t)                     // sub_dimension
    + sizeof (std::uint64_t)                     // payload_size
    + sizeof (std::uint64_t)                     // element_count
    + sizeof (std::uint32_t)                     // cache_cuboid_size
    + sizeof (std::uint32_t)                     // cache_ruler_size
    + sizeof (std::uint32_t)                     // cache_entry_end
    + sizeof (std::uint32_t)                     // cache_entry_lub
    + section_count * sizeof (std::uint64_t)     // section_bytes
    };

  inline constexpr std::size_t header_reserved_size
    {payload_offset - header_fixed_size - description_capacity};

  struct Header
  {
    std::uint64_t magic;
    std::uint32_t version;
    std::uint32_t kind;
    std::uint32_t dimension;
    std::uint32_t sub_dimension;
    std::uint64_t payload_size;
    std::uint64_t element_count;

    std::uint32_t cache_cuboid_size;
    std::uint32_t cache_ruler_size;
    std::uint32_t cache_entry_end;
    std::uint32_t cache_entry_lub;

    std::array<std::uint64_t, section_count> section_bytes;

    std::array<std::byte, header_reserved_size> reserved;
    std::array<char, description_capacity> description;
  };

  static_assert (sizeof (Header) == payload_offset);
  static_assert (std::is_trivially_copyable_v<Header>);
  static_assert (std::is_standard_layout_v<Header>);

  // Compile-time values for the four cache flags, matching
  // ipt::storage::Header.
  inline constexpr std::uint32_t cache_cuboid_size_value
    {IPT_BENCHMARK_CACHE_CUBOID_SIZE != 0 ? 1u : 0u};
  inline constexpr std::uint32_t cache_ruler_size_value
    {IPT_BENCHMARK_CACHE_RULER_SIZE != 0 ? 1u : 0u};
  inline constexpr std::uint32_t cache_entry_end_value
    {IPT_BENCHMARK_CACHE_ENTRY_END != 0 ? 1u : 0u};
  inline constexpr std::uint32_t cache_entry_lub_value
    {IPT_BENCHMARK_CACHE_ENTRY_LUB != 0 ? 1u : 0u};
}
