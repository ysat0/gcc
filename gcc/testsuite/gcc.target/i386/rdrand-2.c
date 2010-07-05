/* { dg-do compile } */
/* { dg-options "-O2 -mrdrnd " } */
/* { dg-final { scan-assembler "rdrand\[ \t]+(%|)eax" } } */

#include <immintrin.h>

unsigned int
read_rdrand32 (void)
{
  return _rdrand_u32 ();
}
