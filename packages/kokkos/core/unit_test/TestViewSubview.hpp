/*
//@HEADER
// ************************************************************************
// 
//                        Kokkos v. 2.0
//              Copyright (2014) Sandia Corporation
// 
// Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
// the U.S. Government retains certain rights in this software.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact  H. Carter Edwards (hcedwar@sandia.gov)
// 
// ************************************************************************
//@HEADER
*/

#include <gtest/gtest.h>

#include <Kokkos_Core.hpp>
#include <stdexcept>
#include <sstream>
#include <iostream>

/*--------------------------------------------------------------------------*/

namespace TestViewSubview {

template<class Layout, class Space>
struct getView {
  static
    Kokkos::View<double**,Layout,Space> get(int n, int m) {
      return Kokkos::View<double**,Layout,Space>("G",n,m);
  }
};

template<class Space>
struct getView<Kokkos::LayoutStride,Space> {
  static
    Kokkos::View<double**,Kokkos::LayoutStride,Space> get(int n, int m) {
      const int rank = 2 ;
      const int order[] = { 0, 1 };
      const unsigned dim[] = { unsigned(n), unsigned(m) };
      Kokkos::LayoutStride stride = Kokkos::LayoutStride::order_dimensions( rank , order , dim );
      return Kokkos::View<double**,Kokkos::LayoutStride,Space>("G",stride);
  }
};

template<class ViewType, class Space>
struct fill_1D {
  typedef typename Space::execution_space execution_space;
  typedef typename ViewType::size_type size_type;
  ViewType a;
  double val;
  fill_1D(ViewType a_, double val_):a(a_),val(val_) {
  }
  KOKKOS_INLINE_FUNCTION
  void operator() (const int i) const {
    a(i) = val;
  }
};

template<class ViewType, class Space>
struct fill_2D {
  typedef typename Space::execution_space execution_space;
  typedef typename ViewType::size_type size_type;
  ViewType a;
  double val;
  fill_2D(ViewType a_, double val_):a(a_),val(val_) {
  }
  KOKKOS_INLINE_FUNCTION
  void operator() (const int i) const{
    for(int j = 0; j < static_cast<int>(a.dimension_1()); j++)
      a(i,j) = val;
  }
};

template<class Layout, class Space>
void test_auto_1d ()
{
  typedef Kokkos::View<double**, Layout, Space> mv_type;
  typedef typename mv_type::size_type size_type;
  const double ZERO = 0.0;
  const double ONE = 1.0;
  const double TWO = 2.0;

  const size_type numRows = 10;
  const size_type numCols = 3;

  mv_type X = getView<Layout,Space>::get(numRows, numCols);
  typename mv_type::HostMirror X_h = Kokkos::create_mirror_view (X);

  fill_2D<mv_type,Space> f1(X, ONE);
  Kokkos::parallel_for(X.dimension_0(),f1);
  Kokkos::deep_copy (X_h, X);
  for (size_type j = 0; j < numCols; ++j) {
    for (size_type i = 0; i < numRows; ++i) {
      ASSERT_TRUE(X_h(i,j) == ONE);
    }
  }

  fill_2D<mv_type,Space> f2(X, 0.0);
  Kokkos::parallel_for(X.dimension_0(),f2);
  Kokkos::deep_copy (X_h, X);
  for (size_type j = 0; j < numCols; ++j) {
    for (size_type i = 0; i < numRows; ++i) {
      ASSERT_TRUE(X_h(i,j) == ZERO);
    }
  }

  fill_2D<mv_type,Space> f3(X, TWO);
  Kokkos::parallel_for(X.dimension_0(),f3);
  Kokkos::deep_copy (X_h, X);
  for (size_type j = 0; j < numCols; ++j) {
    for (size_type i = 0; i < numRows; ++i) {
      ASSERT_TRUE(X_h(i,j) == TWO);
    }
  }

  for (size_type j = 0; j < numCols; ++j) {
    auto X_j = Kokkos::subview (X, Kokkos::ALL(), j);

    fill_1D<decltype(X_j),Space> f4(X_j, ZERO);
    Kokkos::parallel_for(X_j.dimension_0(),f4);
    Kokkos::deep_copy (X_h, X);
    for (size_type i = 0; i < numRows; ++i) {
      ASSERT_TRUE(X_h(i,j) == ZERO);
    }

    for (size_type jj = 0; jj < numCols; ++jj) {
      auto X_jj = Kokkos::subview (X, Kokkos::ALL(), jj);
      fill_1D<decltype(X_jj),Space> f5(X_jj, ONE);
      Kokkos::parallel_for(X_jj.dimension_0(),f5);
      Kokkos::deep_copy (X_h, X);
      for (size_type i = 0; i < numRows; ++i) {
        ASSERT_TRUE(X_h(i,jj) == ONE);
      }
    }
  }
}

template<class LD, class LS, class Space>
void test_1d_strided_assignment_impl(bool a, bool b, bool c, bool d, int n, int m) {
  Kokkos::View<double**,LS,Space> l2d("l2d",n,m);

  int col = n>2?2:0;
  int row = m>2?2:0;

  if(Kokkos::Impl::VerifyExecutionCanAccessMemorySpace<Kokkos::HostSpace,Space>::value) {
  if(a) {
    Kokkos::View<double*,LD,Space> l1da = Kokkos::subview(l2d,Kokkos::ALL(),row);
    ASSERT_TRUE( & l1da(0) == & l2d(0,row) );
    if(n>1)
      ASSERT_TRUE( & l1da(1) == & l2d(1,row) );
  }
  if(b && n>13) {
    Kokkos::View<double*,LD,Space> l1db = Kokkos::subview(l2d,std::pair<unsigned,unsigned>(2,13),row);
    ASSERT_TRUE( & l1db(0) == & l2d(2,row) );
    ASSERT_TRUE( & l1db(1) == & l2d(3,row) );
  }
  if(c) {
    Kokkos::View<double*,LD,Space> l1dc = Kokkos::subview(l2d,col,Kokkos::ALL());
    ASSERT_TRUE( & l1dc(0) == & l2d(col,0) );
    if(m>1)
      ASSERT_TRUE( & l1dc(1) == & l2d(col,1) );
  }
  if(d && m>13) {
    Kokkos::View<double*,LD,Space> l1dd = Kokkos::subview(l2d,col,std::pair<unsigned,unsigned>(2,13));
    ASSERT_TRUE( & l1dd(0) == & l2d(col,2) );
    ASSERT_TRUE( & l1dd(1) == & l2d(col,3) );
  }
  }

}

template<class Space >
void test_1d_strided_assignment() {
  test_1d_strided_assignment_impl<Kokkos::LayoutStride,Kokkos::LayoutLeft,Space>(true,true,true,true,17,3);
  test_1d_strided_assignment_impl<Kokkos::LayoutStride,Kokkos::LayoutRight,Space>(true,true,true,true,17,3);

  test_1d_strided_assignment_impl<Kokkos::LayoutLeft,Kokkos::LayoutLeft,Space>(true,true,false,false,17,3);
  test_1d_strided_assignment_impl<Kokkos::LayoutRight,Kokkos::LayoutLeft,Space>(true,true,false,false,17,3);
  test_1d_strided_assignment_impl<Kokkos::LayoutLeft,Kokkos::LayoutRight,Space>(false,false,true,true,17,3);
  test_1d_strided_assignment_impl<Kokkos::LayoutRight,Kokkos::LayoutRight,Space>(false,false,true,true,17,3);

  test_1d_strided_assignment_impl<Kokkos::LayoutLeft,Kokkos::LayoutLeft,Space>(true,true,false,false,17,1);
  test_1d_strided_assignment_impl<Kokkos::LayoutLeft,Kokkos::LayoutLeft,Space>(true,true,true,true,1,17);
  test_1d_strided_assignment_impl<Kokkos::LayoutRight,Kokkos::LayoutLeft,Space>(true,true,true,true,1,17);
  test_1d_strided_assignment_impl<Kokkos::LayoutRight,Kokkos::LayoutLeft,Space>(true,true,false,false,17,1);

  test_1d_strided_assignment_impl<Kokkos::LayoutLeft,Kokkos::LayoutRight,Space>(true,true,true,true,17,1);
  test_1d_strided_assignment_impl<Kokkos::LayoutLeft,Kokkos::LayoutRight,Space>(false,false,true,true,1,17);
  test_1d_strided_assignment_impl<Kokkos::LayoutRight,Kokkos::LayoutRight,Space>(false,false,true,true,1,17);
  test_1d_strided_assignment_impl<Kokkos::LayoutRight,Kokkos::LayoutRight,Space>(true,true,true,true,17,1);
}

template< class Space >
void test_left_0()
{
  typedef Kokkos::View< int [2][3][4][5][2][3][4][5] , Kokkos::LayoutLeft , Space >
    view_static_8_type ;

  if(Kokkos::Impl::VerifyExecutionCanAccessMemorySpace<Kokkos::HostSpace,Space>::value) {

  view_static_8_type  x_static_8("x_static_left_8");

  ASSERT_TRUE( x_static_8.is_contiguous() );

  Kokkos::View<int,Kokkos::LayoutLeft,Space> x0 = Kokkos::subview( x_static_8 , 0, 0, 0, 0, 0, 0, 0, 0 );

  ASSERT_TRUE( x0.is_contiguous() );
  ASSERT_TRUE( & x0() == & x_static_8(0,0,0,0,0,0,0,0) );

  Kokkos::View<int*,Kokkos::LayoutLeft,Space> x1 =
    Kokkos::subview( x_static_8, Kokkos::pair<int,int>(0,2), 1, 2, 3, 0, 1, 2, 3 );

  ASSERT_TRUE( x1.is_contiguous() );
  ASSERT_TRUE( & x1(0) == & x_static_8(0,1,2,3,0,1,2,3) );
  ASSERT_TRUE( & x1(1) == & x_static_8(1,1,2,3,0,1,2,3) );

  Kokkos::View<int**,Kokkos::LayoutLeft,Space> x2 =
    Kokkos::subview( x_static_8, Kokkos::pair<int,int>(0,2), 1, 2, 3
                               , Kokkos::pair<int,int>(0,2), 1, 2, 3 );

  ASSERT_TRUE( ! x2.is_contiguous() );
  ASSERT_TRUE( & x2(0,0) == & x_static_8(0,1,2,3,0,1,2,3) );
  ASSERT_TRUE( & x2(1,0) == & x_static_8(1,1,2,3,0,1,2,3) );
  ASSERT_TRUE( & x2(0,1) == & x_static_8(0,1,2,3,1,1,2,3) );
  ASSERT_TRUE( & x2(1,1) == & x_static_8(1,1,2,3,1,1,2,3) );

  // Kokkos::View<int**,Kokkos::LayoutLeft,Space> error_2 =
  Kokkos::View<int**,Kokkos::LayoutStride,Space> sx2 =
    Kokkos::subview( x_static_8, 1, Kokkos::pair<int,int>(0,2), 2, 3
                               , Kokkos::pair<int,int>(0,2), 1, 2, 3 );

  ASSERT_TRUE( ! sx2.is_contiguous() );
  ASSERT_TRUE( & sx2(0,0) == & x_static_8(1,0,2,3,0,1,2,3) );
  ASSERT_TRUE( & sx2(1,0) == & x_static_8(1,1,2,3,0,1,2,3) );
  ASSERT_TRUE( & sx2(0,1) == & x_static_8(1,0,2,3,1,1,2,3) );
  ASSERT_TRUE( & sx2(1,1) == & x_static_8(1,1,2,3,1,1,2,3) );

  Kokkos::View<int****,Kokkos::LayoutStride,Space> sx4 =
    Kokkos::subview( x_static_8, 0, Kokkos::pair<int,int>(0,2) /* of [3] */
                               , 1, Kokkos::pair<int,int>(1,3) /* of [5] */
                               , 1, Kokkos::pair<int,int>(0,2) /* of [3] */
                               , 2, Kokkos::pair<int,int>(2,4) /* of [5] */
                   );

  ASSERT_TRUE( ! sx4.is_contiguous() );

  for ( int i0 = 0 ; i0 < (int) sx4.dimension_0() ; ++i0 )
  for ( int i1 = 0 ; i1 < (int) sx4.dimension_1() ; ++i1 )
  for ( int i2 = 0 ; i2 < (int) sx4.dimension_2() ; ++i2 )
  for ( int i3 = 0 ; i3 < (int) sx4.dimension_3() ; ++i3 ) {
    ASSERT_TRUE( & sx4(i0,i1,i2,i3) == & x_static_8(0,0+i0, 1,1+i1, 1,0+i2, 2,2+i3) );
  }

  }
}

template< class Space >
void test_left_1()
{
  typedef Kokkos::View< int ****[2][3][4][5] , Kokkos::LayoutLeft , Space >
    view_type ;

  if(Kokkos::Impl::VerifyExecutionCanAccessMemorySpace<Kokkos::HostSpace,Space>::value) {

  view_type  x8("x_left_8",2,3,4,5);

  ASSERT_TRUE( x8.is_contiguous() );

  Kokkos::View<int,Kokkos::LayoutLeft,Space> x0 = Kokkos::subview( x8 , 0, 0, 0, 0, 0, 0, 0, 0 );

  ASSERT_TRUE( x0.is_contiguous() );
  ASSERT_TRUE( & x0() == & x8(0,0,0,0,0,0,0,0) );

  Kokkos::View<int*,Kokkos::LayoutLeft,Space> x1 =
    Kokkos::subview( x8, Kokkos::pair<int,int>(0,2), 1, 2, 3, 0, 1, 2, 3 );

  ASSERT_TRUE( x1.is_contiguous() );
  ASSERT_TRUE( & x1(0) == & x8(0,1,2,3,0,1,2,3) );
  ASSERT_TRUE( & x1(1) == & x8(1,1,2,3,0,1,2,3) );

  Kokkos::View<int**,Kokkos::LayoutLeft,Space> x2 =
    Kokkos::subview( x8, Kokkos::pair<int,int>(0,2), 1, 2, 3
                               , Kokkos::pair<int,int>(0,2), 1, 2, 3 );

  ASSERT_TRUE( ! x2.is_contiguous() );
  ASSERT_TRUE( & x2(0,0) == & x8(0,1,2,3,0,1,2,3) );
  ASSERT_TRUE( & x2(1,0) == & x8(1,1,2,3,0,1,2,3) );
  ASSERT_TRUE( & x2(0,1) == & x8(0,1,2,3,1,1,2,3) );
  ASSERT_TRUE( & x2(1,1) == & x8(1,1,2,3,1,1,2,3) );

  // Kokkos::View<int**,Kokkos::LayoutLeft,Space> error_2 =
  Kokkos::View<int**,Kokkos::LayoutStride,Space> sx2 =
    Kokkos::subview( x8, 1, Kokkos::pair<int,int>(0,2), 2, 3
                               , Kokkos::pair<int,int>(0,2), 1, 2, 3 );

  ASSERT_TRUE( ! sx2.is_contiguous() );
  ASSERT_TRUE( & sx2(0,0) == & x8(1,0,2,3,0,1,2,3) );
  ASSERT_TRUE( & sx2(1,0) == & x8(1,1,2,3,0,1,2,3) );
  ASSERT_TRUE( & sx2(0,1) == & x8(1,0,2,3,1,1,2,3) );
  ASSERT_TRUE( & sx2(1,1) == & x8(1,1,2,3,1,1,2,3) );

  Kokkos::View<int****,Kokkos::LayoutStride,Space> sx4 =
    Kokkos::subview( x8, 0, Kokkos::pair<int,int>(0,2) /* of [3] */
                       , 1, Kokkos::pair<int,int>(1,3) /* of [5] */
                       , 1, Kokkos::pair<int,int>(0,2) /* of [3] */
                       , 2, Kokkos::pair<int,int>(2,4) /* of [5] */
                   );

  ASSERT_TRUE( ! sx4.is_contiguous() );

  for ( int i0 = 0 ; i0 < (int) sx4.dimension_0() ; ++i0 )
  for ( int i1 = 0 ; i1 < (int) sx4.dimension_1() ; ++i1 )
  for ( int i2 = 0 ; i2 < (int) sx4.dimension_2() ; ++i2 )
  for ( int i3 = 0 ; i3 < (int) sx4.dimension_3() ; ++i3 ) {
    ASSERT_TRUE( & sx4(i0,i1,i2,i3) == & x8(0,0+i0, 1,1+i1, 1,0+i2, 2,2+i3) );
  }

  }
}

template< class Space >
void test_left_2()
{
  typedef Kokkos::View< int **** , Kokkos::LayoutLeft , Space > view_type ;

  if(Kokkos::Impl::VerifyExecutionCanAccessMemorySpace<Kokkos::HostSpace,Space>::value) {

  view_type  x4("x4",2,3,4,5);

  ASSERT_TRUE( x4.is_contiguous() );

  Kokkos::View<int,Kokkos::LayoutLeft,Space> x0 = Kokkos::subview( x4 , 0, 0, 0, 0 );

  ASSERT_TRUE( x0.is_contiguous() );
  ASSERT_TRUE( & x0() == & x4(0,0,0,0) );

  Kokkos::View<int*,Kokkos::LayoutLeft,Space> x1 =
    Kokkos::subview( x4, Kokkos::pair<int,int>(0,2), 1, 2, 3 );

  ASSERT_TRUE( x1.is_contiguous() );
  ASSERT_TRUE( & x1(0) == & x4(0,1,2,3) );
  ASSERT_TRUE( & x1(1) == & x4(1,1,2,3) );

  Kokkos::View<int**,Kokkos::LayoutLeft,Space> x2 =
    Kokkos::subview( x4, Kokkos::pair<int,int>(0,2), 1, Kokkos::pair<int,int>(1,3), 2 );

  ASSERT_TRUE( ! x2.is_contiguous() );
  ASSERT_TRUE( & x2(0,0) == & x4(0,1,1,2) );
  ASSERT_TRUE( & x2(1,0) == & x4(1,1,1,2) );
  ASSERT_TRUE( & x2(0,1) == & x4(0,1,2,2) );
  ASSERT_TRUE( & x2(1,1) == & x4(1,1,2,2) );

  // Kokkos::View<int**,Kokkos::LayoutLeft,Space> error_2 =
  Kokkos::View<int**,Kokkos::LayoutStride,Space> sx2 =
    Kokkos::subview( x4, 1, Kokkos::pair<int,int>(0,2)
                       , 2, Kokkos::pair<int,int>(1,4) );

  ASSERT_TRUE( ! sx2.is_contiguous() );
  ASSERT_TRUE( & sx2(0,0) == & x4(1,0,2,1) );
  ASSERT_TRUE( & sx2(1,0) == & x4(1,1,2,1) );
  ASSERT_TRUE( & sx2(0,1) == & x4(1,0,2,2) );
  ASSERT_TRUE( & sx2(1,1) == & x4(1,1,2,2) );
  ASSERT_TRUE( & sx2(0,2) == & x4(1,0,2,3) );
  ASSERT_TRUE( & sx2(1,2) == & x4(1,1,2,3) );

  Kokkos::View<int****,Kokkos::LayoutStride,Space> sx4 =
    Kokkos::subview( x4, Kokkos::pair<int,int>(1,2) /* of [2] */
                       , Kokkos::pair<int,int>(1,3) /* of [3] */
                       , Kokkos::pair<int,int>(0,4) /* of [4] */
                       , Kokkos::pair<int,int>(2,4) /* of [5] */
                   );

  ASSERT_TRUE( ! sx4.is_contiguous() );

  for ( int i0 = 0 ; i0 < (int) sx4.dimension_0() ; ++i0 )
  for ( int i1 = 0 ; i1 < (int) sx4.dimension_1() ; ++i1 )
  for ( int i2 = 0 ; i2 < (int) sx4.dimension_2() ; ++i2 )
  for ( int i3 = 0 ; i3 < (int) sx4.dimension_3() ; ++i3 ) {
    ASSERT_TRUE( & sx4(i0,i1,i2,i3) == & x4( 1+i0, 1+i1, 0+i2, 2+i3 ) );
  }

  }
}

template< class Space >
void test_left_3()
{
  typedef Kokkos::View< int ** , Kokkos::LayoutLeft , Space > view_type ;

  if(Kokkos::Impl::VerifyExecutionCanAccessMemorySpace<Kokkos::HostSpace,Space>::value) {

  view_type  xm("x4",10,5);

  ASSERT_TRUE( xm.is_contiguous() );

  Kokkos::View<int,Kokkos::LayoutLeft,Space> x0 = Kokkos::subview( xm , 5, 3 );

  ASSERT_TRUE( x0.is_contiguous() );
  ASSERT_TRUE( & x0() == & xm(5,3) );

  Kokkos::View<int*,Kokkos::LayoutLeft,Space> x1 =
    Kokkos::subview( xm, Kokkos::ALL(), 3 );

  ASSERT_TRUE( x1.is_contiguous() );
  for ( int i = 0 ; i < int(xm.dimension_0()) ; ++i ) {
    ASSERT_TRUE( & x1(i) == & xm(i,3) );
  }

  Kokkos::View<int**,Kokkos::LayoutLeft,Space> x2 =
    Kokkos::subview( xm, Kokkos::pair<int,int>(1,9), Kokkos::ALL() );

  ASSERT_TRUE( ! x2.is_contiguous() );
  for ( int j = 0 ; j < int(x2.dimension_1()) ; ++j )
  for ( int i = 0 ; i < int(x2.dimension_0()) ; ++i ) {
    ASSERT_TRUE( & x2(i,j) == & xm(1+i,j) );
  }

  Kokkos::View<int**,Kokkos::LayoutLeft,Space> x2c =
    Kokkos::subview( xm, Kokkos::ALL(), std::pair<int,int>(2,4) );

  ASSERT_TRUE( x2c.is_contiguous() );
  for ( int j = 0 ; j < int(x2c.dimension_1()) ; ++j )
  for ( int i = 0 ; i < int(x2c.dimension_0()) ; ++i ) {
    ASSERT_TRUE( & x2c(i,j) == & xm(i,2+j) );
  }

  Kokkos::View<int**,Kokkos::LayoutLeft,Space> x2_n1 =
    Kokkos::subview( xm , std::pair<int,int>(1,1) , Kokkos::ALL() );

  ASSERT_TRUE( x2_n1.dimension_0() == 0 );
  ASSERT_TRUE( x2_n1.dimension_1() == xm.dimension_1() );

  Kokkos::View<int**,Kokkos::LayoutLeft,Space> x2_n2 =
    Kokkos::subview( xm , Kokkos::ALL() , std::pair<int,int>(1,1) );

  ASSERT_TRUE( x2_n2.dimension_0() == xm.dimension_0() );
  ASSERT_TRUE( x2_n2.dimension_1() == 0 );

  }
}

//----------------------------------------------------------------------------

template< class Space >
void test_right_0()
{
  typedef Kokkos::View< int [2][3][4][5][2][3][4][5] , Kokkos::LayoutRight , Space >
    view_static_8_type ;

  if(Kokkos::Impl::VerifyExecutionCanAccessMemorySpace<Kokkos::HostSpace,Space>::value) {

  view_static_8_type  x_static_8("x_static_right_8");

  Kokkos::View<int,Kokkos::LayoutRight,Space> x0 = Kokkos::subview( x_static_8 , 0, 0, 0, 0, 0, 0, 0, 0 );

  ASSERT_TRUE( & x0() == & x_static_8(0,0,0,0,0,0,0,0) );

  Kokkos::View<int*,Kokkos::LayoutRight,Space> x1 =
    Kokkos::subview( x_static_8, 0, 1, 2, 3, 0, 1, 2, Kokkos::pair<int,int>(1,3) );

  ASSERT_TRUE( x1.dimension_0() == 2 );
  ASSERT_TRUE( & x1(0) == & x_static_8(0,1,2,3,0,1,2,1) );
  ASSERT_TRUE( & x1(1) == & x_static_8(0,1,2,3,0,1,2,2) );

  Kokkos::View<int**,Kokkos::LayoutRight,Space> x2 =
    Kokkos::subview( x_static_8, 0, 1, 2, Kokkos::pair<int,int>(1,3)
                               , 0, 1, 2, Kokkos::pair<int,int>(1,3) );

  ASSERT_TRUE( x2.dimension_0() == 2 );
  ASSERT_TRUE( x2.dimension_1() == 2 );
  ASSERT_TRUE( & x2(0,0) == & x_static_8(0,1,2,1,0,1,2,1) );
  ASSERT_TRUE( & x2(1,0) == & x_static_8(0,1,2,2,0,1,2,1) );
  ASSERT_TRUE( & x2(0,1) == & x_static_8(0,1,2,1,0,1,2,2) );
  ASSERT_TRUE( & x2(1,1) == & x_static_8(0,1,2,2,0,1,2,2) );

  // Kokkos::View<int**,Kokkos::LayoutRight,Space> error_2 =
  Kokkos::View<int**,Kokkos::LayoutStride,Space> sx2 =
    Kokkos::subview( x_static_8, 1, Kokkos::pair<int,int>(0,2), 2, 3
                               , Kokkos::pair<int,int>(0,2), 1, 2, 3 );

  ASSERT_TRUE( sx2.dimension_0() == 2 );
  ASSERT_TRUE( sx2.dimension_1() == 2 );
  ASSERT_TRUE( & sx2(0,0) == & x_static_8(1,0,2,3,0,1,2,3) );
  ASSERT_TRUE( & sx2(1,0) == & x_static_8(1,1,2,3,0,1,2,3) );
  ASSERT_TRUE( & sx2(0,1) == & x_static_8(1,0,2,3,1,1,2,3) );
  ASSERT_TRUE( & sx2(1,1) == & x_static_8(1,1,2,3,1,1,2,3) );

  Kokkos::View<int****,Kokkos::LayoutStride,Space> sx4 =
    Kokkos::subview( x_static_8, 0, Kokkos::pair<int,int>(0,2) /* of [3] */
                               , 1, Kokkos::pair<int,int>(1,3) /* of [5] */
                               , 1, Kokkos::pair<int,int>(0,2) /* of [3] */
                               , 2, Kokkos::pair<int,int>(2,4) /* of [5] */
                   );

  ASSERT_TRUE( sx4.dimension_0() == 2 );
  ASSERT_TRUE( sx4.dimension_1() == 2 );
  ASSERT_TRUE( sx4.dimension_2() == 2 );
  ASSERT_TRUE( sx4.dimension_3() == 2 );
  for ( int i0 = 0 ; i0 < (int) sx4.dimension_0() ; ++i0 )
  for ( int i1 = 0 ; i1 < (int) sx4.dimension_1() ; ++i1 )
  for ( int i2 = 0 ; i2 < (int) sx4.dimension_2() ; ++i2 )
  for ( int i3 = 0 ; i3 < (int) sx4.dimension_3() ; ++i3 ) {
    ASSERT_TRUE( & sx4(i0,i1,i2,i3) == & x_static_8(0, 0+i0, 1, 1+i1, 1, 0+i2, 2, 2+i3) );
  }

  }
}

template< class Space >
void test_right_1()
{
  typedef Kokkos::View< int ****[2][3][4][5] , Kokkos::LayoutRight , Space >
    view_type ;

  if(Kokkos::Impl::VerifyExecutionCanAccessMemorySpace<Kokkos::HostSpace,Space>::value) {

  view_type  x8("x_right_8",2,3,4,5);

  Kokkos::View<int,Kokkos::LayoutRight,Space> x0 = Kokkos::subview( x8 , 0, 0, 0, 0, 0, 0, 0, 0 );

  ASSERT_TRUE( & x0() == & x8(0,0,0,0,0,0,0,0) );

  Kokkos::View<int*,Kokkos::LayoutRight,Space> x1 =
    Kokkos::subview( x8, 0, 1, 2, 3, 0, 1, 2, Kokkos::pair<int,int>(1,3) );

  ASSERT_TRUE( & x1(0) == & x8(0,1,2,3,0,1,2,1) );
  ASSERT_TRUE( & x1(1) == & x8(0,1,2,3,0,1,2,2) );

  Kokkos::View<int**,Kokkos::LayoutRight,Space> x2 =
    Kokkos::subview( x8, 0, 1, 2, Kokkos::pair<int,int>(1,3)
                               , 0, 1, 2, Kokkos::pair<int,int>(1,3) );

  ASSERT_TRUE( & x2(0,0) == & x8(0,1,2,1,0,1,2,1) );
  ASSERT_TRUE( & x2(1,0) == & x8(0,1,2,2,0,1,2,1) );
  ASSERT_TRUE( & x2(0,1) == & x8(0,1,2,1,0,1,2,2) );
  ASSERT_TRUE( & x2(1,1) == & x8(0,1,2,2,0,1,2,2) );

  // Kokkos::View<int**,Kokkos::LayoutRight,Space> error_2 =
  Kokkos::View<int**,Kokkos::LayoutStride,Space> sx2 =
    Kokkos::subview( x8, 1, Kokkos::pair<int,int>(0,2), 2, 3
                               , Kokkos::pair<int,int>(0,2), 1, 2, 3 );

  ASSERT_TRUE( & sx2(0,0) == & x8(1,0,2,3,0,1,2,3) );
  ASSERT_TRUE( & sx2(1,0) == & x8(1,1,2,3,0,1,2,3) );
  ASSERT_TRUE( & sx2(0,1) == & x8(1,0,2,3,1,1,2,3) );
  ASSERT_TRUE( & sx2(1,1) == & x8(1,1,2,3,1,1,2,3) );

  Kokkos::View<int****,Kokkos::LayoutStride,Space> sx4 =
    Kokkos::subview( x8, 0, Kokkos::pair<int,int>(0,2) /* of [3] */
                       , 1, Kokkos::pair<int,int>(1,3) /* of [5] */
                       , 1, Kokkos::pair<int,int>(0,2) /* of [3] */
                       , 2, Kokkos::pair<int,int>(2,4) /* of [5] */
                   );

  for ( int i0 = 0 ; i0 < (int) sx4.dimension_0() ; ++i0 )
  for ( int i1 = 0 ; i1 < (int) sx4.dimension_1() ; ++i1 )
  for ( int i2 = 0 ; i2 < (int) sx4.dimension_2() ; ++i2 )
  for ( int i3 = 0 ; i3 < (int) sx4.dimension_3() ; ++i3 ) {
    ASSERT_TRUE( & sx4(i0,i1,i2,i3) == & x8(0,0+i0, 1,1+i1, 1,0+i2, 2,2+i3) );
  }

  }
}

template< class Space >
void test_right_3()
{
  typedef Kokkos::View< int ** , Kokkos::LayoutRight , Space > view_type ;

  if(Kokkos::Impl::VerifyExecutionCanAccessMemorySpace<Kokkos::HostSpace,Space>::value) {

  view_type  xm("x4",10,5);

  ASSERT_TRUE( xm.is_contiguous() );

  Kokkos::View<int,Kokkos::LayoutRight,Space> x0 = Kokkos::subview( xm , 5, 3 );

  ASSERT_TRUE( x0.is_contiguous() );
  ASSERT_TRUE( & x0() == & xm(5,3) );

  Kokkos::View<int*,Kokkos::LayoutRight,Space> x1 =
    Kokkos::subview( xm, 3, Kokkos::ALL() );

  ASSERT_TRUE( x1.is_contiguous() );
  for ( int i = 0 ; i < int(xm.dimension_1()) ; ++i ) {
    ASSERT_TRUE( & x1(i) == & xm(3,i) );
  }

  Kokkos::View<int**,Kokkos::LayoutRight,Space> x2c =
    Kokkos::subview( xm, Kokkos::pair<int,int>(1,9), Kokkos::ALL() );

  ASSERT_TRUE( x2c.is_contiguous() );
  for ( int j = 0 ; j < int(x2c.dimension_1()) ; ++j )
  for ( int i = 0 ; i < int(x2c.dimension_0()) ; ++i ) {
    ASSERT_TRUE( & x2c(i,j) == & xm(1+i,j) );
  }

  Kokkos::View<int**,Kokkos::LayoutRight,Space> x2 =
    Kokkos::subview( xm, Kokkos::ALL(), std::pair<int,int>(2,4) );

  ASSERT_TRUE( ! x2.is_contiguous() );
  for ( int j = 0 ; j < int(x2.dimension_1()) ; ++j )
  for ( int i = 0 ; i < int(x2.dimension_0()) ; ++i ) {
    ASSERT_TRUE( & x2(i,j) == & xm(i,2+j) );
  }

  Kokkos::View<int**,Kokkos::LayoutRight,Space> x2_n1 =
    Kokkos::subview( xm , std::pair<int,int>(1,1) , Kokkos::ALL() );

  ASSERT_TRUE( x2_n1.dimension_0() == 0 );
  ASSERT_TRUE( x2_n1.dimension_1() == xm.dimension_1() );

  Kokkos::View<int**,Kokkos::LayoutRight,Space> x2_n2 =
    Kokkos::subview( xm , Kokkos::ALL() , std::pair<int,int>(1,1) );

  ASSERT_TRUE( x2_n2.dimension_0() == xm.dimension_0() );
  ASSERT_TRUE( x2_n2.dimension_1() == 0 );

  }
}

namespace Impl {

constexpr int N0=113;
constexpr int N1=11;
constexpr int N2=17;
constexpr int N3=5;
constexpr int N4=7;

template<class SubView,class View>
void test_Check1D(SubView a, View b, std::pair<int,int> range) {
  int errors = 0;
  for(int i=0;i<range.second-range.first;i++) {
    if(a(i)!=b(i+range.first))
      errors++;
  }
  if(errors>0)
    std::cout << "Error Suviews test_Check1D: " << errors <<std::endl;
  ASSERT_TRUE( errors == 0 );
}

template<class SubView,class View>
void test_Check1D2D(SubView a, View b, int i0, std::pair<int,int> range) {
  int errors = 0;
  for(int i1=0;i1<range.second-range.first;i1++) {
    if(a(i1)!=b(i0,i1+range.first))
      errors++;
  }
  if(errors>0)
    std::cout << "Error Suviews test_Check1D2D: " << errors <<std::endl;
  ASSERT_TRUE( errors == 0 );
}

template<class SubView,class View>
void test_Check2D3D(SubView a, View b, int i0, std::pair<int,int> range1, std::pair<int,int> range2) {
  int errors = 0;
  for(int i1=0;i1<range1.second-range1.first;i1++) {
    for(int i2=0;i2<range2.second-range2.first;i2++) {
      if(a(i1,i2)!=b(i0,i1+range1.first,i2+range2.first))
        errors++;
    }
  }
  if(errors>0)
    std::cout << "Error Suviews test_Check2D3D: " << errors <<std::endl;
  ASSERT_TRUE( errors == 0 );
}

template<class SubView,class View>
void test_Check3D5D(SubView a, View b, int i0, int i1, std::pair<int,int> range2, std::pair<int,int> range3, std::pair<int,int> range4) {
  int errors = 0;
  for(int i2=0;i2<range2.second-range2.first;i2++) {
    for(int i3=0;i3<range3.second-range3.first;i3++) {
      for(int i4=0;i4<range4.second-range4.first;i4++) {
        if(a(i2,i3,i4)!=b(i0,i1,i2+range2.first,i3+range3.first,i4+range4.first))
          errors++;
      }
    }
  }
  if(errors>0)
    std::cout << "Error Suviews test_Check3D5D: " << errors <<std::endl;
  ASSERT_TRUE( errors == 0 );
}

template<class Space, class LayoutSub, class Layout, class LayoutOrg, class MemTraits>
void test_1d_assign_impl() {

  { //Breaks
    Kokkos::View<int*,LayoutOrg,Space> a_org("A",N0);
    Kokkos::View<int*,LayoutOrg,Space,MemTraits> a(a_org);
    Kokkos::fence();
    for(int i=0; i<N0; i++)
      a_org(i) = i;

    Kokkos::View<int[N0],Layout,Space,MemTraits> a1(a);
    Kokkos::fence();
    test_Check1D(a1,a,std::pair<int,int>(0,N0));

    Kokkos::View<int[N0],LayoutSub,Space,MemTraits> a2(a1);
    Kokkos::fence();
    test_Check1D(a2,a,std::pair<int,int>(0,N0));
    a1 = a;
    test_Check1D(a1,a,std::pair<int,int>(0,N0));

    //Runtime Fail expected
    //Kokkos::View<int[N1]> afail1(a);

    //Compile Time Fail expected
    //Kokkos::View<int[N1]> afail2(a1);
  }

  { // Works
    Kokkos::View<int[N0],LayoutOrg,Space,MemTraits> a("A");
    Kokkos::View<int*,Layout,Space,MemTraits> a1(a);
    Kokkos::fence();
    test_Check1D(a1,a,std::pair<int,int>(0,N0));
    a1 = a;
    Kokkos::fence();
    test_Check1D(a1,a,std::pair<int,int>(0,N0));
  }
}

template<class Space, class Type, class TypeSub,class LayoutSub, class Layout, class LayoutOrg,class MemTraits>
void test_2d_subview_3d_impl_type() {
  Kokkos::View<int***,LayoutOrg,Space> a_org("A",N0,N1,N2);
  Kokkos::View<Type,Layout,Space,MemTraits> a(a_org);
  for(int i0=0; i0<N0; i0++)
    for(int i1=0; i1<N1; i1++)
      for(int i2=0; i2<N2; i2++)
        a_org(i0,i1,i2) = i0*1000000+i1*1000+i2;
  Kokkos::View<TypeSub,LayoutSub,Space,MemTraits> a1;
  a1 = Kokkos::subview(a,3,Kokkos::ALL(),Kokkos::ALL());
  Kokkos::fence();
  test_Check2D3D(a1,a,3,std::pair<int,int>(0,N1),std::pair<int,int>(0,N2));

  Kokkos::View<TypeSub,LayoutSub,Space,MemTraits> a2(a,3,Kokkos::ALL(),Kokkos::ALL());
  Kokkos::fence();
  test_Check2D3D(a2,a,3,std::pair<int,int>(0,N1),std::pair<int,int>(0,N2));
}

template<class Space, class LayoutSub, class Layout, class LayoutOrg, class MemTraits>
void test_2d_subview_3d_impl_layout() {
  test_2d_subview_3d_impl_type<Space,int[N0][N1][N2],int[N1][N2],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_2d_subview_3d_impl_type<Space,int[N0][N1][N2],int*   [N2],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_2d_subview_3d_impl_type<Space,int[N0][N1][N2],int**      ,LayoutSub, Layout, LayoutOrg, MemTraits>();

  test_2d_subview_3d_impl_type<Space,int*   [N1][N2],int[N1][N2],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_2d_subview_3d_impl_type<Space,int*   [N1][N2],int*   [N2],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_2d_subview_3d_impl_type<Space,int*   [N1][N2],int**      ,LayoutSub, Layout, LayoutOrg, MemTraits>();

  test_2d_subview_3d_impl_type<Space,int**      [N2],int[N1][N2],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_2d_subview_3d_impl_type<Space,int**      [N2],int*   [N2],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_2d_subview_3d_impl_type<Space,int**      [N2],int**      ,LayoutSub, Layout, LayoutOrg, MemTraits>();

  test_2d_subview_3d_impl_type<Space,int***         ,int[N1][N2],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_2d_subview_3d_impl_type<Space,int***         ,int*   [N2],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_2d_subview_3d_impl_type<Space,int***         ,int**      ,LayoutSub, Layout, LayoutOrg, MemTraits>();

  test_2d_subview_3d_impl_type<Space,const int[N0][N1][N2],const int[N1][N2],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_2d_subview_3d_impl_type<Space,const int[N0][N1][N2],const int*   [N2],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_2d_subview_3d_impl_type<Space,const int[N0][N1][N2],const int**      ,LayoutSub, Layout, LayoutOrg, MemTraits>();

  test_2d_subview_3d_impl_type<Space,const int*   [N1][N2],const int[N1][N2],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_2d_subview_3d_impl_type<Space,const int*   [N1][N2],const int*   [N2],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_2d_subview_3d_impl_type<Space,const int*   [N1][N2],const int**      ,LayoutSub, Layout, LayoutOrg, MemTraits>();

  test_2d_subview_3d_impl_type<Space,const int**      [N2],const int[N1][N2],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_2d_subview_3d_impl_type<Space,const int**      [N2],const int*   [N2],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_2d_subview_3d_impl_type<Space,const int**      [N2],const int**      ,LayoutSub, Layout, LayoutOrg, MemTraits>();

  test_2d_subview_3d_impl_type<Space,const int***         ,const int[N1][N2],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_2d_subview_3d_impl_type<Space,const int***         ,const int*   [N2],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_2d_subview_3d_impl_type<Space,const int***         ,const int**      ,LayoutSub, Layout, LayoutOrg, MemTraits>();
}

template<class Space, class Type, class TypeSub,class LayoutSub, class Layout, class LayoutOrg, class MemTraits>
void test_3d_subview_5d_impl_type() {
  Kokkos::View<int*****,LayoutOrg,Space> a_org("A",N0,N1,N2,N3,N4);
  Kokkos::View<Type,Layout,Space,MemTraits> a(a_org);
  for(int i0=0; i0<N0; i0++)
    for(int i1=0; i1<N1; i1++)
      for(int i2=0; i2<N2; i2++)
        for(int i3=0; i3<N3; i3++)
          for(int i4=0; i4<N4; i4++)
            a_org(i0,i1,i2,i3,i4) = i0*1000000+i1*10000+i2*100+i3*10+i4;
  Kokkos::View<TypeSub,LayoutSub,Space,MemTraits> a1;
  a1 = Kokkos::subview(a,3,5,Kokkos::ALL(),Kokkos::ALL(),Kokkos::ALL());
  Kokkos::fence();
  test_Check3D5D(a1,a,3,5,std::pair<int,int>(0,N2),std::pair<int,int>(0,N3),std::pair<int,int>(0,N4));

  Kokkos::View<TypeSub,LayoutSub,Space,MemTraits> a2(a,3,5,Kokkos::ALL(),Kokkos::ALL(),Kokkos::ALL());
  Kokkos::fence();
  test_Check3D5D(a2,a,3,5,std::pair<int,int>(0,N2),std::pair<int,int>(0,N3),std::pair<int,int>(0,N4));
}

template<class Space, class LayoutSub, class Layout, class LayoutOrg, class MemTraits>
void test_3d_subview_5d_impl_layout() {
  test_3d_subview_5d_impl_type<Space, int[N0][N1][N2][N3][N4],int[N2][N3][N4],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_3d_subview_5d_impl_type<Space, int[N0][N1][N2][N3][N4],int*   [N3][N4],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_3d_subview_5d_impl_type<Space, int[N0][N1][N2][N3][N4],int**      [N4],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_3d_subview_5d_impl_type<Space, int[N0][N1][N2][N3][N4],int***         ,LayoutSub, Layout, LayoutOrg, MemTraits>();

  test_3d_subview_5d_impl_type<Space, int*   [N1][N2][N3][N4],int[N2][N3][N4],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_3d_subview_5d_impl_type<Space, int*   [N1][N2][N3][N4],int*   [N3][N4],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_3d_subview_5d_impl_type<Space, int*   [N1][N2][N3][N4],int**      [N4],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_3d_subview_5d_impl_type<Space, int*   [N1][N2][N3][N4],int***         ,LayoutSub, Layout, LayoutOrg, MemTraits>();

  test_3d_subview_5d_impl_type<Space, int**      [N2][N3][N4],int[N2][N3][N4],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_3d_subview_5d_impl_type<Space, int**      [N2][N3][N4],int*   [N3][N4],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_3d_subview_5d_impl_type<Space, int**      [N2][N3][N4],int**      [N4],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_3d_subview_5d_impl_type<Space, int**      [N2][N3][N4],int***         ,LayoutSub, Layout, LayoutOrg, MemTraits>();

  test_3d_subview_5d_impl_type<Space, int***         [N3][N4],int[N2][N3][N4],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_3d_subview_5d_impl_type<Space, int***         [N3][N4],int*   [N3][N4],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_3d_subview_5d_impl_type<Space, int***         [N3][N4],int**      [N4],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_3d_subview_5d_impl_type<Space, int***         [N3][N4],int***         ,LayoutSub, Layout, LayoutOrg, MemTraits>();

  test_3d_subview_5d_impl_type<Space, int****            [N4],int[N2][N3][N4],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_3d_subview_5d_impl_type<Space, int****            [N4],int*   [N3][N4],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_3d_subview_5d_impl_type<Space, int****            [N4],int**      [N4],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_3d_subview_5d_impl_type<Space, int****            [N4],int***         ,LayoutSub, Layout, LayoutOrg, MemTraits>();

  test_3d_subview_5d_impl_type<Space, int*****               ,int[N2][N3][N4],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_3d_subview_5d_impl_type<Space, int*****               ,int*   [N3][N4],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_3d_subview_5d_impl_type<Space, int*****               ,int**      [N4],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_3d_subview_5d_impl_type<Space, int*****               ,int***         ,LayoutSub, Layout, LayoutOrg, MemTraits>();

  test_3d_subview_5d_impl_type<Space, const int[N0][N1][N2][N3][N4],const int[N2][N3][N4],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_3d_subview_5d_impl_type<Space, const int[N0][N1][N2][N3][N4],const int*   [N3][N4],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_3d_subview_5d_impl_type<Space, const int[N0][N1][N2][N3][N4],const int**      [N4],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_3d_subview_5d_impl_type<Space, const int[N0][N1][N2][N3][N4],const int***         ,LayoutSub, Layout, LayoutOrg, MemTraits>();

  test_3d_subview_5d_impl_type<Space, const int*   [N1][N2][N3][N4],const int[N2][N3][N4],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_3d_subview_5d_impl_type<Space, const int*   [N1][N2][N3][N4],const int*   [N3][N4],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_3d_subview_5d_impl_type<Space, const int*   [N1][N2][N3][N4],const int**      [N4],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_3d_subview_5d_impl_type<Space, const int*   [N1][N2][N3][N4],const int***         ,LayoutSub, Layout, LayoutOrg, MemTraits>();

  test_3d_subview_5d_impl_type<Space, const int**      [N2][N3][N4],const int[N2][N3][N4],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_3d_subview_5d_impl_type<Space, const int**      [N2][N3][N4],const int*   [N3][N4],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_3d_subview_5d_impl_type<Space, const int**      [N2][N3][N4],const int**      [N4],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_3d_subview_5d_impl_type<Space, const int**      [N2][N3][N4],const int***         ,LayoutSub, Layout, LayoutOrg, MemTraits>();

  test_3d_subview_5d_impl_type<Space, const int***         [N3][N4],const int[N2][N3][N4],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_3d_subview_5d_impl_type<Space, const int***         [N3][N4],const int*   [N3][N4],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_3d_subview_5d_impl_type<Space, const int***         [N3][N4],const int**      [N4],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_3d_subview_5d_impl_type<Space, const int***         [N3][N4],const int***         ,LayoutSub, Layout, LayoutOrg, MemTraits>();

  test_3d_subview_5d_impl_type<Space, const int****            [N4],const int[N2][N3][N4],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_3d_subview_5d_impl_type<Space, const int****            [N4],const int*   [N3][N4],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_3d_subview_5d_impl_type<Space, const int****            [N4],const int**      [N4],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_3d_subview_5d_impl_type<Space, const int****            [N4],const int***         ,LayoutSub, Layout, LayoutOrg, MemTraits>();

  test_3d_subview_5d_impl_type<Space, const int*****               ,const int[N2][N3][N4],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_3d_subview_5d_impl_type<Space, const int*****               ,const int*   [N3][N4],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_3d_subview_5d_impl_type<Space, const int*****               ,const int**      [N4],LayoutSub, Layout, LayoutOrg, MemTraits>();
  test_3d_subview_5d_impl_type<Space, const int*****               ,const int***         ,LayoutSub, Layout, LayoutOrg, MemTraits>();
}
}

template< class Space, class MemTraits = void>
void test_1d_assign() {
  Impl::test_1d_assign_impl<Space,Kokkos::LayoutLeft  ,Kokkos::LayoutLeft  ,Kokkos::LayoutLeft, MemTraits>();
  //Impl::test_1d_assign_impl<Space,Kokkos::LayoutRight ,Kokkos::LayoutLeft  ,Kokkos::LayoutLeft  >();
  Impl::test_1d_assign_impl<Space,Kokkos::LayoutStride,Kokkos::LayoutLeft  ,Kokkos::LayoutLeft, MemTraits>();
  //Impl::test_1d_assign_impl<Space,Kokkos::LayoutLeft  ,Kokkos::LayoutRight ,Kokkos::LayoutLeft  >();
  Impl::test_1d_assign_impl<Space,Kokkos::LayoutRight ,Kokkos::LayoutRight ,Kokkos::LayoutRight, MemTraits>();
  Impl::test_1d_assign_impl<Space,Kokkos::LayoutStride,Kokkos::LayoutRight ,Kokkos::LayoutRight, MemTraits>();
  //Impl::test_1d_assign_impl<Space,Kokkos::LayoutLeft  ,Kokkos::LayoutStride,Kokkos::LayoutLeft  >();
  //Impl::test_1d_assign_impl<Space,Kokkos::LayoutRight ,Kokkos::LayoutStride,Kokkos::LayoutLeft  >();
  Impl::test_1d_assign_impl<Space,Kokkos::LayoutStride,Kokkos::LayoutStride,Kokkos::LayoutLeft, MemTraits>();
}

template<class Space, class MemTraits = void>
void test_2d_subview_3d() {
  Impl::test_2d_subview_3d_impl_layout<Space,Kokkos::LayoutRight ,Kokkos::LayoutRight, Kokkos::LayoutRight, MemTraits>();
  Impl::test_2d_subview_3d_impl_layout<Space,Kokkos::LayoutStride,Kokkos::LayoutRight, Kokkos::LayoutRight, MemTraits>();
  Impl::test_2d_subview_3d_impl_layout<Space,Kokkos::LayoutStride,Kokkos::LayoutStride,Kokkos::LayoutRight, MemTraits>();
  Impl::test_2d_subview_3d_impl_layout<Space,Kokkos::LayoutStride,Kokkos::LayoutLeft,  Kokkos::LayoutLeft,  MemTraits>();
  Impl::test_2d_subview_3d_impl_layout<Space,Kokkos::LayoutStride,Kokkos::LayoutStride,Kokkos::LayoutLeft,  MemTraits>();
}

template<class Space, class MemTraits = void>
void test_3d_subview_5d() {
  Impl::test_3d_subview_5d_impl_layout<Space,Kokkos::LayoutStride,Kokkos::LayoutRight, Kokkos::LayoutRight, MemTraits>();
  Impl::test_3d_subview_5d_impl_layout<Space,Kokkos::LayoutStride,Kokkos::LayoutStride,Kokkos::LayoutRight, MemTraits>();
  Impl::test_3d_subview_5d_impl_layout<Space,Kokkos::LayoutStride,Kokkos::LayoutLeft,  Kokkos::LayoutLeft,  MemTraits>();
  Impl::test_3d_subview_5d_impl_layout<Space,Kokkos::LayoutStride,Kokkos::LayoutStride,Kokkos::LayoutLeft,  MemTraits>();
}

}
//----------------------------------------------------------------------------

