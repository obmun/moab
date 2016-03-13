
#include <vector>
#include <assert.h>
#include "moab/Matrix3.hpp"
#include "moab/Core.hpp"
#include "moab/CartVect.hpp"
#include "TestUtil.hpp"

using namespace moab;

#define ACOS(x) acos(std::min(std::max(x,-1.0),1.0))

double find_angle(const moab::CartVect& A, const moab::CartVect& B)
{
  const double lA=A.length(), lB=B.length();
  assert(lA > 0.0);
  assert(lB > 0.0);
  const double dPI = 3.14159265;
  const double dot=(A[0]*B[0]+A[1]*B[1]+A[2]*B[2]);
  return ACOS( dot / ( lA * lB ) ) * 180.0 / dPI;
}

#define CHECK_EIGVECREAL_EQUAL( EXP, ACT, EPS ) check_equal_eigvect( (EXP), (ACT), (EPS), #EXP, #ACT, __LINE__, __FILE__ ) 
void check_equal_eigvect( const moab::CartVect& A,
                        const moab::CartVect& B, double eps,
                        const char* sA, const char* sB, 
                        int line, const char* file )
{
  check_equal( A.length(), B.length(), eps, sA, sB, line, file);

  double angle = find_angle(A, B);

  if (  (fabs(A[0] - B[0]) <= eps || fabs(A[0] + B[0]) <= eps) && 
        (fabs(A[1] - B[1]) <= eps || fabs(A[1] + B[1]) <= eps) &&
        (fabs(A[2] - B[2]) <= eps || fabs(A[2] + B[2]) <= eps) && 
        (angle <= eps || fabs(angle - 180.0) <= eps) )
    return;
  
  std::cout << "Equality Test Failed: " << sA << " == " << sB << std::endl;
  std::cout << "  at line " << line << " of '" << file << "'" << std::endl;
   
  std::cout << "  Expected: ";
  std::cout << A << std::endl;
  
  std::cout << "  Actual:   ";
  std::cout << B << std::endl;
  
  flag_error(); 
}


void test_EigenDecomp();

int main ()
{

  int result = 0;

  result += RUN_TEST(test_EigenDecomp);
  
  return result; 

}

// test to ensure the Eigenvalues/vectors are calculated correctly and returned properly
// from the Matrix3 class for a simple case
void test_EigenDecomp()
{
  //Create a matrix
  moab::Matrix3 mat;

  mat(0) = 2;
  mat(1) = -1;
  mat(2) = 0;
  mat(3) = -1;
  mat(4) = 2;
  mat(5) = -1;
  mat(6) = 0;
  mat(7) = -1;
  mat(8) = 2;

  //now do the Eigen Decomposition of this Matrix

  CartVect lamda; 
  Matrix3 vectors;
  moab::ErrorCode rval = mat.eigen_decomposition(lamda, vectors);CHECK_ERR(rval);
  for (int i=0; i < 3; ++i)
    vectors.col(i).normalize();

  //Hardcoded check values for the results
  double lamda_check[3];
  lamda_check[0] = 0.585786; lamda_check[1] = 2.0; lamda_check[2] = 3.41421;

  moab::CartVect vec0_check(0.5, 0.707107, 0.5);
  moab::CartVect vec1_check(0.707107, 3.37748e-17, -0.707107);
  moab::CartVect vec2_check(0.5, -0.707107, 0.5);

  //now verfy that the returns Eigenvalues and Eigenvectors are correct (within some tolerance)
  double tol = 1e-04;
  vec0_check.normalize();
  vec1_check.normalize();
  vec2_check.normalize();

  //check that the correct Eigenvalues are returned correctly (in order)
  CHECK_REAL_EQUAL( lamda[0], lamda_check[0], tol);
  CHECK_REAL_EQUAL( lamda[1], lamda_check[1], tol);
  CHECK_REAL_EQUAL( lamda[2], lamda_check[2], tol);

  //check the Eigenvector values (order should correspond to the Eigenvalues)
  //first vector
  CHECK_EIGVECREAL_EQUAL( vectors.col(0), vec0_check, tol );
  
  //sceond vector
  CHECK_EIGVECREAL_EQUAL( vectors.col(1), vec1_check, tol );

  //third vector
  CHECK_EIGVECREAL_EQUAL( vectors.col(2), vec2_check, tol );

  //another check to ensure the result is valid (AM-kM = 0)
  for(unsigned i=0; i<3; ++i) {
    moab::CartVect v = moab::Matrix::matrix_vector(mat, vectors.col(i))-lamda[i]*vectors.col(i);
    CHECK_REAL_EQUAL( v.length(), 0, tol );
  }

  //for a real, symmetric matrix the Eigenvectors should be orthogonal
  CHECK_REAL_EQUAL( vectors.col(0)%vectors.col(1), 0 , tol );
  CHECK_REAL_EQUAL( vectors.col(0)%vectors.col(2), 0 , tol );
  
  return;
}

