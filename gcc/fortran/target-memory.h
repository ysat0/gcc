/* Simulate storage of variables into target memory, header.
   Copyright (C) 2007
   Free Software Foundation, Inc.
   Contributed by Paul Thomas and Brooks Moses

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to the Free
Software Foundation, 51 Franklin Street, Fifth Floor, Boston, MA
02110-1301, USA.  */

#ifndef GFC_TARGET_MEMORY_H
#define GFC_TARGET_MEMORY_H

#include "gfortran.h"

/* Return the size of an expression in its target representation.  */
size_t gfc_target_expr_size (gfc_expr *);

/* Write a constant expression in binary form to a target buffer.  */
int gfc_target_encode_expr (gfc_expr *, unsigned char *, size_t);

/* Read a target buffer into a constant expression.  */

int gfc_interpret_integer (int, unsigned char *, size_t, mpz_t);
int gfc_interpret_float (int, unsigned char *, size_t, mpfr_t);
int gfc_interpret_complex (int, unsigned char *, size_t, mpfr_t, mpfr_t);
int gfc_interpret_logical (int, unsigned char *, size_t, int *);
int gfc_interpret_character (unsigned char *, size_t, gfc_expr *);
int gfc_interpret_derived (unsigned char *, size_t, gfc_expr *);
int gfc_target_interpret_expr (unsigned char *, size_t, gfc_expr *);

#endif /* GFC_TARGET_MEMORY_H  */
