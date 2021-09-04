#ifndef SMOOTH__LIE_GROUP_HPP_
#define SMOOTH__LIE_GROUP_HPP_

#include <Eigen/Core>
#include <concepts>

/**
 * @file lie_group.hpp Internal and external Lie group interfaces
 */

namespace smooth {

/**
 * @brief Trait class for making a class an LieGroup instance
 */
template<typename T>
struct lie;

// clang-format off

/**
 * @brief Concept defining class with an external Lie group interface defined through the lie trait class.
 */
template<typename G>
concept LieGroup =
requires {
  // Underlying scalar type
  typename lie<G>::Scalar;
  // Default representation
  typename lie<G>::PlainObject;
  // Compile-time degrees of freedom (tangent space dimension). Can be dynamic (equal to -1)
  {lie<G>::Dof}->std::convertible_to<Eigen::Index>;
} &&
// GROUP INTERFACE
requires(const G & g1, const G & g2, typename lie<G>::Scalar eps) {
  // Return the identity element
  {lie<G>::Identity()}->std::convertible_to<typename lie<G>::PlainObject>;
  // Return a random element
  {lie<G>::Random()}->std::convertible_to<typename lie<G>::PlainObject>;
  // Group adjoint
  {lie<G>::Ad(g1)}->std::convertible_to<Eigen::Matrix<typename lie<G>::Scalar, lie<G>::Dof, lie<G>::Dof>>;
  // Group composition
  {lie<G>::composition(g1, g2)}->std::convertible_to<typename lie<G>::PlainObject>;
  // Run-time degrees of freedom (tangent space dimension).
  {lie<G>::dof(g1)}->std::convertible_to<Eigen::Index>;
  // Group inverse
  {lie<G>::inverse(g1)}->std::convertible_to<typename lie<G>::PlainObject>;
  // Check if two elements are (approximately) equal
  {lie<G>::isApprox(g1, g2, eps)}->std::convertible_to<bool>;
  // Group logarithm (maps from group to algebra)
  {lie<G>::log(g1)}->std::convertible_to<Eigen::Matrix<typename lie<G>::Scalar, lie<G>::Dof, 1>>;
} &&
// TANGENT INTERFACE
requires(const Eigen::Matrix<typename lie<G>::Scalar, lie<G>::Dof, 1> & a) {
  // Algebra adjoint
  {lie<G>::ad(a)}->std::convertible_to<Eigen::Matrix<typename lie<G>::Scalar, lie<G>::Dof, lie<G>::Dof>>;
  // Algebra exponential (maps from algebra to group)
  {lie<G>::exp(a)}->std::convertible_to<typename lie<G>::PlainObject>;
  // Right derivative of the exponential map
  {lie<G>::dr_exp(a)}->std::convertible_to<Eigen::Matrix<typename lie<G>::Scalar, lie<G>::Dof, lie<G>::Dof>>;
  // Right derivative of the exponential map inverse
  {lie<G>::dr_expinv(a)}->std::convertible_to<Eigen::Matrix<typename lie<G>::Scalar, lie<G>::Dof, lie<G>::Dof>>;
} && (
  // Cast to different scalar type
  !std::is_convertible_v<typename lie<G>::Scalar, double> ||
  requires (const G & g) {
    {lie<G>::template cast<double>(g)};
  }
) && (
  !std::is_convertible_v<typename lie<G>::Scalar, float> ||
  requires (const G & g) {
    {lie<G>::template cast<double>(g)};
  }
) &&
// PlainObject must be default-constructible
std::is_default_constructible_v<typename lie<G>::PlainObject> &&
std::is_copy_constructible_v<typename lie<G>::PlainObject> &&
// PlainObject must be assignable from G, and vice versa
std::is_assignable_v<G &, typename lie<G>::PlainObject> &&
std::is_assignable_v<typename lie<G>::PlainObject &, G>;

// clang-format on

////////////////////////////////////////////////
//// Free functions that dispatch to lie<G> ////
////////////////////////////////////////////////

// Types

/**
 * @brief Matrix of size Dof x Dof
 */
template<LieGroup G>
using TangentMap = Eigen::Matrix<typename lie<G>::Scalar, lie<G>::Dof, lie<G>::Dof>;

// Group interface

/**
 * @brief Group identity element
 */
template<LieGroup G>
inline auto Identity()
{
  return lie<G>::Identity();
}

/**
 * @brief Random group element
 */
template<LieGroup G>
inline auto Random()
{
  return lie<G>::Random();
}

/**
 * @brief Group adjoint \f$ Ad_g a \coloneq (G * \hat(a) * G^{-1})^{\wedge} \f$
 */
template<LieGroup G>
inline auto Ad(const G & g)
{
  return lie<G>::Ad(g);
}

/**
 * @brief Group binary composition
 */
template<LieGroup G, typename Arg>
inline auto composition(const G & g, Arg && a)
{
  return lie<G>::composition(g, std::forward<Arg>(a));
}

/**
 * @brief Group multinary composition
 */
template<LieGroup G, typename Arg, typename... Args>
inline auto composition(const G & g, Arg && a, Args &&... as)
{
  return composition(composition(g, std::forward<Arg>(a)), std::forward<Args>(as)...);
}

/**
 * @brief Group inverse
 */
template<LieGroup G>
inline auto inverse(const G & g)
{
  return lie<G>::inverse(g);
}

/**
 * @brief Check if two group elements are approximately equal
 */
template<LieGroup G, typename Arg>
inline auto isApprox(const G & g,
  Arg && a,
  typename lie<G>::Scalar eps = Eigen::NumTraits<typename lie<G>::Scalar>::dummy_precision())
{
  return lie<G>::isApprox(g, std::forward<Arg>(a), eps);
}

/**
 * @brief Group logarithm
 *
 * @see exp
 */
template<LieGroup G>
inline auto log(const G & g)
{
  return lie<G>::log(g);
}

// Tangent interface

/**
 * @brief Lie algebra adjoint \f$ ad_a b = [a, b] \f$
 */
template<LieGroup G, typename Arg>
inline auto ad(Arg && a)
{
  return lie<G>::ad(std::forward<Arg>(a));
}

/**
 * @brief Lie algebra exponential
 *
 * @see log
 */
template<LieGroup G, typename Arg>
inline auto exp(Arg && a)
{
  return lie<G>::exp(std::forward<Arg>(a));
}

/**
 * @brief Right derivative of exponential map
 */
template<LieGroup G, typename Arg>
inline auto dr_exp(Arg && a)
{
  return lie<G>::dr_exp(std::forward<Arg>(a));
}

/**
 * @brief Right derivative of exponential map inverse
 */
template<LieGroup G, typename Arg>
inline auto dr_expinv(Arg && a)
{
  return lie<G>::dr_expinv(std::forward<Arg>(a));
}

// Convenience methods

/**
 * @brief Left-plus
 */
template<LieGroup G, typename Derived>
inline typename lie<G>::PlainObject lplus(const G & g, const Eigen::MatrixBase<Derived> & a)
{
  return composition(::smooth::exp<G>(a), g);
}

/**
 * @brief Left-minus
 */
template<LieGroup G>
inline typename Eigen::Matrix<typename lie<G>::Scalar, lie<G>::Dof, 1> lminus(
  const G & g1, const G & g2)
{
  return log(composition(g1, inverse(g2)));
}

/**
 * @brief Left derivative of exponential map
 */
template<LieGroup G, typename Derived>
inline TangentMap<G> dl_exp(const Eigen::MatrixBase<Derived> & a)
{
  return Ad(::smooth::exp<G>(a)) * dr_exp<G>(a);
}

/**
 * @brief Left derivative of exponential map inverse
 */
template<LieGroup G, typename Derived>
inline TangentMap<G> dl_expinv(const Eigen::MatrixBase<Derived> & a)
{
  return -ad<G>(a) + dr_expinv<G>(a);
}

////////////////////////////////////////////////
//// Lie group interface for NativeLieGroup ////
////////////////////////////////////////////////

// clang-format off

/**
 * @brief Concept defining class with an internal Lie group interface.
 *
 * Concept satisfied if G has members that correspond to the LieGroup concept.
 */
template<typename G>
concept NativeLieGroup = requires
{
  typename G::Scalar;
  typename G::Tangent;
  typename G::PlainObject;
  {G::Dof}->std::convertible_to<Eigen::Index>;
  {G::Identity()}->std::convertible_to<typename G::PlainObject>;
  {G::Random()}->std::convertible_to<typename G::PlainObject>;
} &&
(G::Dof >= 1) &&
(G::Tangent::SizeAtCompileTime == G::Dof) &&
requires(const G & g1, const G & g2, typename G::Scalar eps) {
  {g1.dof()}->std::convertible_to<Eigen::Index>;  // degrees of freedom at runtime
  {g1.Ad()}->std::convertible_to<Eigen::Matrix<typename G::Scalar, G::Dof, G::Dof>>;
  {g1 * g2}->std::convertible_to<typename G::PlainObject>;
  {g1.inverse()}->std::convertible_to<typename G::PlainObject>;
  {g1.isApprox(g2, eps)}->std::convertible_to<bool>;
  {g1.log()}->std::convertible_to<Eigen::Matrix<typename G::Scalar, G::Dof, 1>>;
} &&
requires(const Eigen::Matrix<typename G::Scalar, G::Dof, 1> & a) {
  {G::ad(a)}->std::convertible_to<Eigen::Matrix<typename G::Scalar, G::Dof, G::Dof>>;
  {G::exp(a)}->std::convertible_to<typename G::PlainObject>;
  {G::dr_exp(a)}->std::convertible_to<Eigen::Matrix<typename G::Scalar, G::Dof, G::Dof>>;
  {G::dr_expinv(a)}->std::convertible_to<Eigen::Matrix<typename G::Scalar, G::Dof, G::Dof>>;
};

// clang-format off

/**
 * @brief LieGroup interface for NativeLieGroup
 */
template<NativeLieGroup G>
struct lie<G>
{
  // \cond
  using Scalar = typename G::Scalar;
  template<typename NewScalar>
  using CastT       = typename G::template CastT<NewScalar>;
  using PlainObject = typename G::PlainObject;

