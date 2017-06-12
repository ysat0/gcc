/* { dg-do compile } */
/* { dg-options "-O3 -march=armv8-a" } */
/* { dg-require-effective-target arm_hard_vfp_ok }  */

/* Both sdiv and udiv can be used here, so prefer udiv.  */
int f1 (unsigned char *p)
{
  return 100 / p[1];
}

int f2 (unsigned char *p, unsigned short x)
{
  return x / p[0];
}

int f3 (unsigned char *p, int x)
{
  x &= 0x7fffffff;
  return x / p[0];
}

int f5 (unsigned char *p, unsigned short x)
{
  return x % p[0];
}

/* This should only generate signed divisions.  */
int f4 (unsigned char *p)
{
  return -100 / p[1];
}

int f6 (unsigned char *p, short x)
{
  return x % p[0];
}

/* { dg-final { scan-assembler-times "udiv\tr\[0-9\]+, r\[0-9\]+, r\[0-9\]+" 4 } } */
/* { dg-final { scan-assembler-times "sdiv\tr\[0-9\]+, r\[0-9\]+, r\[0-9\]+" 2 } } */
