/* Misc range functions.
   Copyright (C) 2017-2019 Free Software Foundation, Inc.
   Contributed by Aldy Hernandez <aldyh@redhat.com>.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 3, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING3.  If not see
<http://www.gnu.org/licenses/>.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "tree.h"
#include "gimple.h"
#include "gimple-pretty-print.h"
#include "fold-const.h"
#include "ssa.h"
#include "range.h"

value_range_base
range_intersect (const value_range_base &r1, const value_range_base &r2)
{
  value_range_base tmp (r1);
  tmp.intersect (r2);
  return tmp;
}

value_range_base
range_invert (const value_range_base &r1)
{
  value_range_base tmp (r1);
  tmp.invert ();
  return tmp;
}

value_range_base
range_union (const value_range_base &r1, const value_range_base &r2)
{
  value_range_base tmp (r1);
  tmp.union_ (r2);
  return tmp;
}

value_range_base
range_zero (tree type)
{
  return value_range_base (build_zero_cst (type), build_zero_cst (type));
}

value_range_base
range_nonzero (tree type)
{
  return value_range_base (VR_ANTI_RANGE,
			   build_zero_cst (type), build_zero_cst (type));
}

value_range_base
range_positives (tree type)
{
  unsigned prec = TYPE_PRECISION (type);
  signop sign = TYPE_SIGN (type);
  return value_range_base (type, wi::zero (prec), wi::max_value (prec, sign));
}

value_range_base
range_negatives (tree type)
{
  unsigned prec = TYPE_PRECISION (type);
  signop sign = TYPE_SIGN (type);
  value_range_base r;
  if (sign == UNSIGNED)
    r.set_undefined ();
  else
    r = value_range_base (type, wi::min_value (prec, sign),
			  wi::minus_one (prec));
  return r;
}
