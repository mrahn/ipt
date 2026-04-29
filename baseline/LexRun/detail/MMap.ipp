#include <format>
#include <stdexcept>

namespace ipt::baseline::lex_run::storage
{
  template<std::size_t D>
    MMap<D>::MMap (std::filesystem::path const& path)
      : _file {path}
  {
    ipt::baseline::storage::validate_kind_and_dimension
      (_file, ipt::baseline::storage::Kind::LexRun, D);

    auto const& header {_file.header()};
    auto const run_bytes {header.section_bytes[0]};

    if (run_bytes % sizeof (Run<D>) != 0)
    {
      throw std::runtime_error
        { std::format
            ( "ipt::baseline::lex_run::MMap: '{}': section[0] size {} not a"
              " multiple of sizeof(Run<{}>)={}"
            , path.string(), run_bytes, D, sizeof (Run<D>)
            )
        };
    }

    if (header.payload_size < run_bytes)
    {
      throw std::runtime_error
        { std::format
            ( "ipt::baseline::lex_run::MMap: '{}': payload truncated"
            , path.string()
            )
        };
    }

    auto const* const data
      { reinterpret_cast<Run<D> const*> (_file.payload().data())
      };
    _runs = std::span<Run<D> const>
      {data, static_cast<std::size_t> (run_bytes / sizeof (Run<D>))};
    _total_points = static_cast<ipt::Index> (header.element_count);
  }

  template<std::size_t D>
    auto MMap<D>::runs() const noexcept -> std::span<Run<D> const>
  {
    return _runs;
  }

  template<std::size_t D>
    auto MMap<D>::total_points() const noexcept -> ipt::Index
  {
    return _total_points;
  }

  template<std::size_t D>
    auto MMap<D>::description() const noexcept -> std::string_view
  {
    return _file.description();
  }
}
