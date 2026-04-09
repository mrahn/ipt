#pragma once

#include <array>
#include <cstddef>

namespace ipt::baseline
{
  template<typename T, std::size_t D>
    [[nodiscard]] constexpr auto zeros() noexcept -> std::array<T, D>;

  template<typename T, std::size_t D>
    [[nodiscard]] constexpr auto ones() noexcept -> std::array<T, D>;
}

#include "detail/zeros_ones.ipp"
