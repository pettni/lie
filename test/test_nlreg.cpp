#include <gtest/gtest.h>

#include <unsupported/Eigen/NumericalDiff>

#include "nlreg_data.hpp"
#include "smooth/optim/lm.hpp"

/**
 * @brief Functor that wraps a callable with multiple forms of operator()
 */
template <typename _Func, typename _InputType>
struct EigenFunctor
{
  using Scalar = typename _InputType::Scalar;
  using InputType = _InputType;
  using ValueType = std::invoke_result_t<_Func, InputType>;

  static constexpr int InputsAtCompileTime = InputType::SizeAtCompileTime;
  static constexpr int ValuesAtCompileTime = ValueType::SizeAtCompileTime;

  using JacobianType =
    Eigen::Matrix<Scalar, ValueType::SizeAtCompileTime, InputType::SizeAtCompileTime>;

  explicit EigenFunctor(int values, _Func && func)
    : values_(values), func_(std::forward<_Func>(func))
  {
  }

  /**
   * @brief Return (dynamic) dimension of output vector
   */
  int values() const { return values_; }

  /**
   * @brief Functor form used in unsupported/NumericalDiff
   */
  template <typename Derived1, typename Derived2>
  void operator()(const Eigen::MatrixBase<Derived1> & x, Eigen::MatrixBase<Derived2> & y) const
  {
    y = func_(x);
  }

 private:
  int values_;
  _Func func_;
};

// Functor that wraps NumericalDiff with the format expected by minimize
template <typename Input, typename F>
struct MyFunctor
{
  MyFunctor(int values, F && f) : ndiff(EigenFunctor<F, Input>(values, std::forward<F>(f))) {}

  auto operator()(const Input & x)
  {
    typename EigenFunctor<F, Input>::ValueType y(ndiff.values());
    ndiff(x, y);
    return y;
  }

  auto df(const Input & x)
  {
    typename EigenFunctor<F, Input>::JacobianType J(ndiff.values(), x.size());
    ndiff.df(x, J);
    return J;
  }

  Eigen::NumericalDiff<EigenFunctor<F, Input>> ndiff;
};

TEST(NlReg, Misra1a)
{
  static constexpr int np = 2;
  static constexpr int nobs = 14;

  auto [f, data, start1, start2, optim] = Misra1a();

  // static size
  {
    auto f_vec = [&](const Eigen::Matrix<double, np, 1> & p) -> Eigen::Matrix<double, nobs, 1> {
      return data.col(0)
        .binaryExpr(data.col(1), [&](double y, double x) { return f(y, x, p); })
        .eval();
    };

    MyFunctor<Eigen::Matrix<double, np, 1>, decltype(f_vec)> f_wrap(nobs, std::move(f_vec));

    Eigen::Matrix<double, np, 1> p1 = start1;
    minimize(f_wrap, p1);
    ASSERT_TRUE(p1.isApprox(optim, 1e-7));

    Eigen::Matrix<double, np, 1> p2 = start2;
    minimize(f_wrap, p2);
    ASSERT_TRUE(p2.isApprox(optim, 1e-7));
  }

  // dynamic size
  {
    auto f_vec = [&](const Eigen::Matrix<double, -1, 1> & p) -> Eigen::Matrix<double, -1, 1> {
      return data.col(0)
        .binaryExpr(data.col(1), [&](double y, double x) { return f(y, x, p); })
        .eval();
    };

    MyFunctor<Eigen::Matrix<double, -1, 1>, decltype(f_vec)> f_wrap(nobs, std::move(f_vec));

    Eigen::Matrix<double, -1, 1> p1 = start1;
    minimize(f_wrap, p1);
    ASSERT_TRUE(p1.isApprox(optim, 1e-7));

    Eigen::Matrix<double, -1, 1> p2 = start2;
    minimize(f_wrap, p2);
    ASSERT_TRUE(p2.isApprox(optim, 1e-7));
  }
}

TEST(NlReg, Kirby2)
{
  static constexpr int np = 5;
  static constexpr int nobs = 151;

  auto [f, data, start1, start2, optim] = Kirby2();

  // static size
  {
    auto f_vec = [&](const Eigen::Matrix<double, np, 1> & p) -> Eigen::Matrix<double, nobs, 1> {
      return data.col(0)
        .binaryExpr(data.col(1), [&](double y, double x) { return f(y, x, p); })
        .eval();
    };

    MyFunctor<Eigen::Matrix<double, np, 1>, decltype(f_vec)> f_wrap(nobs, std::move(f_vec));

    Eigen::Matrix<double, np, 1> p1 = start1;
    minimize(f_wrap, p1);
    ASSERT_TRUE(p1.isApprox(optim, 1e-7));

    Eigen::Matrix<double, np, 1> p2 = start2;
    minimize(f_wrap, p2);
    ASSERT_TRUE(p2.isApprox(optim, 1e-7));
  }

  // dynamic size
  {
    auto f_vec = [&](const Eigen::Matrix<double, -1, 1> & p) -> Eigen::Matrix<double, -1, 1> {
      return data.col(0)
        .binaryExpr(data.col(1), [&](double y, double x) { return f(y, x, p); })
        .eval();
    };

    MyFunctor<Eigen::Matrix<double, -1, 1>, decltype(f_vec)> f_wrap(nobs, std::move(f_vec));

    Eigen::Matrix<double, -1, 1> p1 = start1;
    minimize(f_wrap, p1);
    ASSERT_TRUE(p1.isApprox(optim, 1e-7));

    Eigen::Matrix<double, -1, 1> p2 = start2;
    minimize(f_wrap, p2);
    ASSERT_TRUE(p2.isApprox(optim, 1e-7));
  }
}

// Cant handle this one..
//
// TEST(NlReg, MGH09)
// {
//   // only compile-time sizes for now..
//   static constexpr int np = 4;
//   static constexpr int nobs = 11;

//   auto [f, data, start1, start2, optim] = MGH09();

//   auto f_vec = [&] (const Eigen::Matrix<double, np, 1> & p) -> Eigen::Matrix<double, nobs, 1> {
//     return data.col(0).binaryExpr(data.col(1), [&](double y, double x) {
//       return f(y, x, p);
//     }).eval();
//   };

//   MyFunctor<Eigen::Matrix<double, np, 1>, decltype(f_vec)> f_wrap(std::move(f_vec));

//   Eigen::Matrix<double, np, 1> p1 = start1;
//   minimize(f_wrap, p1);
//   ASSERT_TRUE(p1.isApprox(optim, 1e-7));

//   Eigen::Matrix<double, np, 1> p2 = start2;
//   minimize(f_wrap, p2);
//   ASSERT_TRUE(p2.isApprox(optim, 1e-7));
// }
