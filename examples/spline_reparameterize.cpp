// Copyright (C) 2021-2022 Petter Nilsson. MIT License.

/**
 * @file bspline.cpp Curve example.
 */

#include "smooth/se2.hpp"
#include "smooth/spline/fit.hpp"
#include "smooth/spline/reparameterize.hpp"
#include "smooth/spline/spline.hpp"

#ifdef ENABLE_PLOTTING
#include <matplot/matplot.h>
#endif

/**
 * @brief Define and plot a B-spline on \f$ \mathbb{SO}(3)\f$.
 */
int main(int, char const **)
{
  std::srand(1);

  std::vector<double> tt{1, 2, 3, 4, 5, 6};
  std::vector<smooth::SE2d> gg{
    smooth::SE2d::Random(),
    smooth::SE2d::Random(),
    smooth::SE2d::Random(),
    smooth::SE2d::Random(),
    smooth::SE2d::Random(),
    smooth::SE2d::Random(),
  };

  auto c = smooth::fit_spline(tt, gg, smooth::spline_specs::FixedDerCubic<smooth::SE2d, 2>{});

  Eigen::Vector3d vmax(1, 1, 1), amax(1, 1, 1);

  auto sfun = smooth::reparameterize_spline(c, -vmax, vmax, -amax, amax, 1, 0);

  std::vector<double> tvec, svec, vvec, avec;
  std::vector<double> vx, vy, w, ax, ay, dw;
  std::vector<double> rvx, rvy, rw, rax, ray, rdw;

  for (double t = 0; t < sfun.t_max(); t += 0.01) {
    Eigen::Matrix<double, 1, 1> ds, d2s;
    double s = sfun(t, ds, d2s);

    Eigen::Vector3d vel, acc;
    c(s, vel, acc);

    Eigen::Vector3d vel_reparam = vel * ds(0);
    Eigen::Vector3d acc_reparam = vel * d2s(0) + acc * ds(0) * ds(0);

    tvec.push_back(t);
    svec.push_back(s);
    vvec.push_back(ds(0));
    avec.push_back(d2s(0));

    vx.push_back(vel.x());
    vy.push_back(vel.y());
    w.push_back(vel.z());

    ax.push_back(acc.x());
    ay.push_back(acc.y());
    dw.push_back(acc.z());

    rvx.push_back(vel_reparam.x());
    rvy.push_back(vel_reparam.y());
    rw.push_back(vel_reparam.z());

    rax.push_back(acc_reparam.x());
    ray.push_back(acc_reparam.y());
    rdw.push_back(acc_reparam.z());
  }

#ifdef ENABLE_PLOTTING
  matplot::figure();
  matplot::hold(matplot::on);
  matplot::title("Reparameterization");
  matplot::plot(tvec, svec, "r")->line_width(2);
  matplot::plot(tvec, vvec, "g")->line_width(2);
  matplot::plot(tvec, avec, "b")->line_width(2);
  matplot::legend({"s", "v", "a"});

  matplot::figure();
  matplot::hold(matplot::on);
  matplot::title("Reparameterized velocities");
  matplot::plot(tvec, vx, "r--")->line_width(2);
  matplot::plot(tvec, vy, "g--")->line_width(2);
  matplot::plot(tvec, w, "b--")->line_width(2);
  matplot::plot(tvec, rvx, "r")->line_width(2);
  matplot::plot(tvec, rvy, "g")->line_width(2);
  matplot::plot(tvec, rw, "b")->line_width(2);

  matplot::figure();
  matplot::hold(matplot::on);
  matplot::title("Reparameterized accelerations");
  matplot::plot(tvec, ax, "r--")->line_width(2);
  matplot::plot(tvec, ay, "g--")->line_width(2);
  matplot::plot(tvec, dw, "b--")->line_width(2);
  matplot::plot(tvec, rax, "r")->line_width(2);
  matplot::plot(tvec, ray, "g")->line_width(2);
  matplot::plot(tvec, rdw, "b")->line_width(2);

  matplot::show();
#endif

  return 0;
}
