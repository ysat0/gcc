/* { dg-do assemble { target powerpc-*-* rs6000-*-* }  } */
/* { dg-options "-O -mpower2 -fno-schedule-insns -w" } */
/* This used to ICE as the peephole was not checking to see
   if the register is a floating point one (I think this cannot
   happen in real life except in this example).  */

register double t1 __asm__("r14");
register double t2 __asm__("r15");
register double t3 __asm__("r16"), t4 __asm__("r17");
void t(double *a, double *b)
{
        t1 = a[-1];
        t2 = a[0];
        t3 = a[1];
        t4 = a[2];
        b[-1] = t1;
        b[0] = t2;
        b[1] = t3;
        b[2] = t4;
}

