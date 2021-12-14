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

#ifndef SMOOTH__SPLINE__REPARAMETERIZE_HPP_
#define SMOOTH__SPLINE__REPARAMETERIZE_HPP_

/**
 * @file
 * @brief Reparamterize a Spline to satisfy derivative constraints.
 */

#include "spline.hpp"
#include "traits.hpp"

namespace smooth {

/**
 * @brief Reparameterize a spline to satisfy velocity and acceleration constraints.
 *
 * If \f$ x(\cdot) \f$ is a Spline, then this function generates a function \f$ s(t) \f$ s.t. the
 * reparamterized spline \f$ x(s(t)) \f$ has body velocity bounded between vel_min and vel_max, and
 * body acceleration bounded between acc_min and acc_max.
 *
 * @param spline spline \f$ x(t) \f$ to reparameterize.
 * @param vel_min, vel_max velocity bounds, must be s.t. vel_min < 0 < vel_max (component-wise).
 * @param acc_min, acc_max acceleration bounds, must be s.t. acc_min < 0 < acc_max (component-wise).
 * @param start_vel target value for \f$ s'(0) \f$ (must be non-negative).
 * @param end_vel target value for \f$ s'(t_{max}) \f$ (must be non-negative).
 * @param N partition size. A larger value implies smaller bound violations.
 *
 * @note Allocates heap memory.
 *
 * @note For best results the input spline should be twice continously differentiable.
 *
 * @note It may not be feasible to satisfy the target boundary velocities. In those cases the
 * resulting velocities will be lower than the desired values.
 */
Spline<2, double> reparameterize_spline(
  const SplineType auto & spline,
  const auto & vel_min,
  const auto & vel_max,
  const auto & acc_min,
  const auto & acc_max,
  const double start_vel = 1,
  const double end_vel   = std::numeric_limits<double>::infinity(),
  const std::size_t N    = 100)
{
  using G = std::invoke_result_t<decltype(spline), double>;

  static_assert(Dof<G> > 0, "reparameterize_spline only supports static-sized types");

  assert(vel_min.size() == Dof<G>);
  assert(vel_max.size() == Dof<G>);
  assert(acc_min.size() == Dof<G>);
  assert(acc_max.size() == Dof<G>);

  assert(vel_min.maxCoeff() < 0);
  assert(vel_max.minCoeff() > 0);
  assert(acc_min.maxCoeff() < 0);
  assert(acc_max.minCoeff() > 0);

  constexpr auto sq  = [](auto x) { return x * x; };
  constexpr auto eps = 1e-8;
  constexpr auto inf = std::numeric_limits<double>::infinity();

  const double s0 = spline.t_min();
  const double sf = spline.t_max();

  const double ds = (sf - s0) / N;

  // REVERSE PASS WITH MINIMAL ACCELERATION

  Eigen::VectorXd v2max(N + 1);
  v2max(N) = end_vel * end_vel;

  for (const auto i : std::views::iota(0u, N + 1) | std::views::reverse) {
    // i : N -> 0
    Tangent<G> vel, acc;
    spline(s0 + ds * i, vel, acc);

    if (i + 1 < N + 1) {
      // APPROACH 1 (exact): Solve linear program in y = v^2
      //
      //  max   y
      //  s.t.                   y - 2 ds a         \leq y(i + 1)    [1]
      //        vel_min.^2  \leq vel.^2 y           \leq vel_max.^2    [2]
      //        acc_min     \leq acc * y + vel * a  \leq acc_max       [3]
      //
      // and use acceleration a at i
      //
      // TODO: implement, use lp2d, and evaluate

      // APPROACH 2 (approximate): calculate constraints from i + 1 and apply them at i

      // figure minimal allowed acceleration at i + 1

      const double a = [&]() {
        const Tangent<G> upper = (acc_max - acc * v2max(i + 1)).cwiseMax(Tangent<G>::Zero());
        const Tangent<G> lower = (acc_min - acc * v2max(i + 1)).cwiseMin(Tangent<G>::Zero());
        double ret             = -inf;
        for (auto j = 0u; j != Dof<G>; ++j) {
          if (vel(j) > eps) {
            ret = std::max<double>(ret, lower(j) / vel(j));
          } else if (vel(j) < -eps) {
            ret = std::max<double>(ret, upper(j) / vel(j));
          }
        }
        return ret;
      }();

      // maximal allowed velocity at i
      v2max(i) = v2max(i + 1) - 2 * ds * a;
    }

    // clamp velocity to not exceed constraints
    for (const auto j : std::views::iota(0, Dof<G>)) {
      if (vel(j) > eps) {
        v2max(i) = std::min<double>(v2max(i), sq(vel_max(j) / vel(j)));
      } else if (vel(j) < -eps) {
        v2max(i) = std::min<double>(v2max(i), sq(vel_min(j) / vel(j)));
      }

      // this ensures that a = 0 is feasible
      if (acc(j) > eps) {
        v2max(i) = std::min<double>(v2max(i), acc_max(j) / acc(j));
      } else if (acc(j) < -eps) {
        v2max(i) = std::min<double>(v2max(i), acc_min(j) / acc(j));
      }
    }
  }

  // FORWARD PASS WITH MAXIMAL ACCELERATION

  Spline<2, double> ret;
  ret.reserve(N + 1);

  // "current" squared velocity
  double v2m = std::min(start_vel * start_vel, v2max(0));

  for (const auto i : std::views::iota(0u, N)) {
    const double si = s0 + ds * i;

    Tangent<G> vel, acc;
    spline(si, vel, acc);

    // velocity at si
    const double vi2 = v2m;
    const double vi  = std::sqrt(vi2);

    // figure maximal allowed acceleration at (si, vi)
    const double ai = [&]() {
      double ret             = inf;
      const Tangent<G> upper = (acc_max - acc * vi2).cwiseMax(Tangent<G>::Zero());
      const Tangent<G> lower = (acc_min - acc * vi2).cwiseMin(Tangent<G>::Zero());
      for (const auto j : std::views::iota(0, Dof<G>)) {
        if (vel(j) > eps) {
          ret = std::min<double>(ret, upper(j) / vel(j));
        } else if (vel(j) < -eps) {
          ret = std::min<double>(ret, lower(j) / vel(j));
        }
      }
      // do not exceed velocity at next step
      ret = std::min<double>(ret, (v2max(i + 1) - vi2) / (2 * ds));
      return ret;
    }();

    if (ai != inf) {
      const double dt = std::abs(ai) < eps
                        ? ds / vi
                        : (-vi + std::sqrt(std::max<double>(eps, vi2 + 2 * ds * ai))) / ai;

      // add segment to spline
      ret.concat_global(Spline<2, double>{
        dt,
        Eigen::Vector2d{dt * vi / 2, dt * (dt * ai + vi) / 2},
        si,
      });

      // update squared velocity with value at end of new segment
      v2m = ai == inf ? vi2 : std::max<double>(eps, vi2 + 2 * ai * ds);
    }
  }

  // reparameterization attains t_max
  ret.concat_global(Spline<2, double>(spline.t_max()));

  return ret;
}

}  // namespace smooth

#endif  // SMOOTH__SPLINE__REPARAMETERIZE_HPP_
