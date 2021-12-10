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

#include <gtest/gtest.h>

#ifdef ENABLE_AUTODIFF_TESTS
#include "smooth/compat/autodiff.hpp"
#endif

#ifdef ENABLE_CERESDIFF_TESTS
#include "smooth/compat/ceres.hpp"
#endif

#include "smooth/diff.hpp"
#include "smooth/so3.hpp"

using namespace smooth;

template<diff::Type dm, typename TypeParam>
void run_rminus_test()
{
  TypeParam g1, g2;
  g1.setRandom();
  g2.setRandom();

  auto [f1, jac1] = diff::dr<1, dm>(
    [&g2](auto v1) { return v1 - g2.template cast<typename decltype(v1)::Scalar>(); }, wrt(g1));
  auto [f2, jac2] = diff::dr<1, dm>(
    [&g1](auto v2) { return g1.template cast<typename decltype(v2)::Scalar>() - v2; }, wrt(g2));
  auto [f3, jac3] = diff::dr<1, dm>([](auto v1, auto v2) { return v1 - v2; }, wrt(g1, g2));

  static_assert(decltype(jac1)::RowsAtCompileTime == traits::man<TypeParam>::Dof, "Error");
  static_assert(decltype(jac1)::ColsAtCompileTime == traits::man<TypeParam>::Dof, "Error");
  static_assert(decltype(jac2)::RowsAtCompileTime == traits::man<TypeParam>::Dof, "Error");
  static_assert(decltype(jac2)::ColsAtCompileTime == traits::man<TypeParam>::Dof, "Error");
  static_assert(decltype(jac3)::RowsAtCompileTime == traits::man<TypeParam>::Dof, "Error");
  static_assert(decltype(jac3)::ColsAtCompileTime == 2 * traits::man<TypeParam>::Dof, "Error");

  auto v         = g1 - g2;
  auto jac1_true = TypeParam::dr_expinv(v);
  auto jac2_true = (-TypeParam::dl_expinv(v)).eval();

  ASSERT_TRUE(f1.isApprox(f2));
  ASSERT_TRUE(f1.isApprox(v));

  ASSERT_TRUE(jac1.isApprox(jac1_true, 1e-5));
  ASSERT_TRUE(jac2.isApprox(jac2_true, 1e-5));
  ASSERT_TRUE(jac1.isApprox(jac3.template leftCols<traits::man<TypeParam>::Dof>(), 1e-5));
  ASSERT_TRUE(jac2.isApprox(jac3.template rightCols<traits::man<TypeParam>::Dof>(), 1e-5));
}

template<diff::Type dm, typename TypeParam>
void run_composition_test()
{
  TypeParam g1, g2;
  g1.setRandom();
  g2.setRandom();

  auto [f1, jac1] = diff::dr<1, dm>([](auto v1, auto v2) { return v1 * v2; }, wrt(g1, g2));

  static_assert(decltype(jac1)::RowsAtCompileTime == traits::man<TypeParam>::Dof, "Error");
  static_assert(decltype(jac1)::ColsAtCompileTime == 2 * traits::man<TypeParam>::Dof, "Error");

  ASSERT_EQ(jac1.rows(), traits::man<TypeParam>::Dof);
  ASSERT_EQ(jac1.cols(), 2 * traits::man<TypeParam>::Dof);

  auto jac1_true = g2.inverse().Ad();
  auto jac2_true = decltype(jac1_true)::Identity();

  ASSERT_TRUE(f1.isApprox(g1 * g2, 1e-5));

  ASSERT_TRUE(jac1.template leftCols<traits::man<TypeParam>::Dof>().isApprox(jac1_true, 1e-5));
  ASSERT_TRUE(jac1.template rightCols<traits::man<TypeParam>::Dof>().isApprox(jac2_true, 1e-5));
}

template<diff::Type dm, typename TypeParam>
void run_exp_test()
{
  typename TypeParam::Tangent a;
  a.setRandom();

  auto [f, jac] = diff::dr<1, dm>(
    [](auto var) { return TypeParam::template CastT<typename decltype(var)::Scalar>::exp(var); },
    wrt(a));

  static_assert(decltype(jac)::RowsAtCompileTime == TypeParam::Dof, "Error");
  static_assert(decltype(jac)::ColsAtCompileTime == TypeParam::Dof, "Error");

  auto jac_true = TypeParam::dr_exp(a);

  ASSERT_TRUE(f.isApprox(TypeParam::exp(a), 1e-5));
  ASSERT_TRUE(jac.isApprox(jac_true, 1e-5));
}

