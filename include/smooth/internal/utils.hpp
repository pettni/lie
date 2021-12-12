// smooth: Lie Theory for Robotics
// https://github.com/pettni/smooth
//
// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
//
// Copyright (c) 2021 Petter Nilsson
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef SMOOTH__INTERNAL__UTILS_HPP_
#define SMOOTH__INTERNAL__UTILS_HPP_

#include <array>
#include <cstddef>
#include <functional>
#include <numeric>
#include <ranges>
#include <utility>

namespace smooth::utils {

////////////////////////////
// INTERVAL BINARY SEARCH //
////////////////////////////

/**
 * @brief Find interval in sorted range with binary search.
 *
 * 1. If r is empty, returns r.end()                  (not found)
 * 2. If t < r.front(), returns r.end()               (not found)
 * 3. If t >= r.back(), returns r.end() - 1           (no upper bound)
 * 4. Otherwise returns it s.t. *it <= t < *(it + 1)
 *
 * @param r sorted range to search in
 * @param t value to search for
 * @param wo comparison operation with signature \p std::weak_ordering(const
 * std::ranges::range_value_t<_R> &, const _T &)
 *
 * @return range iterator it according to the above rules
 */
constexpr auto
binary_interval_search(const std::ranges::range auto & r, const auto & t, auto && wo) noexcept
{
  using T  = std::decay_t<decltype(t)>;
  using Rv = std::ranges::range_value_t<std::decay_t<decltype(r)>>;

  auto left = std::ranges::cbegin(r);
  auto rght = std::ranges::cend(r);

  if (std::ranges::empty(r) || wo(*left, t) > 0) {
    return rght;
  } else if (wo(*(rght - 1), t) <= 0) {
    return rght - 1;
  }

  decltype(left) pivot;

  while (left + 1 < rght) {
    double alpha;
    if constexpr (std::is_convertible_v<Rv, double> && std::is_convertible_v<T, double>) {
      alpha = (static_cast<double>(t) - static_cast<double>(*left))
            / static_cast<double>(*(rght - 1) - *left);
    } else {
      alpha = 0.5;
    }

    pivot = left + static_cast<intptr_t>(static_cast<double>(rght - 1 - left) * alpha);

    if (wo(*(pivot + 1), t) <= 0) {
      left = pivot + 1;
    } else if (wo(*pivot, t) > 0) {
      rght = pivot + 1;
    } else {
      break;
    }
  }

  return pivot;
}

/**
 * @brief Find interval in sorted range with binary search using default comparison.
 */
constexpr auto binary_interval_search(const std::ranges::range auto & r, const auto & t) noexcept
{
  return binary_interval_search(r, t, [](const auto & _s, const auto & _t) { return _s <=> _t; });
}

/////////////////////
// STATIC FOR LOOP //
/////////////////////

/**
 * @brief Compile-time for loop equivalent to the statement (f(0), f(1), ..., f(_I-1))
 */
template<std::size_t _I>
constexpr auto
static_for(auto && f) noexcept(noexcept(std::invoke(f, std::integral_constant<std::size_t, 0>())))
{
  const auto f_caller = [&]<std::size_t... _Idx>(std::index_sequence<_Idx...>)
  {
    return (std::invoke(f, std::integral_constant<std::size_t, _Idx>()), ...);
  };

  return f_caller(std::make_index_sequence<_I>{});
}

/////////////////
// ARRAY UTILS //
/////////////////

/**
 * @brief Prefix-sum an array starting at zero
 */
template<typename _T, std::size_t _L>
constexpr std::array<_T, _L + 1> array_psum(const std::array<_T, _L> & x) noexcept
{
  std::array<_T, _L + 1> ret;
  ret[0] = _T(0);
  std::partial_sum(x.begin(), x.end(), ret.begin() + 1);
  return ret;
}

}  // namespace smooth::utils

#endif  // SMOOTH__INTERNAL__UTILS_HPP_