  static constexpr Eigen::Index Dof = G::Dof;

  // group interface

  static inline PlainObject Identity() { return G::Identity(); }
  static inline PlainObject Random() { return G::Random(); }
  static inline typename G::TangentMap Ad(const G & g) { return g.Ad(); }
  template<NativeLieGroup Go>
  static inline PlainObject composition(const G & g1, const Go & g2)
  {
    return g1.operator*(g2);
  }
  static inline Eigen::Index dof(const G &) { return G::Dof; }
  static inline PlainObject inverse(const G & g) { return g.inverse(); }
  template<NativeLieGroup Go>
  static inline bool isApprox(const G & g, const Go & go, Scalar eps)
  {
    return g.isApprox(go, eps);
  }
  static inline typename G::Tangent log(const G & g) { return g.log(); }
  template<typename NewScalar>
  static inline auto cast(const G & g)
  {
    return g.template cast<NewScalar>();
  }

  // tangent interface

  template<typename Derived>
  static inline typename G::TangentMap ad(const Eigen::MatrixBase<Derived> & a)
  {
    return G::ad(a);
  }
  template<typename Derived>
  static inline PlainObject exp(const Eigen::MatrixBase<Derived> & a)
  {
    return G::exp(a);
  }
  template<typename Derived>
  static inline typename G::TangentMap dr_exp(const Eigen::MatrixBase<Derived> & a)
  {
    return G::dr_exp(a);
  }
  template<typename Derived>
  static inline typename G::TangentMap dr_expinv(const Eigen::MatrixBase<Derived> & a)
  {
    return G::dr_expinv(a);
  }
  // \endcond
};

///////////////////////////////////////////////
//// Lie group interface for Eigen vectors ////
///////////////////////////////////////////////

/**
 * @brief Concept to identify Eigen column vectors
 */
template<typename G>
concept RnType = std::is_base_of_v<Eigen::MatrixBase<G>, G> && G::ColsAtCompileTime == 1;

/**
 * @brief LieGroup interface for RnType
 */
template<RnType G>
struct lie<G>
{
  // \cond
  static constexpr int Dof = G::SizeAtCompileTime;

