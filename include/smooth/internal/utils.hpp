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

#include <Eigen/Core>

#include "smooth/manifold.hpp"

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
 * std::ranges::range_value_t<R> &, const T &)
 *
 * @return range iterator it according to the above rules
 */
template<std::ranges::range R, typename T, typename WO>
auto binary_interval_search(const R & r, const T & t, WO && wo) noexcept
{
  auto left = std::ranges::begin(r);
  auto rght = std::ranges::end(r);

  if (std::ranges::empty(r) || wo(*left, t) > 0) {
    return rght;
  } else if (wo(*(rght - 1), t) <= 0) {
    return rght - 1;
  }

  decltype(left) pivot;

  while (left + 1 < rght) {
    double alpha;
    if constexpr (std::is_convertible_v<std::ranges::range_value_t<R>,
                    double> && std::is_convertible_v<T, double>) {
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
template<std::ranges::range R, typename T, typename S = std::ranges::range_value_t<R>>
auto binary_interval_search(const R & r, const T & t) noexcept
{
  return binary_interval_search(r, t, [](const S & _s, const T & _t) { return _s <=> _t; });
}

/////////////////////
// STATIC FOR LOOP //
/////////////////////

/**
 * @brief Compile-time for loop implementation
 */
template<typename _F, std::size_t... _Idx>
inline static constexpr auto static_for_impl(_F && f, std::index_sequence<_Idx...>)
{
  return (std::invoke(f, std::integral_constant<std::size_t, _Idx>()), ...);
}

/**
 * @brief Compile-time for loop over 0, ..., _I-1
 */
template<std::size_t _I, typename _F>
inline static constexpr auto static_for(_F && f)
{
  return static_for_impl(std::forward<_F>(f), std::make_index_sequence<_I>{});
}

/////////////////
// ARRAY UTILS //
/////////////////

/**
 * @brief Prefix-sum an array starting at zero
 */
template<typename T, std::size_t L>
constexpr std::array<T, L + 1> array_psum(const std::array<T, L> & x)
{
  std::array<T, L + 1> ret;
  ret[0] = T(0);
  std::partial_sum(x.begin(), x.end(), ret.begin() + 1);
  return ret;
}

/////////////////////////////////
// COMPILE-TIME MATRIX ALGEBRA //
/////////////////////////////////

/**
 * @brief Elementary structure for compile-time matrix algebra.
 *
 * Matrix is stored in memory in row-major order.
 */
template<typename _Scalar, std::size_t _Rows, std::size_t _Cols>
struct StaticMatrix : public std::array<std::array<_Scalar, _Cols>, _Rows>
{
  std::size_t Rows = _Rows;
  std::size_t Cols = _Cols;

  using std::array<std::array<_Scalar, _Cols>, _Rows>::operator[];

  /**
   * @brief Construct a matrix filled with zeros
   */
  constexpr StaticMatrix() : std::array<std::array<_Scalar, _Cols>, _Rows>{}
  {
    for (auto i = 0u; i != _Rows; ++i) { operator[](i).fill(_Scalar(0)); }
  }

  /**
   * @brief Add two matrices
   */
  constexpr StaticMatrix<_Scalar, _Rows, _Cols> operator+(
    StaticMatrix<_Scalar, _Rows, _Cols> o) const
  {
    StaticMatrix<_Scalar, _Rows, _Cols> ret;
    for (auto i = 0u; i < _Rows; ++i) {
      for (auto j = 0u; j < _Cols; ++j) { ret[i][j] = operator[](i)[j] + o[i][j]; }
    }
    return ret;
  }

  /**
   * @brief Return transpose of a matrix
   */
  constexpr StaticMatrix<_Scalar, _Rows, _Cols> transpose() const
  {
    StaticMatrix<_Scalar, _Rows, _Cols> ret;
    for (auto i = 0u; i < _Rows; ++i) {
      for (auto j = 0u; j < _Cols; ++j) { ret[j][i] = operator[](i)[j]; }
    }
    return ret;
  }

  /**
   * @brief Multiply two matrices
   */
  template<std::size_t _ColsNew>
  constexpr StaticMatrix<_Scalar, _Rows, _ColsNew> operator*(
    StaticMatrix<_Scalar, _Cols, _ColsNew> o) const
  {
    StaticMatrix<_Scalar, _Rows, _ColsNew> ret;
    for (auto i = 0u; i < _Rows; ++i) {
      for (auto j = 0u; j < _ColsNew; ++j) {
        for (auto k = 0u; k < _Cols; ++k) { ret[i][j] += operator[](i)[k] * o[k][j]; }
      }
    }
    return ret;
  }
};

}  // namespace smooth::utils

#endif  // SMOOTH__INTERNAL__UTILS_HPP_
