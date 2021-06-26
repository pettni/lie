#ifndef SMOOTH__UTILS_HPP_
#define SMOOTH__UTILS_HPP_

#include <array>
#include <cstddef>
#include <functional>
#include <numeric>
#include <utility>

#include <Eigen/Core>

namespace smooth::utils {

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

///////////////////////
// TUPLE STATE UTILS //
///////////////////////

template<typename Tuple>
struct tuple_dof
{};

/**
 * @brief Compile-time size of a tuple of variables.
 *
 * If at least one variable is dynamically sized (size -1), this returns -1.
 */
template<typename... Wrt>
struct tuple_dof<std::tuple<Wrt...>>
{
  static constexpr int value = std::min<int>({std::decay_t<Wrt>::SizeAtCompileTime...}) == -1
                               ? std::min<int>({std::decay_t<Wrt>::SizeAtCompileTime...})
                               : (std::decay_t<Wrt>::SizeAtCompileTime + ...);
};

/**
 * @brief Cast a tuple of variables to a new scalar type.
 */
template<typename Scalar, typename... _Wrt>
auto tuple_cast(const std::tuple<_Wrt...> & wrt)
{
  std::tuple<
    typename std::decay_t<decltype(std::decay_t<_Wrt>{}.template cast<Scalar>())>::PlainObject...>
    ret;
  static_for<sizeof...(_Wrt)>(
    [&](auto i) { std::get<i>(ret) = std::get<i>(wrt).template cast<Scalar>(); });
  return ret;
}

/**
 * @brief Add a tangent vector to a tuple of variables.
 */
template<typename Derived, typename... _Wrt>
auto tuple_plus(const std::tuple<_Wrt...> & wrt, const Eigen::MatrixBase<Derived> & a)
{
  std::tuple<typename std::decay_t<_Wrt>::PlainObject...> ret;
  std::size_t i_beg = 0;
  static_for<sizeof...(_Wrt)>([&](auto i) {
    constexpr auto i_size =
      std::tuple_element_t<i, std::tuple<std::decay_t<_Wrt>...>>::SizeAtCompileTime;
    std::size_t i_len = std::get<i>(wrt).size();
    std::get<i>(ret)  = std::get<i>(wrt) + a.template segment<i_size>(i_beg, i_len);
    i_beg += i_len;
  });
  return ret;
}

/////////////////////////////////
// COMPILE-TIME MATRIX ALGEBRA //
/////////////////////////////////

/**
 * @brief Elementary structure for compile-time matrix algebra
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

#endif  // SMOOTH__UTILS_HPP_
