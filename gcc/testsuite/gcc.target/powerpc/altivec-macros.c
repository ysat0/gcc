/* Copyright (C) 2007 Free Software Foundation, Inc.  */

/* { dg-do preprocess } */
/* { dg-options "-maltivec" } */

/* Conditional macros should not be expanded by pragmas.  */
#pragma __vector
_Pragma ("__vector")

/* Redefinition of conditional macros.  */
/* No warning should be generated.  */

#define __vector __new_vector
#define __pixel __new_pixel
#define __bool __new_bool
#define vector new_vector
#define pixel new_pixel
#define bool new_bool

/* Definition of conditional macros.  */
/* No warning should be generated.  */

#undef __vector
#define __vector __new_vector

#undef __pixel
#define __pixel __new_pixel

#undef __bool
#define __bool __new_bool

#undef vector
#define vector new_vector

#undef pixel
#define pixel new_pixel

#undef bool
#define bool new_bool

/* Re-definition of "unconditional" macros.  */
/* Warnings should be generated as usual.  */

#define __vector	__newer_vector
#define __pixel		__newer_pixel
#define __bool		__newer_bool
#define vector		newer_vector
#define pixel		newer_pixel
#define bool		newer_bool

/* { dg-warning "redefined" "__vector redefined"  { target *-*-* } 44 } */
/* { dg-warning "redefined" "__pixel redefined"   { target *-*-* } 45 } */
/* { dg-warning "redefined" "__bool redefined"    { target *-*-* } 46 } */
/* { dg-warning "redefined" "vector redefined"    { target *-*-* } 47 } */
/* { dg-warning "redefined" "pixel redefined"     { target *-*-* } 48 } */
/* { dg-warning "redefined" "bool redefined"      { target *-*-* } 49 } */

/* { dg-message "note: previous"  "prev __vector defn"  { target *-*-* } 24 } */
/* { dg-message "note: previous"  "prev __pixel defn"   { target *-*-* } 27 } */
/* { dg-message "note: previous"  "prev __bool defn"    { target *-*-* } 30 } */
/* { dg-message "note: previous"  "prev vector defn"    { target *-*-* } 33 } */
/* { dg-message "note: previous"  "prev pixel defn"     { target *-*-* } 36 } */
/* { dg-message "note: previous"  "prev bool defn"      { target *-*-* } 39 } */
