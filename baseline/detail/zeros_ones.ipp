#include <algorithm>

namespace ipt::baseline
{
  template<typename T, std::size_t D>
    constexpr auto zeros() noexcept -> std::array<T, D>
  {
    auto zeros {std::array<T, D>{}};
    std::ranges::fill (zeros, T {0});
    return zeros;
  }

  template<typename T, std::size_t D>
    constexpr auto ones() noexcept -> std::array<T, D>
  {
    auto ones {std::array<T, D>{}};
    std::ranges::fill (ones, T {1});
    return ones;
  }
}
