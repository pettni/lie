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

#ifndef SMOOTH__SPLINE__CUMULATIVE_SPLINE_HPP_
#define SMOOTH__SPLINE__CUMULATIVE_SPLINE_HPP_

/**
 * @file
 * @brief Evaluation of cumulative Lie group splines.
 */

#include "smooth/lie_group.hpp"
#include "smooth/polynomial/basis.hpp"

#include <cassert>
#include <optional>

namespace smooth {

namespace detail {

/// @brief Optional argument for spline time derivatives
template<LieGroup G>
using OptTangent = std::optional<Eigen::Ref<Tangent<G>>>;

/// @brief Optional argument for spline control point derivatives
template<LieGroup G, std::size_t K>
using OptJacobian =
  std::optional<Eigen::Ref<Eigen::Matrix<Scalar<G>, Dof<G>, Dof<G> == -1 ? -1 : Dof<G> *(K + 1)>>>;

}  // namespace detail

/**
 * @brief Evaluate a cumulative spline of order \f$ K \f$ defined as
 * \f[
 *   g = \prod_{i=1}^{K} \exp ( \tilde B_i(u) * v_i )
 * \f]
 * where \f$ \tilde B_i \f$ are cumulative basis functins and \f$ v_i = g_i - g_{i-1} \f$.
 *
 * @tparam K spline order (number of basis functions)
 * @tparam G lie group type
 * @param[in] diff_points range of differences v_i (must be of size K)
 * @param[in] Bcum matrix of cumulative base coefficients (size K+1 x K+1)
 * @param[in] u time point to evaluate spline at (clamped to [0, 1])
 * @param[out] vel calculate first order derivative w.r.t. u
 * @param[out] acc calculate second order derivative w.r.t. u
 * @param[out] der derivatives of g w.r.t. the K+1 control points g_0, g_1, ... g_K
 */
template<std::size_t K, LieGroup G, typename Derived>
inline G cspline_eval_diff(
  std::ranges::sized_range auto && diff_points,
  const Eigen::MatrixBase<Derived> & Bcum,
  Scalar<G> u,
  detail::OptTangent<G> vel     = {},
  detail::OptTangent<G> acc     = {},
  detail::OptJacobian<G, K> der = {}) noexcept
{
  assert(std::ranges::size(diff_points) == K);
  assert(Bcum.cols() == K + 1);
  assert(Bcum.rows() == K + 1);

  const auto U = monomial_derivatives<K, 2, Scalar<G>>(u);

  Eigen::Map<const Eigen::Vector<Scalar<G>, K + 1>> uvec(U[0].data());
  Eigen::Map<const Eigen::Vector<Scalar<G>, K + 1>> duvec(U[1].data());
  Eigen::Map<const Eigen::Vector<Scalar<G>, K + 1>> d2uvec(U[2].data());

  if (vel.has_value()) { vel.value().setZero(); }
  if (acc.has_value()) { acc.value().setZero(); }

  const Eigen::Index xdof = dof(*std::ranges::cbegin(diff_points));

  G g = Identity<G>(xdof);

  for (const auto & [j, v] : utils::zip(std::views::iota(1u), diff_points)) {
    const Scalar<G> Btilde = uvec.dot(Bcum.col(j));

    const G exp_Bt_v = ::smooth::exp<G>(Btilde * v);

    g = composition(g, exp_Bt_v);

    if (vel.has_value() || acc.has_value()) {
      const Scalar<G> dBtilde = duvec.dot(Bcum.col(j));
      const auto Adj          = Ad(inverse(exp_Bt_v));

      if (vel.has_value()) {
        vel.value().applyOnTheLeft(Adj);
        vel.value().noalias() += dBtilde * v;
      }

      if (acc.has_value()) {
        const Scalar<G> d2Btilde = d2uvec.dot(Bcum.col(j));
        acc.value().applyOnTheLeft(Adj);
        acc.value().noalias() += dBtilde * ad<G>(vel.value()) * v;
        acc.value().noalias() += d2Btilde * v;
      }
    }
  }

  if (der.has_value()) {
    der.value().setZero();

    G z2inv = Identity<G>(xdof);

    for (const auto j : std::views::iota(0u, K + 1) | std::views::reverse) {
      // j : K -> 0 (inclusive)

      if (j != K) {
        const Scalar<G> Btilde_jp = uvec.dot(Bcum.col(j + 1));
        const Tangent<G> & vjp    = *std::ranges::next(std::ranges::cbegin(diff_points), j);
        const Tangent<G> sjp      = Btilde_jp * vjp;

        der.value().template block<Dof<G>, Dof<G>>(0, j * Dof<G>).noalias() -=
          Btilde_jp * Ad(z2inv) * dr_exp<G>(sjp) * dl_expinv<G>(vjp);

        z2inv = composition(z2inv, ::smooth::exp<G>(-sjp));
      }

      const Scalar<G> Btilde_j = uvec.dot(Bcum.col(j));

      if (j != 0u) {
        const Tangent<G> & vj = *std::ranges::next(std::ranges::cbegin(diff_points), j - 1);
        der.value().template block<Dof<G>, Dof<G>>(0, j * Dof<G>).noalias() +=
          Btilde_j * Ad(z2inv) * dr_exp<G>(Btilde_j * vj) * dr_expinv<G>(vj);
      } else {
        der.value().template block<Dof<G>, Dof<G>>(0, j * Dof<G>).noalias() += Btilde_j * Ad(z2inv);
      }
    }
  }

  return g;
}

/**
 * @brief Evaluate a cumulative basis spline of order K and calculate derivatives
 * \f[
 *   g = g_0 * \prod_{i=1}^{K} \exp ( \tilde B_i(u) * v_i ),
 * \f]
 * where \f$ \tilde B \f$ are cumulative basis functions and \f$ v_i = g_i - g_{i-1} \f$.
 *
 * @tparam K spline order
 * @param[in] gs LieGroup control points \f$ g_0, g_1, \ldots, g_K \f$ (must be of size K +
 * 1)
 * @param[in] Bcum matrix of cumulative base coefficients (size K+1 x K+1)
 * @param[in] u time point to evaluate spline at (clamped to [0, 1])
 * @param[out] vel calculate first order derivative w.r.t. u
 * @param[out] acc calculate second order derivative w.r.t. u
 * @param[out] der derivatives w.r.t. the K+1 control points
 */
template<
  std::size_t K,
  std::ranges::range R,
  typename Derived,
  LieGroup G = std::ranges::range_value_t<R>>
inline G cspline_eval(
  R && gs,
  const Eigen::MatrixBase<Derived> & Bcum,
  Scalar<G> u,
  detail::OptTangent<G> vel     = {},
  detail::OptTangent<G> acc     = {},
  detail::OptJacobian<G, K> der = {}) noexcept
{
  assert(std::ranges::size(gs) == K + 1);

  static constexpr auto sub  = [](const auto & x1, const auto & x2) { return rminus(x2, x1); };
  const auto diff_pts = gs | utils::views::pairwise_transform(sub);

  return composition(
    *std::ranges::begin(gs), cspline_eval_diff<K, G>(diff_pts, Bcum, u, vel, acc, der));
}

}  // namespace smooth

#endif  // SMOOTH__SPLINE__CUMULATIVE_SPLINE_HPP_