#pragma once

#include <baseline/storage/Header.hpp>
#include <cstddef>
#include <filesystem>
#include <span>
#include <string_view>

namespace ipt::baseline::storage
{
  // POSIX mmap-backed file handle. Opens the file read-only, mmaps
  // its full extent, copies the Header out into a local for safe
  // access, and validates magic/version/dimension/cache flags.
  //
  // The Kind and dimension validation is done by the caller because
  // the expected values are baseline-specific. MappedFile only
  // validates the universal fields (magic, version, cache flags,
  // file size >= payload_offset).
  //
  // Non-copyable, non-movable: hold by reference or unique_ptr.
  //
  class MappedFile
  {
  public:
    [[nodiscard]] explicit MappedFile (std::filesystem::path const&);

    MappedFile (MappedFile const&) = delete;
    MappedFile (MappedFile&&) = delete;
    auto operator= (MappedFile const&) -> MappedFile& = delete;
    auto operator= (MappedFile&&) -> MappedFile& = delete;

    ~MappedFile();

    [[nodiscard]] auto header() const noexcept -> Header const&;

    [[nodiscard]] auto payload() const noexcept -> std::span<std::byte const>;

    [[nodiscard]] auto description() const noexcept -> std::string_view;

    [[nodiscard]] auto path
      (
      ) const noexcept -> std::filesystem::path const&
      ;

  private:
    std::filesystem::path _path;
    void* _base {nullptr};
    std::size_t _length {0};
    Header _header{};
  };

  // Validate that the loaded header matches the baseline-specific
  // expectations. Throws std::runtime_error on mismatch.
  auto validate_kind_and_dimension
    ( MappedFile const& file
    , Kind expected_kind
    , std::uint32_t expected_dimension
    , std::uint32_t expected_sub_dimension = 0
    ) -> void;
}

#include "detail/MappedFile.ipp"
