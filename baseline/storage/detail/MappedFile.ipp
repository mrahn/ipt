#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <format>
#include <memory>
#include <stdexcept>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace ipt::baseline::storage
{
  namespace detail
  {
    [[nodiscard]] inline auto errno_message
      ( char const* what
      , std::filesystem::path const& path
      ) -> std::string
    {
      auto const saved {errno};
      return std::format
        ("{} '{}': {}", what, path.string(), std::strerror (saved));
    }
  }

  inline MappedFile::MappedFile (std::filesystem::path const& path)
    : _path {path}
  {
    auto const fd {::open (path.c_str(), O_RDONLY | O_CLOEXEC)};

    if (fd < 0)
    {
      throw std::runtime_error
        { detail::errno_message
            ("ipt::baseline::storage::MappedFile: cannot open", path)
        };
    }

    struct ::stat st{};

    if (::fstat (fd, std::addressof (st)) != 0)
    {
      auto const message
        { detail::errno_message
            ("ipt::baseline::storage::MappedFile: fstat", path)
        };
      ::close (fd);
      throw std::runtime_error {message};
    }

    auto const file_size {static_cast<std::size_t> (st.st_size)};

    if (file_size < sizeof (Header))
    {
      ::close (fd);
      throw std::runtime_error
        { std::format
            ( "ipt::baseline::storage::MappedFile: '{}' is smaller than"
              " the header ({} < {})"
            , path.string(), file_size, sizeof (Header)
            )
        };
    }

    auto* const base
      {::mmap (nullptr, file_size, PROT_READ, MAP_PRIVATE, fd, 0)};

    auto const map_errno {errno};
    ::close (fd);

    if (base == MAP_FAILED)
    {
      errno = map_errno;
      throw std::runtime_error
        { detail::errno_message
            ("ipt::baseline::storage::MappedFile: mmap", path)
        };
    }

    std::memcpy (std::addressof (_header), base, sizeof (Header));

    auto const reject = [&] (std::string_view message)
    {
      ::munmap (base, file_size);
      throw std::runtime_error
        { std::format
            ( "ipt::baseline::storage::MappedFile: '{}': {}"
            , path.string(), message
            )
        };
    };

    if (_header.magic != magic_value)
    {
      reject ("magic mismatch (not a bench storage file)");
    }

    if (_header.version != format_version)
    {
      reject
        ( std::format
            ( "format version mismatch (file={}, expected={})"
            , _header.version, format_version
            )
        );
    }

    if (  _header.cache_cuboid_size != cache_cuboid_size_value
       || _header.cache_ruler_size  != cache_ruler_size_value
       || _header.cache_entry_end   != cache_entry_end_value
       || _header.cache_entry_lub   != cache_entry_lub_value
       )
    {
      reject
        ( std::format
            ( "cache flag mismatch"
              " (file=[{},{},{},{}], expected=[{},{},{},{}])"
            , _header.cache_cuboid_size, _header.cache_ruler_size
            , _header.cache_entry_end, _header.cache_entry_lub
            , cache_cuboid_size_value, cache_ruler_size_value
            , cache_entry_end_value, cache_entry_lub_value
            )
        );
    }

    auto const expected_size
      {payload_offset + static_cast<std::size_t> (_header.payload_size)};

    if (file_size < expected_size)
    {
      reject
        ( std::format
            ( "file truncated (size={}, expected={})"
            , file_size, expected_size
            )
        );
    }

    if (_header.description.back() != '\0')
    {
      reject ("description is not NUL-terminated");
    }

    _base = base;
    _length = file_size;
  }

  inline MappedFile::~MappedFile()
  {
    if (_base != nullptr)
    {
      ::munmap (_base, _length);
    }
  }

  inline auto MappedFile::header() const noexcept -> Header const&
  {
    return _header;
  }

  inline auto MappedFile::path
    (
    ) const noexcept -> std::filesystem::path const&
  {
    return _path;
  }

  inline auto MappedFile::payload
    (
    ) const noexcept -> std::span<std::byte const>
  {
    return std::span<std::byte const>
      { static_cast<std::byte const*> (_base) + payload_offset
      , static_cast<std::size_t> (_header.payload_size)
      };
  }

  inline auto MappedFile::description() const noexcept -> std::string_view
  {
    auto const* const data {_header.description.data()};
    auto const length
      { static_cast<std::size_t>
          (std::find (data, data + description_capacity, '\0') - data)
      };

    return std::string_view {data, length};
  }

  inline auto validate_kind_and_dimension
    ( MappedFile const& file
    , Kind expected_kind
    , std::uint32_t expected_dimension
    , std::uint32_t expected_sub_dimension
    ) -> void
  {
    auto const& header {file.header()};

    if (header.kind != static_cast<std::uint32_t> (expected_kind))
    {
      throw std::runtime_error
        { std::format
            ( "ipt::baseline::storage::MappedFile: '{}': kind mismatch"
              " (file={}, expected={})"
            , file.path().string(), header.kind
            , static_cast<std::uint32_t> (expected_kind)
            )
        };
    }

    if (header.dimension != expected_dimension)
    {
      throw std::runtime_error
        { std::format
            ( "ipt::baseline::storage::MappedFile: '{}': dimension mismatch"
              " (file={}, expected={})"
            , file.path().string(), header.dimension, expected_dimension
            )
        };
    }

    if (header.sub_dimension != expected_sub_dimension)
    {
      throw std::runtime_error
        { std::format
            ( "ipt::baseline::storage::MappedFile: '{}':"
              " sub_dimension mismatch (file={}, expected={})"
            , file.path().string(), header.sub_dimension
            , expected_sub_dimension
            )
        };
    }
  }
}
