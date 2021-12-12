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
binary_interval_search(std::ranges::random_access_range auto && r, auto && t, auto && wo) noexcept
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
constexpr auto
binary_interval_search(std::ranges::random_access_range auto && r, auto && t) noexcept
{
  return binary_interval_search(
    std::forward<decltype(r)>(r),
    std::forward<decltype(t)>(t),
    [](const auto & _s, const auto & _t) { return _s <=> _t; });
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
  std::partial_sum(x.begin(), x.end(), std::next(ret.begin(), 1));
  return ret;
}

/////////////////
// RANGE UTILS //
/////////////////

namespace views {

namespace sr = std::ranges;

/// @brief Apply function to pairwise elements
template<sr::input_range R, std::copy_constructible F>
  requires sr::view<R>
class pairwise_transform_view : public sr::view_interface<pairwise_transform_view<R, F>>
{
public:
  class _Iterator
  {
  private:
    const pairwise_transform_view * parent_;
    sr::iterator_t<const R> it1_, it2_;

  public:
    using value_type = std::remove_cvref_t<
      std::invoke_result_t<F &, sr::range_reference_t<R>, sr::range_reference_t<R>>>;
    using difference_type = sr::range_difference_t<R>;

    _Iterator() = default;
    constexpr _Iterator(const pairwise_transform_view * parent, const R & r)
        : parent_(parent), it1_(std::ranges::begin(r)), it2_(std::ranges::begin(r))
    {
      if (it2_ != std::ranges::end(r)) { ++it2_; }
    }

    constexpr decltype(auto) operator*() const { return std::invoke(parent_->f_, *it1_, *it2_); }

    constexpr _Iterator & operator++()
    {
      ++it1_, ++it2_;
      return *this;
    }

    constexpr void operator++(int) { ++it1_, ++it2_; }

    constexpr _Iterator operator++(int) requires sr::forward_range<R>
    {
      _Iterator tmp = *this;
      ++this;
      return tmp;
    }

    constexpr _Iterator & operator--() requires sr::bidirectional_range<R>
    {
      --it1_, --it2_;
      return *this;
    }

    constexpr _Iterator operator--(int) requires sr::bidirectional_range<R>
    {
      auto tmp = *this;
      --this;
      return tmp;
    }

    friend constexpr bool operator==(const _Iterator & x, const _Iterator & y)
    {
      return x.it1_ == y.it1_;
    }

    friend constexpr bool operator==(const _Iterator & x, const sr::sentinel_t<const R> & y)
    {
      return x.it2_ == y;
    }
  };

private:
  R base_{};
  F f_{};

public:
  template<typename Fp>
  constexpr pairwise_transform_view(R base, Fp && f) : base_(base), f_(std::forward<Fp>(f))
  {}

  constexpr R base() const & { return base_; }

  constexpr R base() && { return std::move(base_); }

  constexpr _Iterator begin() const { return _Iterator(this, base_); }

  constexpr sr::sentinel_t<const R> end() const { return sr::end(base_); }

  constexpr auto size() const requires sr::sized_range<const R>
  {
    const auto s = sr::size(base_);
    return (s >= 2) ? s - 1 : 0;
  }
};

template<typename R, typename F>
pairwise_transform_view(R &&, F) -> pairwise_transform_view<std::views::all_t<R>, F>;

/// @brief Apply function to pairwise elements
struct _PairwiseTransform : sr::views::__adaptor::_RangeAdaptor<_PairwiseTransform>
{
  template<sr::viewable_range R, typename F>
  constexpr auto operator()(R && r, F && f) const
  {
    return pairwise_transform_view(std::forward<R>(r), std::forward<F>(f));
  }

  using _RangeAdaptor<_PairwiseTransform>::operator();
  static constexpr int _S_arity = 2;
};

/// @brief Apply function to pairwise elements
inline constexpr _PairwiseTransform pairwise_transform;

}  // namespace views

}  // namespace smooth::utils

#endif  // SMOOTH__INTERNAL__UTILS_HPP_
