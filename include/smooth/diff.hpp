// Copyright (C) 2021-2022 Petter Nilsson. MIT License.

#pragma once

/**
 * @file
 * @brief Differentiation on Manifolds.
 */

#include <Eigen/Core>
#include <type_traits>

#include "detail/utils.hpp"
#include "manifold.hpp"
#include "wrt.hpp"

namespace smooth::diff {

/**
 * @brief Available differentiation methods
 */
enum class Type {
  Numerical,  ///< Numerical (forward) derivatives
  Autodiff,   ///< Uses the autodiff (https://autodiff.github.io) library; requires  \p
              ///< compat/autodiff.hpp
  Ceres,      ///< Uses the Ceres (http://ceres-solver.org) built-in autodiff; requires \p
              ///< compat/ceres.hpp
  Analytic,   ///< Hand-coded derivative, @see diffable_order1, diffable_order2
  Default     ///< Select based on availability (Analytic > Autodiff > Ceres > Numerical)
};

static constexpr Type DefaultType =
#ifdef SMOOTH_DIFF_AUTODIFF
  Type::Autodiff;
#elif defined SMOOTH_DIFF_CERES
  Type::Ceres;
#else
  Type::Numerical;
#endif

/**
 * @brief Differentiation in tangent space
 *
 * @tparam K differentiation order (0, 1 or 2)
 * @tparam D differentiation method to use
 *
 * First derivatives are returned as a matrix df s.t.
 * df(i, j) = dfi / dxj, where fi is the i:th degree of freedom of f, and xj the j:th degree of
 * freedom of x.
 *
 * Second derivatives are stored as a horizontally stacked block matrix
 * d2f = [ d2f0 d2f1 ... d2fN ],
 * where d2fi(j, k) = d2fi / dxjxk is the Hessian of the i:th degree of freedom of f.
 *
 * @param f function to differentiate
 * @param x reference tuple of function arguments
 * @return {f(x)} for K = 0, {f(x), dr f(x)} for K = 1, {f(x), dr f(x), d2r f(x)} for K = 2
 *
 * @note Only scalar functions suppored for K = 2
 *
 * @note All arguments in x as well as the return type \f$f(x)\f$ must satisfy
 * the Manifold concept.
 */
template<std::size_t K, Type D>
auto dr(auto && f, auto && x);

/**
 * @brief Differentiation in tangent space using default method
 *
 * @tparam K differentiation order
 *
 * @param f function to differentiate
 * @param x reference tuple of function arguments
 * @return \p std::pair containing value and right derivative: \f$(f(x), \mathrm{d}^r f_x)\f$
 *
 * @note All arguments in x as well as the return type \f$f(x)\f$ must satisfy
 * the Manifold concept.
 */
template<std::size_t K = 1>
auto dr(auto && f, auto && x);

}  // namespace smooth::diff

#include "detail/diff_impl.hpp"
