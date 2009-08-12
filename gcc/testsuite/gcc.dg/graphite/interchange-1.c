/* Formerly known as ltrans-1.c */

double u[1782225];
int foo(int N, int *res)
{
  int i, j;
  double sum = 0.0;

  for (i = 0; i < N; i++)
    {
      for (j = 0; j < N; j++)
	sum = sum + u[i + 1335 * j];
      
      u[1336 * i] *= 2;
    }
  *res = sum + N;
}

/* { dg-final { scan-tree-dump-times "will be interchanged" 1 "graphite" { xfail *-*-* } } } */ 
/* { dg-final { cleanup-tree-dump "graphite" } } */
