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

#include "smooth/so2.hpp"
#include "smooth/so3.hpp"

TEST(SO3, Quaternion)
{
  for (auto i = 0u; i != 5; ++i) {
    auto g1       = smooth::SO3d::Random();
    const auto g2 = smooth::SO3d::Random();

    const auto g_prod = smooth::SO3d(g1.quat() * g2.quat());

    ASSERT_TRUE(g_prod.isApprox(g1 * g2));
  }

  for (auto i = 0u; i != 5; ++i) {
    smooth::SO3d g1, g2;
    g1.quat() = Eigen::Quaterniond::UnitRandom();
    g2.quat() = Eigen::Quaterniond::UnitRandom();

    const auto g_prod = smooth::SO3d(g1.quat() * g2.quat());

    ASSERT_TRUE(g_prod.isApprox(g1 * g2));
  }
}

TEST(SO3, Eulerangles)
{
  double ang = 0.345;
  smooth::SO3d g1(Eigen::Quaterniond(std::cos(ang / 2), std::sin(ang / 2), 0, 0));
  ASSERT_EQ(g1.eulerAngles().z(), ang);

  smooth::SO3d g2(Eigen::Quaterniond(std::cos(ang / 2), 0, std::sin(ang / 2), 0));
  ASSERT_EQ(g2.eulerAngles().y(), ang);

  smooth::SO3d g3(Eigen::Quaterniond(std::cos(ang / 2), 0, 0, std::sin(ang / 2)));
  ASSERT_EQ(g3.eulerAngles().x(), ang);
}

TEST(SO3, Action)
{
  for (auto i = 0u; i != 5; ++i) {
    Eigen::Quaterniond q = Eigen::Quaterniond::UnitRandom();
    smooth::SO3d g(q);

    Eigen::Vector3d v = Eigen::Vector3d::Random();

    ASSERT_TRUE((g * v).isApprox(q * v));
    ASSERT_TRUE((g * v).isApprox(g.quat() * v));
    ASSERT_TRUE((g * v).isApprox(g.matrix() * v));
  }
}

TEST(SO3, ProjectLift)
{
  for (auto i = 0u; i != 5; ++i) {
    const smooth::SO3d g = smooth::SO3d::Random();
    const auto so2       = g.project_so2();
    const auto so3       = so2.lift_so3();

    ASSERT_NEAR(g.eulerAngles().x(), so3.eulerAngles().x(), 1e-6);
  }
}

TEST(SO2, Project)
{
  using std::cos, std::sin;

  std::srand(14);

  for (auto i = 0u; i != 10; ++i) {
    double angle = rand();

    smooth::SO2d so2(angle);

    smooth::SO3d so31(Eigen::Quaterniond(cos(angle / 2), -1e-5, -1e-5, sin(angle / 2)));
    smooth::SO3d so32(Eigen::Quaterniond(cos(angle / 2), -1e-5, 1e-5, sin(angle / 2)));
    smooth::SO3d so33(Eigen::Quaterniond(cos(angle / 2), 1e-5, -1e-5, sin(angle / 2)));
    smooth::SO3d so34(Eigen::Quaterniond(cos(angle / 2), 1e-5, 1e-5, sin(angle / 2)));

    ASSERT_TRUE(so31.project_so2().isApprox(so2, 1e-4));
    ASSERT_TRUE(so32.project_so2().isApprox(so2, 1e-4));
    ASSERT_TRUE(so33.project_so2().isApprox(so2, 1e-4));
    ASSERT_TRUE(so34.project_so2().isApprox(so2, 1e-4));
  }
}

TEST(SE3, SignedInverse)
{
  Eigen::Vector4d c = Eigen::Vector4d::Random();

  smooth::SO3d g1(Eigen::Quaterniond(c(0), c(1), c(2), c(3)));
  smooth::SO3d g2(Eigen::Quaterniond(-c(0), -c(1), -c(2), -c(3)));

  ASSERT_TRUE(g1.isApprox(g2));
  ASSERT_LE((g1 - g2).cwiseAbs().maxCoeff(), 1e-10);
}
