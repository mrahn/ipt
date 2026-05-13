namespace ipt::baseline::row_csr::storage
{
  template<std::size_t D, std::size_t K>
    auto Vector<D, K>::row_keys
      (
      ) const noexcept -> std::span<RowKey<D, K> const>
  {
    return std::span<RowKey<D, K> const> {_row_keys};
  }

  template<std::size_t D, std::size_t K>
    auto Vector<D, K>::row_offsets
      (
      ) const noexcept -> std::span<ipt::Index const>
  {
    return std::span<ipt::Index const> {_row_offsets};
  }

  template<std::size_t D, std::size_t K>
    auto Vector<D, K>::column_values
      (
      ) const noexcept -> std::span<ColumnValue<D, K> const>
  {
    return std::span<ColumnValue<D, K> const> {_column_values};
  }

  template<std::size_t D, std::size_t K>
    auto Vector<D, K>::push_row_key (RowKey<D, K> const& key) -> void
  {
    _row_keys.push_back (key);
  }

  template<std::size_t D, std::size_t K>
    auto Vector<D, K>::push_row_offset (ipt::Index off) -> void
  {
    _row_offsets.push_back (off);
  }

  template<std::size_t D, std::size_t K>
    auto Vector<D, K>::push_column_value (ColumnValue<D, K> const& v) -> void
  {
    _column_values.push_back (v);
  }

  template<std::size_t D, std::size_t K>
    auto Vector<D, K>::last_row_key
      (
      ) const noexcept -> RowKey<D, K> const&
  {
    return _row_keys.back();
  }

  template<std::size_t D, std::size_t K>
    auto Vector<D, K>::row_keys_empty() const noexcept -> bool
  {
    return _row_keys.empty();
  }
}
