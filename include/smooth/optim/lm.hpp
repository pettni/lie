#ifndef SMOOTH__OPTIM__LM_HPP_
#define SMOOTH__OPTIM__LM_HPP_

#include <Eigen/Core>
#include <Eigen/Jacobi>
#include <Eigen/QR>
#include <iostream>

/**
 * @brief Solve structured least-squares probelm
 *
 *   min_x \| [J ; diag(d)] x + [r; 0] \|^2
 *
 * Where a is a size N vector and J is M x N with M >= N
 * and it must hold that J^T J + D^T D is positive semi-definite
 *
 * @param J_qr QR decomposition J P = Q R of J with column pivoting
 * @param d vector of length N representing diagonal matrix
 * @param r vector with same number of elements as there are rows in J
 * @param[out] Rt output matrix s.t. Rt' Rt = P' (J' J + D' D) P
 *
 * Currenly only supports square J matrices
 */
template <int N, int M = N>
requires(M >= N) Eigen::Matrix<double, N, 1> solve_ls(
  const Eigen::ColPivHouseholderQR<Eigen::Matrix<double, M, N>> & J_qr,
  const Eigen::Matrix<double, N, 1> & d,
  const Eigen::Matrix<double, M, 1> & r,
  std::optional<Eigen::Ref<Eigen::Matrix<double, N, N>>> Rt = {})
{
  // form system
  // [A; B] x + [a; b]
  // where A = R
  //       B = P' diag(D) P
  //       a = Q' r
  //       b = 0
  Eigen::Matrix<double, N, N> A = J_qr.matrixR().template topLeftCorner<N, N>();
  Eigen::Matrix<double, N, N> B = (J_qr.colsPermutation().transpose() * d).asDiagonal();

  Eigen::Matrix<double, N, 1> a, b;
  a = (J_qr.matrixQ().transpose() * r).template head<N>();
  b.setZero();

  // QR decomposition of [A; B] with Givens rotations;
  // where it is known that A is A triangular and B is diagonal
  //
  // algorithm:
  //   R = A, Q = I
  //   for each nonzero in B part:
  //      find rotation G that eliminates nonzero without introducing new zeros
  //      Q = Q G
  //      R = G' R
  Eigen::JacobiRotation<double> G;
  for (auto col = 0u; col != N; ++col)  // for each column
  {
    for (auto row = 0u; row != col + 1; ++row) {  // for each row above diagonal
      G.makeGivens(A(col, col), B(row, col));     // eliminates B(row, col)

      // perform matrix multiplication R = G' R
      A(col, col) = G.c() * A(col, col) - G.s() * B(row, col);
      for (int i = col + 1; i < N; ++i) {
        double tmp = G.c() * A(col, i) - G.s() * B(row, i);
        B(row, i) = G.s() * A(col, i) + G.c() * B(row, i);
        A(col, i) = tmp;
      }

      // At the end we need Q matrix to multiply rhs as
      // Q' * rhs = (I * G0 * ... * Gk)' * rhs = Gk' * ... * G0' * rhs
      //
      // We can therefore do it as we go, here we set
      // rhs = G * rhs
      double tmp = G.s() * a(col) + G.c() * b(row);
      a(col) = G.c() * a(col) - G.s() * b(row);
      b(row) = tmp;
    }
  }

  if (Rt.has_value()) {
    Rt.value().template triangularView<Eigen::Upper>()
      = A.template triangularView<Eigen::Upper>();
  }

  // check rank of upper-diagonal A (may happen if D not full-rank)
  int rank = 0;
  for (auto i = 0u; i != N && A(i, i) >= Eigen::NumTraits<double>::dummy_precision(); ++i) {
    ++rank;
  }

  // solve triangular system A z = a to obtain z = R^-1 z
  Eigen::Matrix<double, N, 1> sol;
  sol.tail(N - rank).setZero();
  sol.head(rank) = A.topLeftCorner(rank, rank).template triangularView<Eigen::Upper>().solve(a.head(rank));

  // solution is now equal to P z
  return -(J_qr.colsPermutation() * sol);
}

/**
 * @brief Approximate a Levenberg-Marquardt parameter lambda s.t. if x solves
 *
 *   \| [J; sqrt(lambda) * diag(D)] x  +  [r ; 0] \|^2
 *
 * then either
 *  * lambda = 0 AND \|diag(D) * x\| <= 0.1 Delta
 *    OR
 *  * lambda > 0 AND 0.9 Delta <= \|diag(D) * x\| <= 1.1 Delta
 *
 * @param J matrix MxN
 * @param d vector size Nx1
 * @param r vector size Mx1
 * @param Delta scalar
 * @param[out] x output least squares solution for final lambda
 * */