template<int Nx, int Ny, diff::Type DiffType>
void test_linear(double prec = 1e-10)
{
  for (auto it = 0u; it != 10; ++it) {
    Eigen::Matrix<double, Nx, 1> t = Eigen::Matrix<double, Nx, 1>::Random();

    Eigen::Matrix<double, Ny, Nx> H = Eigen::Matrix<double, Ny, Nx>::Random();
    Eigen::Matrix<double, Ny, 1> h  = Eigen::Matrix<double, Ny, 1>::Random();

    auto f = [&H, &h]<typename T>(const Eigen::Matrix<T, Nx, 1> & var) -> Eigen::Matrix<T, Ny, 1> {
      return H * var + h;
    };

    const auto [fval, dr_f] = diff::dr<1, DiffType>(f, wrt(t));
    ASSERT_TRUE(fval.isApprox(f(t)));
    ASSERT_TRUE(dr_f.isApprox(H, prec));
  }
}

template<diff::Type DiffType>
void test_second()
{
  const auto f = []<typename T>(const Eigen::Vector3<T> & xx) -> T { return xx.squaredNorm(); };

  Eigen::Vector3d g{2, 4, 6};

  const auto [F, df, d2f] = diff::dr<2, DiffType>(f, wrt(g));

  ASSERT_NEAR(F, 2 * 2 + 4 * 4 + 6 * 6, 1e-6);
  ASSERT_TRUE(df.isApprox(Eigen::RowVector3d{4, 8, 12}, 1e-4));
  ASSERT_TRUE(d2f.isApprox(Eigen::Matrix3d{{2, 0, 0}, {0, 2, 0}, {0, 0, 2}}, 1e-4));
}

TEST(Differentiation, NumericalSuite)
{
  test_linear<3, 3, diff::Type::Numerical>(1e-6);
  test_linear<3, 10, diff::Type::Numerical>(1e-6);
  test_linear<10, 3, diff::Type::Numerical>(1e-6);

  run_rminus_test<diff::Type::Numerical, SO3d>();
  run_composition_test<diff::Type::Numerical, SO3d>();
  run_exp_test<diff::Type::Numerical, SO3d>();

  test_second<diff::Type::Numerical>();
}

#ifdef ENABLE_AUTODIFF_TESTS
TEST(Differentiation, AutodiffSuite)
{
  test_linear<3, 3, diff::Type::Autodiff>();
  test_linear<3, 10, diff::Type::Autodiff>();
  test_linear<10, 3, diff::Type::Autodiff>();

  run_rminus_test<diff::Type::Autodiff, SO3d>();
  run_composition_test<diff::Type::Autodiff, SO3d>();
  run_exp_test<diff::Type::Autodiff, SO3d>();

  test_second<diff::Type::Autodiff>();
}
#endif

#ifdef ENABLE_CERESDIFF_TESTS
TEST(Differentiation, CeresSuite)
{
  test_linear<3, 3, diff::Type::Ceres>();
  test_linear<3, 10, diff::Type::Ceres>();
  test_linear<10, 3, diff::Type::Ceres>();

  run_rminus_test<diff::Type::Ceres, SO3d>();
  run_composition_test<diff::Type::Ceres, SO3d>();
  run_exp_test<diff::Type::Ceres, SO3d>();
}
#endif

TEST(Differentiation, Const)
{
  const auto f    = [](const auto & xx) { return xx.log(); };
  SO3d g          = SO3d::Random();
  const SO3d g_nc = g;

  const auto [v1, d1] = diff::detail::dr_numerical(f, wrt(g));
  const auto [v2, d2] = diff::detail::dr_numerical(f, wrt(g_nc));

  ASSERT_TRUE(v1.isApprox(v2));
  ASSERT_TRUE(d1.isApprox(d2));
}

TEST(Differentiation, Dynamic)
{
  Eigen::VectorXd v(3);
  v.setRandom();

  auto [f1, jac1] =
    diff::dr<1, diff::Type::Numerical>([](auto v1) { return (2 * v1).eval(); }, wrt(v));

  static_assert(decltype(jac1)::RowsAtCompileTime == -1, "Error");
  static_assert(decltype(jac1)::ColsAtCompileTime == -1, "Error");

  ASSERT_EQ(f1.size(), 3);
  ASSERT_EQ(jac1.cols(), 3);
  ASSERT_EQ(jac1.rows(), 3);

  Eigen::Matrix3d diag = Eigen::Vector3d::Constant(2).asDiagonal();
  ASSERT_TRUE(jac1.isApprox(diag, 1e-5));
}

TEST(Differentiation, Mixed)
{
  Eigen::Vector3d v(3);
  v.setRandom();

  auto [f1, jac1] = diff::dr<1, diff::Type::Numerical>(
    [](auto v1) {
      Eigen::VectorXd ret(2);
      ret << 2. * v1(1), 2. * v1(0);
      return ret;
    },
    wrt(v));

  static_assert(decltype(jac1)::RowsAtCompileTime == -1, "Error");
  static_assert(decltype(jac1)::ColsAtCompileTime == 3, "Error");

  ASSERT_EQ(f1.size(), 2);
  ASSERT_EQ(jac1.cols(), 3);
  ASSERT_EQ(jac1.rows(), 2);

  Eigen::Matrix<double, 2, 3> diag;
  diag.setZero();
  diag(0, 1) = 2;
  diag(1, 0) = 2;
  ASSERT_TRUE(jac1.isApprox(diag, 1e-5));
}
