/* { dg-do compile { target i?86-*-* x86_64-*-* } } */
/* { dg-options "-O0 -mssse3 -msse4a" } */

/* Test that the intrinsics compile without optimization.  All of them are
   defined as inline functions in mmintrin.h that reference the proper
   builtin functions.  Defining away "static" and "__inline" results in
   all of them being compiled as proper functions.  */

#define static
#define __inline

#include <ammintrin.h>
#include <tmmintrin.h>

