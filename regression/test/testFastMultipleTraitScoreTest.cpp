#include "IO.h"

#include "FastMultipleTraitLinearRegressionScoreTest.h"
#include "MathMatrix.h"
#include "MathVector.h"
#include "MatrixIO.h"

#include "Formula.h"
#include "SimpleTimer.h"

int main(int argc, char* argv[]) {
  Matrix G;
  Matrix Y;
  Matrix Cov;

  LoadMatrix("input.mt.g", G);
  LoadMatrix("input.mt.y", Y);
  LoadMatrix("input.mt.cov", Cov);
  Cov.SetColumnLabel(0, "c1");
  Cov.SetColumnLabel(1, "c2");
  Y.SetColumnLabel(0, "y1");
  Y.SetColumnLabel(1, "y2");
  Y.SetColumnLabel(2, "y3");

  FormulaVector tests;
  {
    const char* tp1[] = {"y1"};
    const char* tc1[] = {"c1"};
    std::vector<std::string> p1(tp1, tp1 + 1);
    std::vector<std::string> c1(tc1, tc1 + 1);
    tests.add(p1, c1);
  }

  {
    const char* tp1[] = {"y2"};
    const char* tc1[] = {"c2"};
    std::vector<std::string> p1(tp1, tp1 + 1);
    std::vector<std::string> c1(tc1, tc1 + 1);
    tests.add(p1, c1);
  }

  {
    const char* tp1[] = {"y2"};
    const char* tc1[] = {"c1", "c2"};
    std::vector<std::string> p1(tp1, tp1 + 1);
    std::vector<std::string> c1(tc1, tc1 + 2);
    tests.add(p1, c1);
  }

  {
    const char* tp1[] = {"y1"};
    const char* tc1[] = {"1"};
    std::vector<std::string> p1(tp1, tp1 + 1);
    std::vector<std::string> c1(tc1, tc1 + 1);
    tests.add(p1, c1);
  }

  AccurateTimer t;
  {
    FastMultipleTraitLinearRegressionScoreTest mt(1024);

    bool ret = mt.FitNullModel(Cov, Y, tests);
    if (ret == false) {
      printf("Fit null model failed!\n");
      exit(1);
    }

    ret = mt.AddGenotype(G);
    if (ret == false) {
      printf("Add covariate failed!\n");
      exit(1);
    }
    ret = mt.TestCovariateBlock();
    if (ret == false) {
      printf("Test covariate block failed!\n");
      exit(1);
    }
    const Matrix& u = mt.GetU();
    printf("u\t");
    PrintRow(u, 0);
    printf("\n");

    const Matrix& v = mt.GetV();
    printf("v\t");
    PrintRow(v, 0);
    printf("\n");

    const Matrix& pval = mt.GetPvalue();
    printf("pval\t");
    PrintRow(pval, 0);
    printf("\n");
  }
  return 0;
}
