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

#ifndef SMOOTH__INTERP__CURVE__HPP_
#define SMOOTH__INTERP__CURVE__HPP_

/**
 * @file
 * @brief bezier splines on lie groups.
 */

#include <algorithm>
#include <iostream>
#include <ranges>

#include "smooth/concepts.hpp"
#include "smooth/internal/utils.hpp"

#include "common.hpp"

namespace smooth {

/**
 * @brief Curve type
 *
 * A curve is a continuous function \f$ x : \mathbb{R} \rightarrow \mathbb{G} \f$ such that \f$ x(0)
 * = e \f$.
 */
template<LieGroup G>
class Curve
{
public:
  Curve() : end_t_{}, end_g_{}, vs_{}, seg_T0_{}, seg_Del_{} {}

  /**
   * @brief Create Curve with one segment and given velocities
   */
  Curve(double T, std::array<typename G::Tangent, 3> && vs)
      : end_t_{T}, vs_(std::move(vs)), seg_T0_{0}, seg_Del_{1}
  {
    end_g_.resize(1);
    end_g_[0] = eval(T);
  }

  /**
   * @brief Create Curve with one segment and given velocities
   */
  template<std::ranges::range Rv>
  Curve(double T, const Rv & vs) : end_t_{T}, seg_T0_{0}, seg_Del_{1}
  {
    if (std::ranges::size(vs) != 3) { throw std::runtime_error("Wrong number of control points"); }

    vs_.resize(1);
    std::copy(std::ranges::begin(vs), std::ranges::end(vs), vs_[0].begin());

    end_g_.resize(1);
    end_g_[0] = eval(T);
  }

  /// @brief Copy constructor
  Curve(const Curve &) = default;

  /// @brief Move constructor
  Curve(Curve &&) = default;

  /// @brief Copy assignment
  Curve & operator=(const Curve &) = default;

  /// @brief Move assignment
  Curve & operator=(Curve &&) = default;

  /// @brief Destructor
  ~Curve() = default;

  /**
   * @brief Create Curve that starts at identity and with constant body velocity.
   *
   * The resulting curve is
   * \f[
   *   x(t) = \exp(t v), \quad t \in [0, T].
   * \f]
   *
   * @param v body velocity
   * @param T curve end point
   */
  static Curve ConstantVelocity(const typename G::Tangent & v, double T = 1)
  {
    std::array<typename G::Tangent, 3> vs;
    vs.fill(T * v / 3);
    return Curve(T, vs);
  }

  /**
   * @brief Create Curve that starts at identity and with a given initial velocity, end
   * position, and end velocity.
   *
   * @param v body velocity
   * @param T curve end point
   */
  static Curve FixedCubic(
    const G & gb, const typename G::Tangent & va, const typename G::Tangent & vb, double T = 1)
  {
    std::array<typename G::Tangent, 3> vs;
    vs[0] = T * va / 3;
    vs[2] = T * vb / 3;
    vs[1] = (G::exp(-vs[0]) * gb * G::exp(-vs[2])).log();
    return Curve(T, vs);
  }

  /// @brief Number of Curve segments.
  std::size_t size() const { return end_t_.size(); }

  /// @brief Number of Curve segments.
  bool empty() const { return size() == 0; }

  /// @brief Start time of curve (always equal to zero).
  double t_min() const { return 0; }

  /// @brief End time of curve.
  double t_max() const
  {
    if (empty()) { return 0; }
    return end_t_.back();
  }

  /// @brief Curve start (always equal to identity).
  G start() const { return G::Identity(); }

  /// @brief Curve end .
  G end() const
  {
    if (empty()) { return G::Identity(); }
    return end_g_.back();
  }

  /// @brief Concatenate two curves
  Curve & operator*=(const Curve & other)
  {
    std::size_t N1 = size();
    std::size_t N2 = other.size();

    const double tend = t_max();
    const G gend      = end();

    end_t_.resize(N1 + N2);
    end_g_.resize(N1 + N2);
    vs_.resize(N1 + N2);
    seg_T0_.resize(N1 + N2);
    seg_Del_.resize(N1 + N2);

    for (auto i = 0u; i < N2; ++i) {
      end_t_[N1 + i]   = tend + other.end_t_[i];
      end_g_[N1 + i]   = gend * other.end_g_[i];
      vs_[N1 + i]      = other.vs_[i];
      seg_T0_[N1 + i]  = other.seg_T0_[i];
      seg_Del_[N1 + i] = other.seg_Del_[i];
    }

    return *this;
  }

  Curve operator*(const Curve & o)
  {
    Curve ret = *this;
    ret *= o;
    return ret;
  }

