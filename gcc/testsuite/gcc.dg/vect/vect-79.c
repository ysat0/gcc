/* { dg-do run { target powerpc*-*-* } } */
/* { dg-do run { target i?86-*-* x86_64-*-* } } */
/* { dg-options "-O2 -ftree-vectorize -fdump-tree-vect-stats -maltivec" { target powerpc*-*-* } } */
/* { dg-options "-O2 -ftree-vectorize -fdump-tree-vect-stats -msse2" { target i?86-*-* x86_64-*-* } } */

#include <stdarg.h>
#include "tree-vect.h"

#define N 16

typedef float afloat __attribute__ ((__aligned__(16)));

afloat a[N];
afloat b[N+4] = {0.2, 1.3, 2.3, 3.4, 4.5, 5.6, 7.8, 9.0, 10.11, 11.12, 12.13, 13.14, 14.15, 15.16, 16.17, 17.18, 18.19, 19.20}; 
afloat c[N] = {0.2, 1.3, 2.3, 3.4, 4.5, 5.6, 7.8, 9.0, 10.11, 11.12, 12.13, 13.14, 14.15, 15.16};

/* Not vectorizable. Alias. */
int
main2 (afloat *pa, afloat *pb, afloat *pc)
{
  int i;
  afloat *q = pb + 4;

  for (i = 0; i < N; i++)
    {
      pa[i] = q[i] * pc[i];
    }

  for (i = 0; i < N; i++)
    {
      if (pa[i] != q[i] * pc[i])
	abort();
    }
  
  return 0;
}


int main (void)
{ 
  check_vect ();

  main2 (a, b, c);

  return 0;	
}

/* { dg-final { scan-tree-dump-times "vectorized 1 loops" 1 "vect"  { xfail *-*-* } } } */
