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

namespace ipt::storage
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

  template<std::size_t D>
    MMap<D>::MMap (std::filesystem::path const& path)
  {
    auto const fd {::open (path.c_str(), O_RDONLY | O_CLOEXEC)};

    if (fd < 0)
    {
      throw std::runtime_error
        {detail::errno_message ("ipt::storage::MMap: cannot open", path)};
    }

    struct ::stat st{};

    if (::fstat (fd, std::addressof (st)) != 0)
    {
      auto const message
        {detail::errno_message ("ipt::storage::MMap: fstat", path)};
      ::close (fd);
      throw std::runtime_error {message};
    }

    auto const file_size {static_cast<std::size_t> (st.st_size)};

    if (file_size < sizeof (Header))
    {
      ::close (fd);
      throw std::runtime_error
        { std::format
            ( "ipt::storage::MMap: file '{}' is smaller than the header"
              " ({} < {})"
            , path.string(), file_size, sizeof (Header)
            )
        };
    }

    auto* const base
      { ::mmap (nullptr, file_size, PROT_READ, MAP_PRIVATE, fd, 0)
      };

    auto const map_errno {errno};
    ::close (fd);

    if (base == MAP_FAILED)
    {
      errno = map_errno;
      throw std::runtime_error
        {detail::errno_message ("ipt::storage::MMap: mmap", path)};
    }

    // Copy the header out of the mapping into a local so subsequent
    // accesses do not depend on aliasing rules, then validate.
    std::memcpy (std::addressof (_header), base, sizeof (Header));

    auto const reject = [&] (std::string_view message)
    {
      ::munmap (base, file_size);
      throw std::runtime_error
        { std::format
            ("ipt::storage::MMap: '{}': {}", path.string(), message)
        };
    };

    if (_header.magic != magic_value)
    {
      reject ("magic mismatch (not an IPT storage file)");
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

    if (_header.dimension != D)
    {
      reject
        ( std::format
            ( "dimension mismatch (file={}, expected={})"
            , _header.dimension, D
            )
        );
    }

    if (_header.entry_size != sizeof (Entry<D>))
    {
      reject
        ( std::format
            ( "entry size mismatch (file={}, expected={})"
            , _header.entry_size, sizeof (Entry<D>)
            )
        );
    }

    if (  _header.cache_cuboid_size != cache_cuboid_size_value
       || _header.cache_ruler_size != cache_ruler_size_value
       || _header.cache_entry_end != cache_entry_end_value
       || _header.cache_entry_lub != cache_entry_lub_value
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
      {entries_offset + _header.entry_count * sizeof (Entry<D>)};

    if (file_size < expected_size)
    {
      reject
        ( std::format
            ( "file truncated (size={}, expected={})"
            , file_size, expected_size
            )
        );
    }

    // Defensive: ensure description is NUL-terminated within the
    // header, so description() can build a bounded string_view.
    if (_header.description.back() != '\0')
    {
      reject ("description is not NUL-terminated");
    }

    _base = base;
    _length = file_size;

    auto const entry_bytes
      {static_cast<std::byte const*> (base) + entries_offset};

    _entries = std::span<Entry<D> const>
      { reinterpret_cast<Entry<D> const*> (entry_bytes), _header.entry_count
      };
  }

  template<std::size_t D>
    MMap<D>::~MMap()
  {
    if (_base != nullptr)
    {
      ::munmap (_base, _length);
    }
  }

  template<std::size_t D>
    auto MMap<D>::description() const noexcept -> std::string_view
  {
    auto const* const data {_header.description.data()};
    auto const length
      { static_cast<std::size_t>
          (std::find (data, data + description_capacity, '\0') - data)
      };

    return std::string_view {data, length};
  }

  template<std::size_t D>
    auto MMap<D>::operator== (MMap const& other) const noexcept -> bool
  {
    return std::ranges::equal (_entries, other._entries);
  }

  template<std::size_t D>
    auto MMap<D>::entries
      (
      ) const noexcept -> std::span<Entry<D> const>
  {
    return _entries;
  }
}