  G eval(double t, detail::OptTangent<G> vel = {}, detail::OptTangent<G> acc = {}) const
  {
    const auto istar = find_idx(t);

    double ta = istar == 0 ? 0 : end_t_[istar - 1];
    double T  = end_t_[istar] - ta;

    const double Del = seg_Del_[istar];
    const double u   = std::clamp<double>(seg_T0_[istar] + Del * (t - ta) / T, 0, 1);

    constexpr auto Mstatic = detail::cum_coefmat<CSplineType::BEZIER, double, 3>().transpose();
    Eigen::Map<const Eigen::Matrix<double, 3 + 1, 3 + 1, Eigen::RowMajor>> M(Mstatic[0].data());

    G g0 = istar == 0 ? G::Identity() : end_g_[istar - 1];

    if (seg_T0_[istar] > 0) {
      // compensate for cropped intervals
      g0 *= cspline_eval_diff<3, G>(vs_[istar], M, seg_T0_[istar]).inverse();
    }

    G g = g0 * cspline_eval_diff<3, G>(vs_[istar], M, u, vel, acc);

    if (vel.has_value()) { vel.value() *= Del / T; }
    if (acc.has_value()) { acc.value() *= Del * Del / (T * T); }

    return g;
  }

  Curve crop(double ta, double tb = std::numeric_limits<double>::infinity()) const
  {
    if (tb < ta) { throw std::runtime_error("Curve: crop interval must be positive"); }

    ta = std::max<double>(ta, 0);
    tb = std::min<double>(tb, t_max());

    const std::size_t i0 = find_idx(ta);
    std::size_t Nseg     = find_idx(tb) + 1 - i0;

    // prevent last segment from being empty
    if (end_t_[i0 + Nseg] == tb) { --Nseg; }

    // state at new from beginning of curve
    const G ga = eval(ta), gb = eval(tb);

    std::vector<double> end_t(Nseg);
    std::vector<G> end_g(Nseg);
    std::vector<std::array<typename G::Tangent, 3>> vs(Nseg);
    std::vector<double> seg_T0(Nseg), seg_Del(Nseg);

    // copy over all relevant segments
    for (auto i = 0u; i != Nseg; ++i) {
      if (i == Nseg - 1) {
        end_t[i] = tb - ta;
        end_g[i] = ga.inverse() * gb;
      } else {
        end_t[i] = end_t_[i0 + i] - ta;
        end_g[i] = ga.inverse() * end_g_[i0 + i];
      }
      vs[i]      = vs_[i0 + i];
      seg_T0[i]  = seg_T0_[i0 + i];
      seg_Del[i] = seg_Del_[i0 + i];
    }

    // crop first segment
    {
      const double tta = 0;
      const double ttb = end_t_[i0];
      const double sa  = ta;
      const double sb  = ttb;

      seg_T0[0] += seg_Del[0] * (sa - tta) / (ttb - tta);
      seg_Del[0] *= (sb - sa) / (ttb - tta);
    }

    // crop last segment
    {
      const double tta = Nseg == 1 ? ta : end_t_[Nseg - 2];
      const double ttb = end_t_[Nseg - 1];
      const double sa  = tta;
      const double sb  = tb;

      seg_T0[Nseg - 1] += seg_Del[Nseg - 1] * (sa - tta) / (ttb - tta);
      seg_Del[Nseg - 1] *= (sb - sa) / (ttb - tta);
    }

    // create new curve with appropriate body velocities
    Curve<G> ret;
    ret.end_t_   = end_t;
    ret.end_g_   = end_g;
    ret.vs_      = vs;
    ret.seg_T0_  = seg_T0;
    ret.seg_Del_ = seg_Del;

    return ret;
  }

private:
  std::size_t find_idx(double t) const
  {
    // TODO binary search
    std::size_t istar = 0;
    while (istar + 1 < size() && end_t_[istar] <= t) { ++istar; }
    return istar;
  }

  // segment i is defined by
  //
  //  - time interval:  end_t_[i-1], end_t_[i]
  //  - g interval:     end[i-1], end_g_[i]
  //  - velocities:     vs_[i]
  //  - crop:           seg_T0_[i], seg_Del_[i]

  // segment end times
  std::vector<double> end_t_;

  // segment end points
  std::vector<G> end_g_;

  // segment bezier velocities
  std::vector<std::array<typename G::Tangent, 3>> vs_;

  // segment crop information
  std::vector<double> seg_T0_, seg_Del_;
};

}  // namespace smooth

#endif  // SMOOTH__INTERP__CURVE__HPP_