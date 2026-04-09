#pragma once

#include <array>
#include <baseline/storage/Header.hpp>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string_view>

namespace ipt::baseline::storage
{
  // Build a NUL-padded description array from `description`, throwing
  // if it does not fit.
  [[nodiscard]] auto make_description
    ( std::string_view
    ) -> std::array<char, description_capacity>
    ;

  // Pack a trivially-copyable value into the header's reserved area.
  template<typename T>
    [[nodiscard]] auto make_reserved
      ( T const&
      ) -> std::array<std::byte, header_reserved_size>
      ;

  // All baseline-specific fields needed to build a Header. Callers
  // pass a fully-populated HeaderInit; make_header fills in the
  // universal fields (magic, version, cache flags) and returns the
  // complete Header in a single expression.
  struct HeaderInit
  {
    Kind kind;
    std::uint32_t dimension;
    std::uint64_t payload_size;
    std::uint64_t element_count;
    std::array<std::uint64_t, section_count> section_bytes;
    std::string_view description {};
    std::uint32_t sub_dimension {0u};
    std::array<std::byte, header_reserved_size> reserved {};
  };

  [[nodiscard]] auto make_header (HeaderInit) -> Header;

  // Open `path` for binary writing, write the populated `header`
  // followed by each section in `sections` in order. The total
  // payload size and section_bytes must match the header. Throws
  // std::runtime_error on I/O failure.
  auto write_with_sections
    ( std::filesystem::path const&
    , Header const&
    , std::span<std::span<std::byte const> const>
    ) -> void
    ;
}

#include "detail/Write.ipp"
