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

#ifndef SMOOTH__MANIFOLD_HPP_
#define SMOOTH__MANIFOLD_HPP_

#include <Eigen/Core>
#include <concepts>

/**
 * @file manifold.hpp Manifold interface and free Manifold functions.
 */

namespace smooth {

namespace traits {

/**
 * @brief Trait class for making a class a Manifold instance via specialization.
 */
template<typename T>
struct man;

}  // namespace traits

// clang-format off

/**
 * @brief Class-external Manifold interface defined through the traits::man trait class.
 */
template<typename M>
concept Manifold =
requires {
  // Underlying scalar type
  typename traits::man<M>::Scalar;
  // Default representation
  typename traits::man<M>::PlainObject;
  // Compile-time degrees of freedom (tangent space dimension). Can be dynamic (equal to -1)
  {traits::man<M>::Dof}->std::convertible_to<Eigen::Index>;
  // A default-initialized Manifold object
  {traits::man<M>::Default()}->std::convertible_to<typename traits::man<M>::PlainObject>;
} &&
requires(const M & m1, const M & m2, const Eigen::Matrix<typename traits::man<M>::Scalar, traits::man<M>::Dof, 1> & a) {
  // Run-time degrees of freedom (tangent space dimension)
  {traits::man<M>::dof(m1)}->std::convertible_to<Eigen::Index>;
  // Right-plus: add a tangent vector to a Manifold element to obtain a new Manifold element
  {traits::man<M>::rplus(m1, a)}->std::convertible_to<typename traits::man<M>::PlainObject>;
  // Right-minus: subtract a Manifold element from another to obtain the difference tangent vector
  {traits::man<M>::rminus(m1, m2)}->std::convertible_to<Eigen::Matrix<typename traits::man<M>::Scalar, traits::man<M>::Dof, 1>>;
} && (
  !std::is_convertible_v<typename traits::man<M>::Scalar, double> ||
  requires (const M & m) {
    {traits::man<M>::template cast<double>(m)}->std::convertible_to<typename traits::man<M>::template CastT<double>>;
  }
) && (
  !std::is_convertible_v<typename traits::man<M>::Scalar, float> ||
  requires (const M & m) {
    {traits::man<M>::template cast<float>(m)}->std::convertible_to<typename traits::man<M>::template CastT<float>>;
  }
) &&
// PlainObject must be default-constructible
std::is_default_constructible_v<typename traits::man<M>::PlainObject> &&
std::is_copy_constructible_v<typename traits::man<M>::PlainObject> &&
// PlainObject must be assignable from M
std::is_assignable_v<typename traits::man<M>::PlainObject &, M>;

// clang-format on

namespace traits {

/**
 * @brief Concept to identify Eigen column vectors
 */
template<typename G>
concept RnType = std::is_base_of_v<Eigen::MatrixBase<G>, G> && G::ColsAtCompileTime == 1;

/**
 * @brief Manifold interface for RnType
 */
template<RnType M>
struct man<M>
{
  static constexpr int Dof = M::SizeAtCompileTime;

  using Scalar      = typename M::Scalar;
  using PlainObject = typename M::PlainObject;
  template<typename NewScalar>
  using CastT = Eigen::Matrix<NewScalar, M::SizeAtCompileTime, 1>;

  static inline PlainObject Default() { return M::Zero(); }

  static inline Eigen::Index dof(const M & m) { return m.size(); }

  template<typename NewScalar>
  static inline CastT<NewScalar> cast(const M & m)
  {
    return m.template cast<NewScalar>();
  }

  template<typename Derived>
  static inline PlainObject rplus(const M & g, const Eigen::MatrixBase<Derived> & a)
  {
    return g + a;
  }

  template<typename Derived>
  static inline Eigen::Matrix<Scalar, M::SizeAtCompileTime, 1> rminus(const M & m1, const Eigen::MatrixBase<Derived> & m2)
  {
    return m1 - m2;
  }
};

/**
 * @brief Trait class to mark external types as scalars.
 */
template<typename T>
struct scalar_trait
{
  static constexpr bool value = false;
};

/**
 * @brief Concept to identify built-in scalars
 */
template<typename T>
concept ScalarType = std::is_floating_point_v<T> || traits::scalar_trait<T>::value;

/**
 * @brief Manifold interface for ScalarType
 */
template<ScalarType M>
struct man<M>
{
  static constexpr int Dof = 1;

  using Scalar      = M;
  using PlainObject = M;
  template<typename NewScalar>
  using CastT = NewScalar;

  static inline PlainObject Default() { return M(0); }

  static inline Eigen::Index dof(const M &) { return 1; }

  template<typename NewScalar>
  static inline CastT<NewScalar> cast(const M & m)
  {
    return static_cast<NewScalar>(m);
  }

  template<typename Derived>
  static inline PlainObject rplus(const M & g, const Eigen::MatrixBase<Derived> & a)
  {
    return g + a.x();
  }

  static inline Eigen::Matrix<Scalar, Dof, 1> rminus(const M & m1, const M & m2)
  {
    return Eigen::Matrix<Scalar, Dof, 1>(m1 - m2);
  }
};

}  // namespace traits

////////////////////////////////////////////////
//// Free functions that dispatch to traits::man<M> ////
////////////////////////////////////////////////

// Static constants

/**
 * @brief Manifold degrees of freedom (tangent space dimension)
 *
 * @note Equal to -1 for a dynamically sized Manifold
 */
template<Manifold M>
static inline constexpr Eigen::Index Dof = traits::man<M>::Dof;

// Types

/**
 * @brief Manifold scalar type
 */
template<Manifold M>
using Scalar = typename traits::man<M>::Scalar;

/**
 * @brief Manifold default type
 */
template<Manifold M>
using PlainObject = typename traits::man<M>::PlainObject;

/**
 * @brief Cast'ed type
 */
template<typename NewScalar, Manifold M>
using CastT = typename traits::man<M>::template CastT<NewScalar>;

/**
 * @brief Tangent as a Dof-lenth Eigen vector
 */
template<Manifold M>
using Tangent = Eigen::Matrix<typename traits::man<M>::Scalar, traits::man<M>::Dof, 1>;

// Functions

/**
 * @brief Default-initialized Manifold
 */
template<Manifold M>
inline typename traits::man<M>::PlainObject Default()
{
  return traits::man<M>::Default();
}

/**
 * @brief Manifold degrees of freedom (tangent space dimension)
 */
template<Manifold M>
inline Eigen::Index dof(const M & m)
{
  return traits::man<M>::dof(m);
}

/**
 * @brief Cast to different scalar type
 */
template<typename NewScalar, Manifold M>
inline CastT<NewScalar, M> cast(const M & m)
{
  return traits::man<M>::template cast<NewScalar>(m);
}

/**
 * @brief Manifold right-plus
 */
template<Manifold M, typename Derived>
inline PlainObject<M> rplus(const M & m, const Eigen::MatrixBase<Derived> & a)
{
  return traits::man<M>::rplus(m, a);
}

/**
 * @brief Manifold right-minus
 */
template<Manifold M, Manifold Mo>
inline Tangent<M> rminus(const M & g1, const Mo & g2)
{
  return traits::man<M>::rminus(g1, g2);
}

}  // namespace smooth

#endif