  using Scalar      = typename G::Scalar;
  using PlainObject = Eigen::Matrix<Scalar, Dof, 1>;
  template<typename NewScalar>
  using CastT = Eigen::Matrix<NewScalar, Dof, 1>;

private:
  using Tangent    = Eigen::Matrix<Scalar, Dof, 1>;
  using TangentMap = Eigen::Matrix<Scalar, Dof, Dof>;

public:
  // group interface

  static inline PlainObject Identity() { return G::Zero(); }
  static inline PlainObject Random() { return G::Random(); }
  static inline TangentMap Ad(const G &) { return TangentMap::Identity(); }
  template<typename Derived>
  static inline PlainObject composition(const G & g1, const Eigen::MatrixBase<Derived> & g2)
  {
    return g1 + g2;
  }
  static inline Eigen::Index dof(const G & g) { return g.size(); }
  static inline PlainObject inverse(const G & g) { return -g; }
  template<typename Derived>
  static inline bool isApprox(const G & g, const Eigen::MatrixBase<Derived> & g2, Scalar eps)
  {
    return g.isApprox(g2, eps);
  }
  static inline Tangent log(const G & g) { return g; }
  template<typename NewScalar>
  static inline Eigen::Matrix<NewScalar, Dof, 1> cast(const G & g)
  {
    return g.template cast<NewScalar>();
  }

