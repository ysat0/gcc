! { dg-require-effective-target fopenmp }
! { dg-options "-cpp -fopenmp" }
! { dg-do preprocess }

#ifndef _OPENMP
# error _OPENMP not defined
#endif

#if _OPENMP != 201107
# error _OPENMP defined to wrong value
#endif
