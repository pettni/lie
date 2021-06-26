#include <smooth/so3.hpp>
#include <smooth/nls.hpp>


struct MyFunctor
{
  using Group = smooth::SO3d;

  Eigen::Vector3d operator()(const smooth::SO3d & g)
  {
    return g.log();
  }

  Eigen::Matrix3d df(const smooth::SO3d & g)
  {
    return smooth::SO3d::dr_expinv(g.log());
  }
};


int main()
{
  MyFunctor f;
  auto g = smooth::SO3d::Random();
  smooth::minimize(f, smooth::wrt(g));

  return EXIT_SUCCESS;
}