  // tangent interface

  template<typename Derived>
  static inline TangentMap ad(const Eigen::MatrixBase<Derived> &)
  {
    return TangentMap::Zero();
  }
  template<typename Derived>
  static inline PlainObject exp(const Eigen::MatrixBase<Derived> & a)
  {
    return a;
  }
  template<typename Derived>
  static inline TangentMap dr_exp(const Eigen::MatrixBase<Derived> &)
  {
    return TangentMap::Identity();
  }
  template<typename Derived>
  static inline TangentMap dr_expinv(const Eigen::MatrixBase<Derived> &)
  {
    return TangentMap::Identity();
  }
  // \endcond
};

///////////////////////////////////////////////////////////////
//// Lie group interface for built-in floating point types ////
///////////////////////////////////////////////////////////////

/**
 * @brief Concept to identify built-in scalars
 */
template<typename G>
concept FloatingPointType = std::is_floating_point_v<G>;

/**
 * @brief LieGroup interface for FloatingPointType
 */
template<FloatingPointType G>
struct lie<G>
{
  // \cond
  using Scalar      = G;
  using PlainObject = G;
  template<typename NewScalar>
  using CastT = NewScalar;

  static constexpr int Dof = 1;

private:
  using Tangent    = Eigen::Matrix<Scalar, Dof, 1>;
  using TangentMap = Eigen::Matrix<Scalar, Dof, Dof>;

public:
  // group interface

  static inline PlainObject Identity() { return G(0); }
  static inline PlainObject Random()
  {
    return G(Scalar(-1) + static_cast<Scalar>(rand()) / static_cast<Scalar>(RAND_MAX / 2));
  }
  static inline TangentMap Ad(G) { return TangentMap{1}; }
  static inline PlainObject composition(G g1, G g2) { return g1 + g2; }
  static inline Eigen::Index dof(G) { return 1; }
  static inline PlainObject inverse(G g) { return -g; }
  static inline bool isApprox(G g1, G g2, Scalar eps)
  {
    using std::abs;
    return abs<G>(g1 - g2) <= eps * abs<G>(g1);
  }
  static inline Tangent log(G g) { return Tangent{g}; }
  template<typename NewScalar>
  static inline NewScalar cast(G g)
  {
    return static_cast<NewScalar>(g);
  }

  // tangent interface

  template<typename Derived>
  static inline TangentMap ad(const Eigen::MatrixBase<Derived> &)
  {
    return TangentMap::Zero();
  }
  template<typename Derived>
  static inline PlainObject exp(const Eigen::MatrixBase<Derived> & a)
  {
    return a(0);
  }
  template<typename Derived>
  static inline Tangent vee(const Eigen::MatrixBase<Derived> & A)
  {
    return Tangent{A(0, 1)};
  }
  template<typename Derived>
  static inline TangentMap dr_exp(const Eigen::MatrixBase<Derived> &)
  {
    return TangentMap::Identity();
  }
  template<typename Derived>
  static inline TangentMap dr_expinv(const Eigen::MatrixBase<Derived> &)
  {
    return TangentMap::Identity();
  }
  // \endcond
};

}  // namespace smooth

#endif  // SMOOTH__LIE_GROUP_HPP_
