/* { dg-additional-options "-w" } */

#include <assert.h>

/* Test of reduction on loop directive (gangs, non-private reduction
   variable).  */

int
main (int argc, char *argv[])
{
  int i, arr[1024], res = 0, hres = 0;

  for (i = 0; i < 1024; i++)
    arr[i] = i;

  #pragma acc parallel num_gangs(32) num_workers(32) vector_length(32) \
		       copy(res)
  {
    #pragma acc loop gang reduction(+:res)
    for (i = 0; i < 1024; i++)
      res += arr[i];
  }

  for (i = 0; i < 1024; i++)
    hres += arr[i];

  assert (res == hres);

  res = hres = 1;

  #pragma acc parallel num_gangs(32) num_workers(32) vector_length(32) \
		       copy(res)
  {
    #pragma acc loop gang reduction(*:res)
    for (i = 0; i < 12; i++)
      res *= arr[i];
  }

  for (i = 0; i < 12; i++)
    hres *= arr[i];

  assert (res == hres);

  return 0;
}