template <int N, int M = N>
requires(M >= N)
double lmpar(
  const Eigen::Matrix<double, M, N> J,
  const Eigen::Matrix<double, N, 1> & d,
  const Eigen::Matrix<double, M, 1> & r,
  double Delta,
  std::optional<Eigen::Ref<Eigen::Matrix<double, N, 1>>> x = {}
)
{
  // calculate qr decomposition
  Eigen::ColPivHouseholderQR<Eigen::Matrix<double, M, N>> J_qr(J);

  // calculate phi(0)
  Eigen::Matrix<double, N, 1> z_iter = J_qr.solve(-r);
  Eigen::Matrix<double, N, N> Rt_iter = J_qr.matrixR();

  Eigen::Matrix<double, N, 1> q = d.cwiseProduct(z_iter);
  double q_norm = q.stableNorm();

  double alpha = 0;
  double phi = q_norm - Delta;
  double dphi;

  if (phi <= 0.1 * Delta) {
    // zero solution fulfills condition
    if (x.has_value()) {
      x.value() = z_iter;
    }
    return alpha;
  }

  // lower bound
  double l = 0;
  if (J_qr.rank() == N) {
    // can caluclate dphi(0) here
    Eigen::Matrix<double, N, 1> y = J_qr.colsPermutation().inverse() * (d.cwiseProduct(q) / q_norm);
    J_qr.matrixR().template triangularView<Eigen::Upper>().transpose().solveInPlace(y);

    dphi = -q_norm * y.squaredNorm();
    l = std::max(l, -phi / dphi);
  }

  // upper bound
  double u = ((J * d.cwiseInverse().asDiagonal()).transpose() * r).stableNorm() / Delta;

  for (auto i = 0u; i != 20; ++i) {
    // ensure alpha stays within bounds
    if (!(l < alpha && alpha < u)) {
      alpha = std::max<double>(0.001 * u, sqrt(l * u));
    }

    // calculate phi
    z_iter = solve_ls<N, M>(J_qr, sqrt(alpha) * d, r, Rt_iter);
    Eigen::Matrix<double, N, 1> q = d.cwiseProduct(z_iter);
    double q_norm = q.stableNorm();
    phi = q_norm - Delta;

    if (std::abs(phi) <= 0.1 * Delta) {
      break;  // condition fulfilled
    }

    // calculate derivative of phi
    Eigen::Matrix<double, N, 1> y = J_qr.colsPermutation().transpose() * (d.cwiseProduct(q) / q_norm);
    Rt_iter.template triangularView<Eigen::Upper>().transpose().solveInPlace(y);
    dphi = -q_norm * y.squaredNorm();

    // update bounds
    l = std::max<double>(l, alpha - phi / dphi);
    if (phi < 0) {
      u = alpha;
    }

    // update alpha
    alpha = alpha - ((phi + Delta) / Delta) * (phi / dphi);
  }

  if (x.has_value()) {
    x.value() = z_iter;
  }

  return alpha;
}

template <typename _F>
void optimize(_F && f)
{
  using F = std::decay_t<_F>;
  using G = typename F::Group;
  using Tangent = typename G::Tangent;

  using R = decltype(f(G{}));

  static constexpr int num_res = R::SizeAtCompileTime;

  using Jac = Eigen::Matrix<double, num_res, G::lie_dof>;

  // optimization variable
  G g = G::Random();
  Jac J = f.df(g);
  R r = f(g);

  // trust region
  double Delta = 1;

  // scaling parameters
  Eigen::Matrix<double, G::lie_dof, 1> diag = J.colwise().stableNorm();

  std::cout << "starting parameters" << std::endl;
  std::cout << "g " << g << std::endl;
  std::cout << "r " << r.transpose() << std::endl;
  std::cout << "Delta " << Delta << std::endl;
  std::cout << "diag " << diag.transpose() << std::endl;

  std::cout << std::endl << std::endl;

  for (int i = 0; i != 10; ++i) {
    std::cout << "iteration " << i << std::endl;

    // calculate parameter (for now 1 / trust region)
    double lambda = 1. / Delta;
    std::cout << "calculated parameter " << lambda << std::endl;

    // set up least squares problem
    Eigen::Matrix<double, num_res + G::lie_dof, G::lie_dof> lhs;
    lhs.template block<G::lie_dof, G::lie_dof>(0, 0) = J;
    lhs.template block<G::lie_dof, G::lie_dof>(G::lie_dof, 0) = sqrt(lambda) * diag.asDiagonal();

    Eigen::Matrix<double, num_res + G::lie_dof, 1> rhs;
    rhs.template head<num_res>() = -r;
    rhs.template tail<G::lie_dof>().setZero();

    // solve least-squares problem with a big hammer
    Tangent a_hammer = lhs.colPivHouseholderQr().solve(rhs);

    // solve least-squares problem with a small scalpel
    Eigen::ColPivHouseholderQR<Eigen::Matrix<double, G::lie_dof, G::lie_dof>> J_qrdecomp(J);
    Tangent a = solve_ls<G::lie_dof>(J_qrdecomp, sqrt(lambda) * diag, r);

    std::cout << "step       " << a.transpose() << std::endl;
    std::cout << "step truth " << a_hammer.transpose() << std::endl;

    double rpre_n = r.stableNorm();

    // update optimization variables
    g += a;
    J = f.df(g);
    r = f(g);
    std::cout << "new g " << g << std::endl;
    std::cout << "new r " << r.transpose() << std::endl;

    // calculate actual to predicted reduction
    double up = 1. - Eigen::numext::abs2(r.stableNorm() / rpre_n);
    double dn1 = Eigen::numext::abs2((J * a).stableNorm() / rpre_n);
    double dn2 =
      2 * Eigen::numext::abs2(std::sqrt(lambda) * (diag.cwiseProduct(a)).stableNorm() / rpre_n);
    double rho = up / (dn1 + dn2);
    std::cout << "rho " << rho << std::endl;

    // update trust region
    if (rho < 0.25) {
      Delta /= 2;
    } else if ((rho <= 0.75 && lambda == 0) || lambda >= 0.75) {
      Delta = 2 * diag.cwiseProduct(a).stableNorm();
    }
    std::cout << "updated Delta " << Delta << std::endl;

    // update scaling
    const auto J_n = J.colwise().stableNorm().transpose().eval();
    diag = diag.cwiseMax(J_n);
    std::cout << "updated diag " << diag.transpose() << std::endl;

    std::cout << std::endl << std::endl;
  }
}

#endif  // SMOOTH__OPTIM__LM_HPP_
