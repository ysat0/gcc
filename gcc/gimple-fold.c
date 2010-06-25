/* Statement simplification on GIMPLE.
   Copyright (C) 2010 Free Software Foundation, Inc.
   Split out from tree-ssa-ccp.c.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 3, or (at your option) any
later version.

GCC is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING3.  If not see
<http://www.gnu.org/licenses/>.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "tree.h"
#include "flags.h"
#include "function.h"
#include "tree-dump.h"
#include "tree-flow.h"
#include "tree-pass.h"
#include "tree-ssa-propagate.h"
#include "target.h"


/* If SYM is a constant variable with known value, return the value.
   NULL_TREE is returned otherwise.  */

tree
get_symbol_constant_value (tree sym)
{
  if (TREE_STATIC (sym)
      && (TREE_READONLY (sym)
	  || TREE_CODE (sym) == CONST_DECL))
    {
      tree val = DECL_INITIAL (sym);
      if (val)
	{
	  STRIP_NOPS (val);
	  if (is_gimple_min_invariant (val))
	    {
	      if (TREE_CODE (val) == ADDR_EXPR)
		{
		  tree base = get_base_address (TREE_OPERAND (val, 0));
		  if (base && TREE_CODE (base) == VAR_DECL)
		    {
		      TREE_ADDRESSABLE (base) = 1;
		      if (gimple_referenced_vars (cfun))
			add_referenced_var (base);
		    }
		}
	      return val;
	    }
	}
      /* Variables declared 'const' without an initializer
	 have zero as the initializer if they may not be
	 overridden at link or run time.  */
      if (!val
	  && !DECL_EXTERNAL (sym)
	  && targetm.binds_local_p (sym)
          && (INTEGRAL_TYPE_P (TREE_TYPE (sym))
	       || SCALAR_FLOAT_TYPE_P (TREE_TYPE (sym))))
	return fold_convert (TREE_TYPE (sym), integer_zero_node);
    }

  return NULL_TREE;
}


/* Return true if we may propagate the address expression ADDR into the
   dereference DEREF and cancel them.  */

bool
may_propagate_address_into_dereference (tree addr, tree deref)
{
  gcc_assert (INDIRECT_REF_P (deref)
	      && TREE_CODE (addr) == ADDR_EXPR);

  /* Don't propagate if ADDR's operand has incomplete type.  */
  if (!COMPLETE_TYPE_P (TREE_TYPE (TREE_OPERAND (addr, 0))))
    return false;

  /* If the address is invariant then we do not need to preserve restrict
     qualifications.  But we do need to preserve volatile qualifiers until
     we can annotate the folded dereference itself properly.  */
  if (is_gimple_min_invariant (addr)
      && (!TREE_THIS_VOLATILE (deref)
	  || TYPE_VOLATILE (TREE_TYPE (addr))))
    return useless_type_conversion_p (TREE_TYPE (deref),
				      TREE_TYPE (TREE_OPERAND (addr, 0)));

  /* Else both the address substitution and the folding must result in
     a valid useless type conversion sequence.  */
  return (useless_type_conversion_p (TREE_TYPE (TREE_OPERAND (deref, 0)),
				     TREE_TYPE (addr))
	  && useless_type_conversion_p (TREE_TYPE (deref),
					TREE_TYPE (TREE_OPERAND (addr, 0))));
}


/* A subroutine of fold_stmt.  Attempts to fold *(A+O) to A[X].
   BASE is an array type.  OFFSET is a byte displacement.  ORIG_TYPE
   is the desired result type.

   LOC is the location of the original expression.  */

static tree
maybe_fold_offset_to_array_ref (location_t loc, tree base, tree offset,
				tree orig_type,
				bool allow_negative_idx)
{
  tree min_idx, idx, idx_type, elt_offset = integer_zero_node;
  tree array_type, elt_type, elt_size;
  tree domain_type;

  /* If BASE is an ARRAY_REF, we can pick up another offset (this time
     measured in units of the size of elements type) from that ARRAY_REF).
     We can't do anything if either is variable.

     The case we handle here is *(&A[N]+O).  */
  if (TREE_CODE (base) == ARRAY_REF)
    {
      tree low_bound = array_ref_low_bound (base);

      elt_offset = TREE_OPERAND (base, 1);
      if (TREE_CODE (low_bound) != INTEGER_CST
	  || TREE_CODE (elt_offset) != INTEGER_CST)
	return NULL_TREE;

      elt_offset = int_const_binop (MINUS_EXPR, elt_offset, low_bound, 0);
      base = TREE_OPERAND (base, 0);
    }

  /* Ignore stupid user tricks of indexing non-array variables.  */
  array_type = TREE_TYPE (base);
  if (TREE_CODE (array_type) != ARRAY_TYPE)
    return NULL_TREE;
  elt_type = TREE_TYPE (array_type);
  if (!useless_type_conversion_p (orig_type, elt_type))
    return NULL_TREE;

  /* Use signed size type for intermediate computation on the index.  */
  idx_type = ssizetype;

  /* If OFFSET and ELT_OFFSET are zero, we don't care about the size of the
     element type (so we can use the alignment if it's not constant).
     Otherwise, compute the offset as an index by using a division.  If the
     division isn't exact, then don't do anything.  */
  elt_size = TYPE_SIZE_UNIT (elt_type);
  if (!elt_size)
    return NULL;
  if (integer_zerop (offset))
    {
      if (TREE_CODE (elt_size) != INTEGER_CST)
	elt_size = size_int (TYPE_ALIGN (elt_type));

      idx = build_int_cst (idx_type, 0);
    }
  else
    {
      unsigned HOST_WIDE_INT lquo, lrem;
      HOST_WIDE_INT hquo, hrem;
      double_int soffset;

      /* The final array offset should be signed, so we need
	 to sign-extend the (possibly pointer) offset here
	 and use signed division.  */
      soffset = double_int_sext (tree_to_double_int (offset),
				 TYPE_PRECISION (TREE_TYPE (offset)));
      if (TREE_CODE (elt_size) != INTEGER_CST
	  || div_and_round_double (TRUNC_DIV_EXPR, 0,
				   soffset.low, soffset.high,
				   TREE_INT_CST_LOW (elt_size),
				   TREE_INT_CST_HIGH (elt_size),
				   &lquo, &hquo, &lrem, &hrem)
	  || lrem || hrem)
	return NULL_TREE;

      idx = build_int_cst_wide (idx_type, lquo, hquo);
    }

  /* Assume the low bound is zero.  If there is a domain type, get the
     low bound, if any, convert the index into that type, and add the
     low bound.  */
  min_idx = build_int_cst (idx_type, 0);
  domain_type = TYPE_DOMAIN (array_type);
  if (domain_type)
    {
      idx_type = domain_type;
      if (TYPE_MIN_VALUE (idx_type))
	min_idx = TYPE_MIN_VALUE (idx_type);
      else
	min_idx = fold_convert (idx_type, min_idx);

      if (TREE_CODE (min_idx) != INTEGER_CST)
	return NULL_TREE;

      elt_offset = fold_convert (idx_type, elt_offset);
    }

  if (!integer_zerop (min_idx))
    idx = int_const_binop (PLUS_EXPR, idx, min_idx, 0);
  if (!integer_zerop (elt_offset))
    idx = int_const_binop (PLUS_EXPR, idx, elt_offset, 0);

  /* Make sure to possibly truncate late after offsetting.  */
  idx = fold_convert (idx_type, idx);

  /* We don't want to construct access past array bounds. For example
       char *(c[4]);
       c[3][2];
     should not be simplified into (*c)[14] or tree-vrp will
     give false warnings.  The same is true for
       struct A { long x; char d[0]; } *a;
       (char *)a - 4;
     which should be not folded to &a->d[-8].  */
  if (domain_type
      && TYPE_MAX_VALUE (domain_type)
      && TREE_CODE (TYPE_MAX_VALUE (domain_type)) == INTEGER_CST)
    {
      tree up_bound = TYPE_MAX_VALUE (domain_type);

      if (tree_int_cst_lt (up_bound, idx)
	  /* Accesses after the end of arrays of size 0 (gcc
	     extension) and 1 are likely intentional ("struct
	     hack").  */
	  && compare_tree_int (up_bound, 1) > 0)
	return NULL_TREE;
    }
  if (domain_type
      && TYPE_MIN_VALUE (domain_type))
    {
      if (!allow_negative_idx
	  && TREE_CODE (TYPE_MIN_VALUE (domain_type)) == INTEGER_CST
	  && tree_int_cst_lt (idx, TYPE_MIN_VALUE (domain_type)))
	return NULL_TREE;
    }
  else if (!allow_negative_idx
	   && compare_tree_int (idx, 0) < 0)
    return NULL_TREE;

  {
    tree t = build4 (ARRAY_REF, elt_type, base, idx, NULL_TREE, NULL_TREE);
    SET_EXPR_LOCATION (t, loc);
    return t;
  }
}


/* Attempt to fold *(S+O) to S.X.
   BASE is a record type.  OFFSET is a byte displacement.  ORIG_TYPE
   is the desired result type.

   LOC is the location of the original expression.  */

static tree
maybe_fold_offset_to_component_ref (location_t loc, tree record_type,
				    tree base, tree offset, tree orig_type)
{
  tree f, t, field_type, tail_array_field, field_offset;
  tree ret;
  tree new_base;

  if (TREE_CODE (record_type) != RECORD_TYPE
      && TREE_CODE (record_type) != UNION_TYPE
      && TREE_CODE (record_type) != QUAL_UNION_TYPE)
    return NULL_TREE;

  /* Short-circuit silly cases.  */
  if (useless_type_conversion_p (record_type, orig_type))
    return NULL_TREE;

  tail_array_field = NULL_TREE;
  for (f = TYPE_FIELDS (record_type); f ; f = TREE_CHAIN (f))
    {
      int cmp;

      if (TREE_CODE (f) != FIELD_DECL)
	continue;
      if (DECL_BIT_FIELD (f))
	continue;

      if (!DECL_FIELD_OFFSET (f))
	continue;
      field_offset = byte_position (f);
      if (TREE_CODE (field_offset) != INTEGER_CST)
	continue;

      /* ??? Java creates "interesting" fields for representing base classes.
	 They have no name, and have no context.  With no context, we get into
	 trouble with nonoverlapping_component_refs_p.  Skip them.  */
      if (!DECL_FIELD_CONTEXT (f))
	continue;

      /* The previous array field isn't at the end.  */
      tail_array_field = NULL_TREE;

      /* Check to see if this offset overlaps with the field.  */
      cmp = tree_int_cst_compare (field_offset, offset);
      if (cmp > 0)
	continue;

      field_type = TREE_TYPE (f);

      /* Here we exactly match the offset being checked.  If the types match,
	 then we can return that field.  */
      if (cmp == 0
	  && useless_type_conversion_p (orig_type, field_type))
	{
	  t = fold_build3 (COMPONENT_REF, field_type, base, f, NULL_TREE);
	  return t;
	}

      /* Don't care about offsets into the middle of scalars.  */
      if (!AGGREGATE_TYPE_P (field_type))
	continue;

      /* Check for array at the end of the struct.  This is often
	 used as for flexible array members.  We should be able to
	 turn this into an array access anyway.  */
      if (TREE_CODE (field_type) == ARRAY_TYPE)
	tail_array_field = f;

      /* Check the end of the field against the offset.  */
      if (!DECL_SIZE_UNIT (f)
	  || TREE_CODE (DECL_SIZE_UNIT (f)) != INTEGER_CST)
	continue;
      t = int_const_binop (MINUS_EXPR, offset, field_offset, 1);
      if (!tree_int_cst_lt (t, DECL_SIZE_UNIT (f)))
	continue;

      /* If we matched, then set offset to the displacement into
	 this field.  */
      new_base = fold_build3 (COMPONENT_REF, field_type, base, f, NULL_TREE);
      SET_EXPR_LOCATION (new_base, loc);

      /* Recurse to possibly find the match.  */
      ret = maybe_fold_offset_to_array_ref (loc, new_base, t, orig_type,
					    f == TYPE_FIELDS (record_type));
      if (ret)
	return ret;
      ret = maybe_fold_offset_to_component_ref (loc, field_type, new_base, t,
						orig_type);
      if (ret)
	return ret;
    }

  if (!tail_array_field)
    return NULL_TREE;

  f = tail_array_field;
  field_type = TREE_TYPE (f);
  offset = int_const_binop (MINUS_EXPR, offset, byte_position (f), 1);

  /* If we get here, we've got an aggregate field, and a possibly
     nonzero offset into them.  Recurse and hope for a valid match.  */
  base = fold_build3 (COMPONENT_REF, field_type, base, f, NULL_TREE);
  SET_EXPR_LOCATION (base, loc);

  t = maybe_fold_offset_to_array_ref (loc, base, offset, orig_type,
				      f == TYPE_FIELDS (record_type));
  if (t)
    return t;
  return maybe_fold_offset_to_component_ref (loc, field_type, base, offset,
					     orig_type);
}

/* Attempt to express (ORIG_TYPE)BASE+OFFSET as BASE->field_of_orig_type
   or BASE[index] or by combination of those.

   LOC is the location of original expression.

   Before attempting the conversion strip off existing ADDR_EXPRs and
   handled component refs.  */

tree
maybe_fold_offset_to_reference (location_t loc, tree base, tree offset,
				tree orig_type)
{
  tree ret;
  tree type;

  STRIP_NOPS (base);
  if (TREE_CODE (base) != ADDR_EXPR)
    return NULL_TREE;

  base = TREE_OPERAND (base, 0);

  /* Handle case where existing COMPONENT_REF pick e.g. wrong field of union,
     so it needs to be removed and new COMPONENT_REF constructed.
     The wrong COMPONENT_REF are often constructed by folding the
     (type *)&object within the expression (type *)&object+offset  */
  if (handled_component_p (base))
    {
      HOST_WIDE_INT sub_offset, size, maxsize;
      tree newbase;
      newbase = get_ref_base_and_extent (base, &sub_offset,
					 &size, &maxsize);
      gcc_assert (newbase);
      if (size == maxsize
	  && size != -1
	  && !(sub_offset & (BITS_PER_UNIT - 1)))
	{
	  base = newbase;
	  if (sub_offset)
	    offset = int_const_binop (PLUS_EXPR, offset,
				      build_int_cst (TREE_TYPE (offset),
						     sub_offset / BITS_PER_UNIT), 1);
	}
    }
  if (useless_type_conversion_p (orig_type, TREE_TYPE (base))
      && integer_zerop (offset))
    return base;
  type = TREE_TYPE (base);

  ret = maybe_fold_offset_to_component_ref (loc, type, base, offset, orig_type);
  if (!ret)
    ret = maybe_fold_offset_to_array_ref (loc, base, offset, orig_type, true);

  return ret;
}

/* Attempt to express (ORIG_TYPE)&BASE+OFFSET as &BASE->field_of_orig_type
   or &BASE[index] or by combination of those.

   LOC is the location of the original expression.

   Before attempting the conversion strip off existing component refs.  */

tree
maybe_fold_offset_to_address (location_t loc, tree addr, tree offset,
			      tree orig_type)
{
  tree t;

  gcc_assert (POINTER_TYPE_P (TREE_TYPE (addr))
	      && POINTER_TYPE_P (orig_type));

  t = maybe_fold_offset_to_reference (loc, addr, offset,
				      TREE_TYPE (orig_type));
  if (t != NULL_TREE)
    {
      tree orig = addr;
      tree ptr_type;

      /* For __builtin_object_size to function correctly we need to
         make sure not to fold address arithmetic so that we change
	 reference from one array to another.  This would happen for
	 example for

	   struct X { char s1[10]; char s2[10] } s;
	   char *foo (void) { return &s.s2[-4]; }

	 where we need to avoid generating &s.s1[6].  As the C and
	 C++ frontends create different initial trees
	 (char *) &s.s1 + -4  vs.  &s.s1[-4]  we have to do some
	 sophisticated comparisons here.  Note that checking for the
	 condition after the fact is easier than trying to avoid doing
	 the folding.  */
      STRIP_NOPS (orig);
      if (TREE_CODE (orig) == ADDR_EXPR)
	orig = TREE_OPERAND (orig, 0);
      if ((TREE_CODE (orig) == ARRAY_REF
	   || (TREE_CODE (orig) == COMPONENT_REF
	       && TREE_CODE (TREE_TYPE (TREE_OPERAND (orig, 1))) == ARRAY_TYPE))
	  && (TREE_CODE (t) == ARRAY_REF
	      || TREE_CODE (t) == COMPONENT_REF)
	  && !operand_equal_p (TREE_CODE (orig) == ARRAY_REF
			       ? TREE_OPERAND (orig, 0) : orig,
			       TREE_CODE (t) == ARRAY_REF
			       ? TREE_OPERAND (t, 0) : t, 0))
	return NULL_TREE;

      ptr_type = build_pointer_type (TREE_TYPE (t));
      if (!useless_type_conversion_p (orig_type, ptr_type))
	return NULL_TREE;
      return build_fold_addr_expr_with_type_loc (loc, t, ptr_type);
    }

  return NULL_TREE;
}

/* A subroutine of fold_stmt.  Attempt to simplify *(BASE+OFFSET).
   Return the simplified expression, or NULL if nothing could be done.  */

static tree
maybe_fold_stmt_indirect (tree expr, tree base, tree offset)
{
  tree t;
  bool volatile_p = TREE_THIS_VOLATILE (expr);
  location_t loc = EXPR_LOCATION (expr);

  /* We may well have constructed a double-nested PLUS_EXPR via multiple
     substitutions.  Fold that down to one.  Remove NON_LVALUE_EXPRs that
     are sometimes added.  */
  base = fold (base);
  STRIP_TYPE_NOPS (base);
  TREE_OPERAND (expr, 0) = base;

  /* One possibility is that the address reduces to a string constant.  */
  t = fold_read_from_constant_string (expr);
  if (t)
    return t;

  /* Add in any offset from a POINTER_PLUS_EXPR.  */
  if (TREE_CODE (base) == POINTER_PLUS_EXPR)
    {
      tree offset2;

      offset2 = TREE_OPERAND (base, 1);
      if (TREE_CODE (offset2) != INTEGER_CST)
	return NULL_TREE;
      base = TREE_OPERAND (base, 0);

      offset = fold_convert (sizetype,
			     int_const_binop (PLUS_EXPR, offset, offset2, 1));
    }

  if (TREE_CODE (base) == ADDR_EXPR)
    {
      tree base_addr = base;

      /* Strip the ADDR_EXPR.  */
      base = TREE_OPERAND (base, 0);

      /* Fold away CONST_DECL to its value, if the type is scalar.  */
      if (TREE_CODE (base) == CONST_DECL
	  && is_gimple_min_invariant (DECL_INITIAL (base)))
	return DECL_INITIAL (base);

      /* If there is no offset involved simply return the folded base.  */
      if (integer_zerop (offset))
	return base;

      /* Try folding *(&B+O) to B.X.  */
      t = maybe_fold_offset_to_reference (loc, base_addr, offset,
					  TREE_TYPE (expr));
      if (t)
	{
	  /* Preserve volatileness of the original expression.
	     We can end up with a plain decl here which is shared
	     and we shouldn't mess with its flags.  */
	  if (!SSA_VAR_P (t))
	    TREE_THIS_VOLATILE (t) = volatile_p;
	  return t;
	}
    }
  else
    {
      /* We can get here for out-of-range string constant accesses,
	 such as "_"[3].  Bail out of the entire substitution search
	 and arrange for the entire statement to be replaced by a
	 call to __builtin_trap.  In all likelihood this will all be
	 constant-folded away, but in the meantime we can't leave with
	 something that get_expr_operands can't understand.  */

      t = base;
      STRIP_NOPS (t);
      if (TREE_CODE (t) == ADDR_EXPR
	  && TREE_CODE (TREE_OPERAND (t, 0)) == STRING_CST)
	{
	  /* FIXME: Except that this causes problems elsewhere with dead
	     code not being deleted, and we die in the rtl expanders
	     because we failed to remove some ssa_name.  In the meantime,
	     just return zero.  */
	  /* FIXME2: This condition should be signaled by
	     fold_read_from_constant_string directly, rather than
	     re-checking for it here.  */
	  return integer_zero_node;
	}

      /* Try folding *(B+O) to B->X.  Still an improvement.  */
      if (POINTER_TYPE_P (TREE_TYPE (base)))
	{
          t = maybe_fold_offset_to_reference (loc, base, offset,
				              TREE_TYPE (expr));
	  if (t)
	    return t;
	}
    }

  /* Otherwise we had an offset that we could not simplify.  */
  return NULL_TREE;
}


/* A quaint feature extant in our address arithmetic is that there
   can be hidden type changes here.  The type of the result need
   not be the same as the type of the input pointer.

   What we're after here is an expression of the form
	(T *)(&array + const)
   where array is OP0, const is OP1, RES_TYPE is T and
   the cast doesn't actually exist, but is implicit in the
   type of the POINTER_PLUS_EXPR.  We'd like to turn this into
	&array[x]
   which may be able to propagate further.  */

tree
maybe_fold_stmt_addition (location_t loc, tree res_type, tree op0, tree op1)
{
  tree ptd_type;
  tree t;

  /* The first operand should be an ADDR_EXPR.  */
  if (TREE_CODE (op0) != ADDR_EXPR)
    return NULL_TREE;
  op0 = TREE_OPERAND (op0, 0);

  /* It had better be a constant.  */
  if (TREE_CODE (op1) != INTEGER_CST)
    {
      /* Or op0 should now be A[0] and the non-constant offset defined
	 via a multiplication by the array element size.  */
      if (TREE_CODE (op0) == ARRAY_REF
	  && integer_zerop (TREE_OPERAND (op0, 1))
	  && TREE_CODE (op1) == SSA_NAME
	  && host_integerp (TYPE_SIZE_UNIT (TREE_TYPE (op0)), 1))
	{
	  gimple offset_def = SSA_NAME_DEF_STMT (op1);
	  if (!is_gimple_assign (offset_def))
	    return NULL_TREE;

	  /* As we will end up creating a variable index array access
	     in the outermost array dimension make sure there isn't
	     a more inner array that the index could overflow to.  */
	  if (TREE_CODE (TREE_OPERAND (op0, 0)) == ARRAY_REF)
	    return NULL_TREE;

	  /* Do not build array references of something that we can't
	     see the true number of array dimensions for.  */
	  if (!DECL_P (TREE_OPERAND (op0, 0))
	      && !handled_component_p (TREE_OPERAND (op0, 0)))
	    return NULL_TREE;

	  if (gimple_assign_rhs_code (offset_def) == MULT_EXPR
	      && TREE_CODE (gimple_assign_rhs2 (offset_def)) == INTEGER_CST
	      && tree_int_cst_equal (gimple_assign_rhs2 (offset_def),
				     TYPE_SIZE_UNIT (TREE_TYPE (op0))))
	    return build_fold_addr_expr
			  (build4 (ARRAY_REF, TREE_TYPE (op0),
				   TREE_OPERAND (op0, 0),
				   gimple_assign_rhs1 (offset_def),
				   TREE_OPERAND (op0, 2),
				   TREE_OPERAND (op0, 3)));
	  else if (integer_onep (TYPE_SIZE_UNIT (TREE_TYPE (op0)))
		   && gimple_assign_rhs_code (offset_def) != MULT_EXPR)
	    return build_fold_addr_expr
			  (build4 (ARRAY_REF, TREE_TYPE (op0),
				   TREE_OPERAND (op0, 0),
				   op1,
				   TREE_OPERAND (op0, 2),
				   TREE_OPERAND (op0, 3)));
	}
      return NULL_TREE;
    }

  /* If the first operand is an ARRAY_REF, expand it so that we can fold
     the offset into it.  */
  while (TREE_CODE (op0) == ARRAY_REF)
    {
      tree array_obj = TREE_OPERAND (op0, 0);
      tree array_idx = TREE_OPERAND (op0, 1);
      tree elt_type = TREE_TYPE (op0);
      tree elt_size = TYPE_SIZE_UNIT (elt_type);
      tree min_idx;

      if (TREE_CODE (array_idx) != INTEGER_CST)
	break;
      if (TREE_CODE (elt_size) != INTEGER_CST)
	break;

      /* Un-bias the index by the min index of the array type.  */
      min_idx = TYPE_DOMAIN (TREE_TYPE (array_obj));
      if (min_idx)
	{
	  min_idx = TYPE_MIN_VALUE (min_idx);
	  if (min_idx)
	    {
	      if (TREE_CODE (min_idx) != INTEGER_CST)
		break;

	      array_idx = fold_convert (TREE_TYPE (min_idx), array_idx);
	      if (!integer_zerop (min_idx))
		array_idx = int_const_binop (MINUS_EXPR, array_idx,
					     min_idx, 0);
	    }
	}

      /* Convert the index to a byte offset.  */
      array_idx = fold_convert (sizetype, array_idx);
      array_idx = int_const_binop (MULT_EXPR, array_idx, elt_size, 0);

      /* Update the operands for the next round, or for folding.  */
      op1 = int_const_binop (PLUS_EXPR,
			     array_idx, op1, 0);
      op0 = array_obj;
    }

  ptd_type = TREE_TYPE (res_type);
  /* If we want a pointer to void, reconstruct the reference from the
     array element type.  A pointer to that can be trivially converted
     to void *.  This happens as we fold (void *)(ptr p+ off).  */
  if (VOID_TYPE_P (ptd_type)
      && TREE_CODE (TREE_TYPE (op0)) == ARRAY_TYPE)
    ptd_type = TREE_TYPE (TREE_TYPE (op0));

  /* At which point we can try some of the same things as for indirects.  */
  t = maybe_fold_offset_to_array_ref (loc, op0, op1, ptd_type, true);
  if (!t)
    t = maybe_fold_offset_to_component_ref (loc, TREE_TYPE (op0), op0, op1,
					    ptd_type);
  if (t)
    {
      t = build1 (ADDR_EXPR, res_type, t);
      SET_EXPR_LOCATION (t, loc);
    }

  return t;
}

/* Subroutine of fold_stmt.  We perform several simplifications of the
   memory reference tree EXPR and make sure to re-gimplify them properly
   after propagation of constant addresses.  IS_LHS is true if the
   reference is supposed to be an lvalue.  */

static tree
maybe_fold_reference (tree expr, bool is_lhs)
{
  tree *t = &expr;

  if (TREE_CODE (expr) == ARRAY_REF
      && !is_lhs)
    {
      tree tem = fold_read_from_constant_string (expr);
      if (tem)
	return tem;
    }

  /* ???  We might want to open-code the relevant remaining cases
     to avoid using the generic fold.  */
  if (handled_component_p (*t)
      && CONSTANT_CLASS_P (TREE_OPERAND (*t, 0)))
    {
      tree tem = fold (*t);
      if (tem != *t)
	return tem;
    }

  while (handled_component_p (*t))
    t = &TREE_OPERAND (*t, 0);

  if (TREE_CODE (*t) == INDIRECT_REF)
    {
      tree tem = maybe_fold_stmt_indirect (*t, TREE_OPERAND (*t, 0),
					   integer_zero_node);
      /* Avoid folding *"abc" = 5 into 'a' = 5.  */
      if (is_lhs && tem && CONSTANT_CLASS_P (tem))
	tem = NULL_TREE;
      if (!tem
	  && TREE_CODE (TREE_OPERAND (*t, 0)) == ADDR_EXPR)
	/* If we had a good reason for propagating the address here,
	   make sure we end up with valid gimple.  See PR34989.  */
	tem = TREE_OPERAND (TREE_OPERAND (*t, 0), 0);

      if (tem)
	{
	  *t = tem;
	  tem = maybe_fold_reference (expr, is_lhs);
	  if (tem)
	    return tem;
	  return expr;
	}
    }
  else if (!is_lhs
	   && DECL_P (*t))
    {
      tree tem = get_symbol_constant_value (*t);
      if (tem
	  && useless_type_conversion_p (TREE_TYPE (*t), TREE_TYPE (tem)))
	{
	  *t = unshare_expr (tem);
	  tem = maybe_fold_reference (expr, is_lhs);
	  if (tem)
	    return tem;
	  return expr;
	}
    }

  return NULL_TREE;
}


/* Attempt to fold an assignment statement pointed-to by SI.  Returns a
   replacement rhs for the statement or NULL_TREE if no simplification
   could be made.  It is assumed that the operands have been previously
   folded.  */

static tree
fold_gimple_assign (gimple_stmt_iterator *si)
{
  gimple stmt = gsi_stmt (*si);
  enum tree_code subcode = gimple_assign_rhs_code (stmt);
  location_t loc = gimple_location (stmt);

  tree result = NULL_TREE;

  switch (get_gimple_rhs_class (subcode))
    {
    case GIMPLE_SINGLE_RHS:
      {
        tree rhs = gimple_assign_rhs1 (stmt);

        /* Try to fold a conditional expression.  */
        if (TREE_CODE (rhs) == COND_EXPR)
          {
	    tree op0 = COND_EXPR_COND (rhs);
	    tree tem;
	    bool set = false;
	    location_t cond_loc = EXPR_LOCATION (rhs);

	    if (COMPARISON_CLASS_P (op0))
	      {
		fold_defer_overflow_warnings ();
		tem = fold_binary_loc (cond_loc,
				   TREE_CODE (op0), TREE_TYPE (op0),
				   TREE_OPERAND (op0, 0),
				   TREE_OPERAND (op0, 1));
		/* This is actually a conditional expression, not a GIMPLE
		   conditional statement, however, the valid_gimple_rhs_p
		   test still applies.  */
		set = (tem && is_gimple_condexpr (tem)
		       && valid_gimple_rhs_p (tem));
		fold_undefer_overflow_warnings (set, stmt, 0);
	      }
	    else if (is_gimple_min_invariant (op0))
	      {
		tem = op0;
		set = true;
	      }
	    else
	      return NULL_TREE;

	    if (set)
	      result = fold_build3_loc (cond_loc, COND_EXPR, TREE_TYPE (rhs), tem,
				    COND_EXPR_THEN (rhs), COND_EXPR_ELSE (rhs));
          }

	else if (TREE_CODE (rhs) == TARGET_MEM_REF)
	  return maybe_fold_tmr (rhs);

	else if (REFERENCE_CLASS_P (rhs))
	  return maybe_fold_reference (rhs, false);

	else if (TREE_CODE (rhs) == ADDR_EXPR)
	  {
	    tree tem = maybe_fold_reference (TREE_OPERAND (rhs, 0), true);
	    if (tem)
	      result = fold_convert (TREE_TYPE (rhs),
				     build_fold_addr_expr_loc (loc, tem));
	  }

	else if (TREE_CODE (rhs) == CONSTRUCTOR
		 && TREE_CODE (TREE_TYPE (rhs)) == VECTOR_TYPE
		 && (CONSTRUCTOR_NELTS (rhs)
		     == TYPE_VECTOR_SUBPARTS (TREE_TYPE (rhs))))
	  {
	    /* Fold a constant vector CONSTRUCTOR to VECTOR_CST.  */
	    unsigned i;
	    tree val;

	    FOR_EACH_CONSTRUCTOR_VALUE (CONSTRUCTOR_ELTS (rhs), i, val)
	      if (TREE_CODE (val) != INTEGER_CST
		  && TREE_CODE (val) != REAL_CST
		  && TREE_CODE (val) != FIXED_CST)
		return NULL_TREE;

	    return build_vector_from_ctor (TREE_TYPE (rhs),
					   CONSTRUCTOR_ELTS (rhs));
	  }

	else if (DECL_P (rhs))
	  return unshare_expr (get_symbol_constant_value (rhs));

        /* If we couldn't fold the RHS, hand over to the generic
           fold routines.  */
        if (result == NULL_TREE)
          result = fold (rhs);

        /* Strip away useless type conversions.  Both the NON_LVALUE_EXPR
           that may have been added by fold, and "useless" type
           conversions that might now be apparent due to propagation.  */
        STRIP_USELESS_TYPE_CONVERSION (result);

        if (result != rhs && valid_gimple_rhs_p (result))
	  return result;

	return NULL_TREE;
      }
      break;

    case GIMPLE_UNARY_RHS:
      {
	tree rhs = gimple_assign_rhs1 (stmt);

	result = fold_unary_loc (loc, subcode, gimple_expr_type (stmt), rhs);
	if (result)
	  {
	    /* If the operation was a conversion do _not_ mark a
	       resulting constant with TREE_OVERFLOW if the original
	       constant was not.  These conversions have implementation
	       defined behavior and retaining the TREE_OVERFLOW flag
	       here would confuse later passes such as VRP.  */
	    if (CONVERT_EXPR_CODE_P (subcode)
		&& TREE_CODE (result) == INTEGER_CST
		&& TREE_CODE (rhs) == INTEGER_CST)
	      TREE_OVERFLOW (result) = TREE_OVERFLOW (rhs);

	    STRIP_USELESS_TYPE_CONVERSION (result);
	    if (valid_gimple_rhs_p (result))
	      return result;
	  }
	else if (CONVERT_EXPR_CODE_P (subcode)
		 && POINTER_TYPE_P (gimple_expr_type (stmt))
		 && POINTER_TYPE_P (TREE_TYPE (gimple_assign_rhs1 (stmt))))
	  {
	    tree type = gimple_expr_type (stmt);
	    tree t = maybe_fold_offset_to_address (loc,
						   gimple_assign_rhs1 (stmt),
						   integer_zero_node, type);
	    if (t)
	      return t;
	  }
      }
      break;

    case GIMPLE_BINARY_RHS:
      /* Try to fold pointer addition.  */
      if (gimple_assign_rhs_code (stmt) == POINTER_PLUS_EXPR)
	{
	  tree type = TREE_TYPE (gimple_assign_rhs1 (stmt));
	  if (TREE_CODE (TREE_TYPE (type)) == ARRAY_TYPE)
	    {
	      type = build_pointer_type (TREE_TYPE (TREE_TYPE (type)));
	      if (!useless_type_conversion_p
		    (TREE_TYPE (gimple_assign_lhs (stmt)), type))
		type = TREE_TYPE (gimple_assign_rhs1 (stmt));
	    }
	  result = maybe_fold_stmt_addition (gimple_location (stmt),
					     type,
					     gimple_assign_rhs1 (stmt),
					     gimple_assign_rhs2 (stmt));
	}

      if (!result)
        result = fold_binary_loc (loc, subcode,
                              TREE_TYPE (gimple_assign_lhs (stmt)),
                              gimple_assign_rhs1 (stmt),
                              gimple_assign_rhs2 (stmt));

      if (result)
        {
          STRIP_USELESS_TYPE_CONVERSION (result);
          if (valid_gimple_rhs_p (result))
	    return result;

	  /* Fold might have produced non-GIMPLE, so if we trust it blindly
	     we lose canonicalization opportunities.  Do not go again
	     through fold here though, or the same non-GIMPLE will be
	     produced.  */
          if (commutative_tree_code (subcode)
              && tree_swap_operands_p (gimple_assign_rhs1 (stmt),
                                       gimple_assign_rhs2 (stmt), false))
            return build2 (subcode, TREE_TYPE (gimple_assign_lhs (stmt)),
                           gimple_assign_rhs2 (stmt),
                           gimple_assign_rhs1 (stmt));
        }
      break;

    case GIMPLE_TERNARY_RHS:
      result = fold_ternary_loc (loc, subcode,
				 TREE_TYPE (gimple_assign_lhs (stmt)),
				 gimple_assign_rhs1 (stmt),
				 gimple_assign_rhs2 (stmt),
				 gimple_assign_rhs3 (stmt));

      if (result)
        {
          STRIP_USELESS_TYPE_CONVERSION (result);
          if (valid_gimple_rhs_p (result))
	    return result;

	  /* Fold might have produced non-GIMPLE, so if we trust it blindly
	     we lose canonicalization opportunities.  Do not go again
	     through fold here though, or the same non-GIMPLE will be
	     produced.  */
          if (commutative_ternary_tree_code (subcode)
              && tree_swap_operands_p (gimple_assign_rhs1 (stmt),
                                       gimple_assign_rhs2 (stmt), false))
            return build3 (subcode, TREE_TYPE (gimple_assign_lhs (stmt)),
			   gimple_assign_rhs2 (stmt),
			   gimple_assign_rhs1 (stmt),
			   gimple_assign_rhs3 (stmt));
        }
      break;

    case GIMPLE_INVALID_RHS:
      gcc_unreachable ();
    }

  return NULL_TREE;
}

/* Attempt to fold a conditional statement. Return true if any changes were
   made. We only attempt to fold the condition expression, and do not perform
   any transformation that would require alteration of the cfg.  It is
   assumed that the operands have been previously folded.  */

static bool
fold_gimple_cond (gimple stmt)
{
  tree result = fold_binary_loc (gimple_location (stmt),
			     gimple_cond_code (stmt),
                             boolean_type_node,
                             gimple_cond_lhs (stmt),
                             gimple_cond_rhs (stmt));

  if (result)
    {
      STRIP_USELESS_TYPE_CONVERSION (result);
      if (is_gimple_condexpr (result) && valid_gimple_rhs_p (result))
        {
          gimple_cond_set_condition_from_tree (stmt, result);
          return true;
        }
    }

  return false;
}

/* Convert EXPR into a GIMPLE value suitable for substitution on the
   RHS of an assignment.  Insert the necessary statements before
   iterator *SI_P.  The statement at *SI_P, which must be a GIMPLE_CALL
   is replaced.  If the call is expected to produces a result, then it
   is replaced by an assignment of the new RHS to the result variable.
   If the result is to be ignored, then the call is replaced by a
   GIMPLE_NOP.  */

void
gimplify_and_update_call_from_tree (gimple_stmt_iterator *si_p, tree expr)
{
  tree lhs;
  tree tmp = NULL_TREE;  /* Silence warning.  */
  gimple stmt, new_stmt;
  gimple_stmt_iterator i;
  gimple_seq stmts = gimple_seq_alloc();
  struct gimplify_ctx gctx;
  gimple last = NULL;

  stmt = gsi_stmt (*si_p);

  gcc_assert (is_gimple_call (stmt));

  lhs = gimple_call_lhs (stmt);

  push_gimplify_context (&gctx);

  if (lhs == NULL_TREE)
    gimplify_and_add (expr, &stmts);
  else
    tmp = get_initialized_tmp_var (expr, &stmts, NULL);

  pop_gimplify_context (NULL);

  if (gimple_has_location (stmt))
    annotate_all_with_location (stmts, gimple_location (stmt));

  /* The replacement can expose previously unreferenced variables.  */
  for (i = gsi_start (stmts); !gsi_end_p (i); gsi_next (&i))
    {
      if (last)
	{
	  gsi_insert_before (si_p, last, GSI_NEW_STMT);
	  gsi_next (si_p);
	}
      new_stmt = gsi_stmt (i);
      find_new_referenced_vars (new_stmt);
      mark_symbols_for_renaming (new_stmt);
      last = new_stmt;
    }

  if (lhs == NULL_TREE)
    {
      unlink_stmt_vdef (stmt);
      release_defs (stmt);
      new_stmt = last;
    }
  else
    {
      if (last)
	{
	  gsi_insert_before (si_p, last, GSI_NEW_STMT);
	  gsi_next (si_p);
	}
      new_stmt = gimple_build_assign (lhs, tmp);
      gimple_set_vuse (new_stmt, gimple_vuse (stmt));
      gimple_set_vdef (new_stmt, gimple_vdef (stmt));
      move_ssa_defining_stmt_for_defs (new_stmt, stmt);
    }

  gimple_set_location (new_stmt, gimple_location (stmt));
  gsi_replace (si_p, new_stmt, false);
}

/* Return the string length, maximum string length or maximum value of
   ARG in LENGTH.
   If ARG is an SSA name variable, follow its use-def chains.  If LENGTH
   is not NULL and, for TYPE == 0, its value is not equal to the length
   we determine or if we are unable to determine the length or value,
   return false.  VISITED is a bitmap of visited variables.
   TYPE is 0 if string length should be returned, 1 for maximum string
   length and 2 for maximum value ARG can have.  */

static bool
get_maxval_strlen (tree arg, tree *length, bitmap visited, int type)
{
  tree var, val;
  gimple def_stmt;

  if (TREE_CODE (arg) != SSA_NAME)
    {
      if (TREE_CODE (arg) == COND_EXPR)
        return get_maxval_strlen (COND_EXPR_THEN (arg), length, visited, type)
               && get_maxval_strlen (COND_EXPR_ELSE (arg), length, visited, type);
      /* We can end up with &(*iftmp_1)[0] here as well, so handle it.  */
      else if (TREE_CODE (arg) == ADDR_EXPR
	       && TREE_CODE (TREE_OPERAND (arg, 0)) == ARRAY_REF
	       && integer_zerop (TREE_OPERAND (TREE_OPERAND (arg, 0), 1)))
	{
	  tree aop0 = TREE_OPERAND (TREE_OPERAND (arg, 0), 0);
	  if (TREE_CODE (aop0) == INDIRECT_REF
	      && TREE_CODE (TREE_OPERAND (aop0, 0)) == SSA_NAME)
	    return get_maxval_strlen (TREE_OPERAND (aop0, 0),
				      length, visited, type);
	}

      if (type == 2)
	{
	  val = arg;
	  if (TREE_CODE (val) != INTEGER_CST
	      || tree_int_cst_sgn (val) < 0)
	    return false;
	}
      else
	val = c_strlen (arg, 1);
      if (!val)
	return false;

      if (*length)
	{
	  if (type > 0)
	    {
	      if (TREE_CODE (*length) != INTEGER_CST
		  || TREE_CODE (val) != INTEGER_CST)
		return false;

	      if (tree_int_cst_lt (*length, val))
		*length = val;
	      return true;
	    }
	  else if (simple_cst_equal (val, *length) != 1)
	    return false;
	}

      *length = val;
      return true;
    }

  /* If we were already here, break the infinite cycle.  */
  if (bitmap_bit_p (visited, SSA_NAME_VERSION (arg)))
    return true;
  bitmap_set_bit (visited, SSA_NAME_VERSION (arg));

  var = arg;
  def_stmt = SSA_NAME_DEF_STMT (var);

  switch (gimple_code (def_stmt))
    {
      case GIMPLE_ASSIGN:
        /* The RHS of the statement defining VAR must either have a
           constant length or come from another SSA_NAME with a constant
           length.  */
        if (gimple_assign_single_p (def_stmt)
            || gimple_assign_unary_nop_p (def_stmt))
          {
            tree rhs = gimple_assign_rhs1 (def_stmt);
            return get_maxval_strlen (rhs, length, visited, type);
          }
        return false;

      case GIMPLE_PHI:
	{
	  /* All the arguments of the PHI node must have the same constant
	     length.  */
	  unsigned i;

	  for (i = 0; i < gimple_phi_num_args (def_stmt); i++)
          {
            tree arg = gimple_phi_arg (def_stmt, i)->def;

            /* If this PHI has itself as an argument, we cannot
               determine the string length of this argument.  However,
               if we can find a constant string length for the other
               PHI args then we can still be sure that this is a
               constant string length.  So be optimistic and just
               continue with the next argument.  */
            if (arg == gimple_phi_result (def_stmt))
              continue;

            if (!get_maxval_strlen (arg, length, visited, type))
              return false;
          }
        }
        return true;

      default:
        return false;
    }
}


/* Fold builtin call in statement STMT.  Returns a simplified tree.
   We may return a non-constant expression, including another call
   to a different function and with different arguments, e.g.,
   substituting memcpy for strcpy when the string length is known.
   Note that some builtins expand into inline code that may not
   be valid in GIMPLE.  Callers must take care.  */

tree
gimple_fold_builtin (gimple stmt)
{
  tree result, val[3];
  tree callee, a;
  int arg_idx, type;
  bitmap visited;
  bool ignore;
  int nargs;
  location_t loc = gimple_location (stmt);

  gcc_assert (is_gimple_call (stmt));

  ignore = (gimple_call_lhs (stmt) == NULL);

  /* First try the generic builtin folder.  If that succeeds, return the
     result directly.  */
  result = fold_call_stmt (stmt, ignore);
  if (result)
    {
      if (ignore)
	STRIP_NOPS (result);
      return result;
    }

  /* Ignore MD builtins.  */
  callee = gimple_call_fndecl (stmt);
  if (DECL_BUILT_IN_CLASS (callee) == BUILT_IN_MD)
    return NULL_TREE;

  /* If the builtin could not be folded, and it has no argument list,
     we're done.  */
  nargs = gimple_call_num_args (stmt);
  if (nargs == 0)
    return NULL_TREE;

  /* Limit the work only for builtins we know how to simplify.  */
  switch (DECL_FUNCTION_CODE (callee))
    {
    case BUILT_IN_STRLEN:
    case BUILT_IN_FPUTS:
    case BUILT_IN_FPUTS_UNLOCKED:
      arg_idx = 0;
      type = 0;
      break;
    case BUILT_IN_STRCPY:
    case BUILT_IN_STRNCPY:
      arg_idx = 1;
      type = 0;
      break;
    case BUILT_IN_MEMCPY_CHK:
    case BUILT_IN_MEMPCPY_CHK:
    case BUILT_IN_MEMMOVE_CHK:
    case BUILT_IN_MEMSET_CHK:
    case BUILT_IN_STRNCPY_CHK:
      arg_idx = 2;
      type = 2;
      break;
    case BUILT_IN_STRCPY_CHK:
    case BUILT_IN_STPCPY_CHK:
      arg_idx = 1;
      type = 1;
      break;
    case BUILT_IN_SNPRINTF_CHK:
    case BUILT_IN_VSNPRINTF_CHK:
      arg_idx = 1;
      type = 2;
      break;
    default:
      return NULL_TREE;
    }

  if (arg_idx >= nargs)
    return NULL_TREE;

  /* Try to use the dataflow information gathered by the CCP process.  */
  visited = BITMAP_ALLOC (NULL);
  bitmap_clear (visited);

  memset (val, 0, sizeof (val));
  a = gimple_call_arg (stmt, arg_idx);
  if (!get_maxval_strlen (a, &val[arg_idx], visited, type))
    val[arg_idx] = NULL_TREE;

  BITMAP_FREE (visited);

  result = NULL_TREE;
  switch (DECL_FUNCTION_CODE (callee))
    {
    case BUILT_IN_STRLEN:
      if (val[0] && nargs == 1)
	{
	  tree new_val =
              fold_convert (TREE_TYPE (gimple_call_lhs (stmt)), val[0]);

	  /* If the result is not a valid gimple value, or not a cast
	     of a valid gimple value, then we can not use the result.  */
	  if (is_gimple_val (new_val)
	      || (is_gimple_cast (new_val)
		  && is_gimple_val (TREE_OPERAND (new_val, 0))))
	    return new_val;
	}
      break;

    case BUILT_IN_STRCPY:
      if (val[1] && is_gimple_val (val[1]) && nargs == 2)
	result = fold_builtin_strcpy (loc, callee,
                                      gimple_call_arg (stmt, 0),
                                      gimple_call_arg (stmt, 1),
				      val[1]);
      break;

    case BUILT_IN_STRNCPY:
      if (val[1] && is_gimple_val (val[1]) && nargs == 3)
	result = fold_builtin_strncpy (loc, callee,
                                       gimple_call_arg (stmt, 0),
                                       gimple_call_arg (stmt, 1),
                                       gimple_call_arg (stmt, 2),
				       val[1]);
      break;

    case BUILT_IN_FPUTS:
      if (nargs == 2)
	result = fold_builtin_fputs (loc, gimple_call_arg (stmt, 0),
				     gimple_call_arg (stmt, 1),
				     ignore, false, val[0]);
      break;

    case BUILT_IN_FPUTS_UNLOCKED:
      if (nargs == 2)
	result = fold_builtin_fputs (loc, gimple_call_arg (stmt, 0),
				     gimple_call_arg (stmt, 1),
				     ignore, true, val[0]);
      break;

    case BUILT_IN_MEMCPY_CHK:
    case BUILT_IN_MEMPCPY_CHK:
    case BUILT_IN_MEMMOVE_CHK:
    case BUILT_IN_MEMSET_CHK:
      if (val[2] && is_gimple_val (val[2]) && nargs == 4)
	result = fold_builtin_memory_chk (loc, callee,
                                          gimple_call_arg (stmt, 0),
                                          gimple_call_arg (stmt, 1),
                                          gimple_call_arg (stmt, 2),
                                          gimple_call_arg (stmt, 3),
					  val[2], ignore,
					  DECL_FUNCTION_CODE (callee));
      break;

    case BUILT_IN_STRCPY_CHK:
    case BUILT_IN_STPCPY_CHK:
      if (val[1] && is_gimple_val (val[1]) && nargs == 3)
	result = fold_builtin_stxcpy_chk (loc, callee,
                                          gimple_call_arg (stmt, 0),
                                          gimple_call_arg (stmt, 1),
                                          gimple_call_arg (stmt, 2),
					  val[1], ignore,
					  DECL_FUNCTION_CODE (callee));
      break;

    case BUILT_IN_STRNCPY_CHK:
      if (val[2] && is_gimple_val (val[2]) && nargs == 4)
	result = fold_builtin_strncpy_chk (loc, gimple_call_arg (stmt, 0),
                                           gimple_call_arg (stmt, 1),
                                           gimple_call_arg (stmt, 2),
                                           gimple_call_arg (stmt, 3),
					   val[2]);
      break;

    case BUILT_IN_SNPRINTF_CHK:
    case BUILT_IN_VSNPRINTF_CHK:
      if (val[1] && is_gimple_val (val[1]))
	result = gimple_fold_builtin_snprintf_chk (stmt, val[1],
                                                   DECL_FUNCTION_CODE (callee));
      break;

    default:
      gcc_unreachable ();
    }

  if (result && ignore)
    result = fold_ignored_result (result);
  return result;
}

/* Search for a base binfo of BINFO that corresponds to TYPE and return it if
   it is found or NULL_TREE if it is not.  */

static tree
get_base_binfo_for_type (tree binfo, tree type)
{
  int i;
  tree base_binfo;
  tree res = NULL_TREE;

  for (i = 0; BINFO_BASE_ITERATE (binfo, i, base_binfo); i++)
    if (TREE_TYPE (base_binfo) == type)
      {
	gcc_assert (!res);
	res = base_binfo;
      }

  return res;
}

/* Return a binfo describing the part of object referenced by expression REF.
   Return NULL_TREE if it cannot be determined.  REF can consist of a series of
   COMPONENT_REFs of a declaration or of an INDIRECT_REF or it can also be just
   a simple declaration, indirect reference or an SSA_NAME.  If the function
   discovers an INDIRECT_REF or an SSA_NAME, it will assume that the
   encapsulating type is described by KNOWN_BINFO, if it is not NULL_TREE.
   Otherwise the first non-artificial field declaration or the base declaration
   will be examined to get the encapsulating type. */

tree
gimple_get_relevant_ref_binfo (tree ref, tree known_binfo)
{
  while (true)
    {
      if (TREE_CODE (ref) == COMPONENT_REF)
	{
	  tree par_type;
	  tree binfo, base_binfo;
	  tree field = TREE_OPERAND (ref, 1);

	  if (!DECL_ARTIFICIAL (field))
	    {
	      tree type = TREE_TYPE (field);
	      if (TREE_CODE (type) == RECORD_TYPE)
		return TYPE_BINFO (type);
	      else
		return NULL_TREE;
	    }

	  par_type = TREE_TYPE (TREE_OPERAND (ref, 0));
	  binfo = TYPE_BINFO (par_type);
	  if (!binfo
	      || BINFO_N_BASE_BINFOS (binfo) == 0)
	    return NULL_TREE;

	  base_binfo = BINFO_BASE_BINFO (binfo, 0);
	  if (BINFO_TYPE (base_binfo) != TREE_TYPE (field))
	    {
	      tree d_binfo;

	      d_binfo = gimple_get_relevant_ref_binfo (TREE_OPERAND (ref, 0),
						       known_binfo);
	      /* Get descendant binfo. */
	      if (!d_binfo)
		return NULL_TREE;
	      return get_base_binfo_for_type (d_binfo, TREE_TYPE (field));
	    }

	  ref = TREE_OPERAND (ref, 0);
	}
      else if (DECL_P (ref) && TREE_CODE (TREE_TYPE (ref)) == RECORD_TYPE)
	return TYPE_BINFO (TREE_TYPE (ref));
      else if (known_binfo
	       && (TREE_CODE (ref) == SSA_NAME
		   || TREE_CODE (ref) == INDIRECT_REF))
	return known_binfo;
      else
	return NULL_TREE;
    }
}

/* Fold a OBJ_TYPE_REF expression to the address of a function. TOKEN is
   integer form of OBJ_TYPE_REF_TOKEN of the reference expression.  KNOWN_BINFO
   carries the binfo describing the true type of OBJ_TYPE_REF_OBJECT(REF).  */

tree
gimple_fold_obj_type_ref_known_binfo (HOST_WIDE_INT token, tree known_binfo)
{
  HOST_WIDE_INT i;
  tree v, fndecl;

  v = BINFO_VIRTUALS (known_binfo);
  i = 0;
  while (i != token)
    {
      i += (TARGET_VTABLE_USES_DESCRIPTORS
	    ? TARGET_VTABLE_USES_DESCRIPTORS : 1);
      v = TREE_CHAIN (v);
    }

  fndecl = TREE_VALUE (v);
  return build_fold_addr_expr (fndecl);
}


/* Fold a OBJ_TYPE_REF expression to the address of a function.  If KNOWN_TYPE
   is not NULL_TREE, it is the true type of the outmost encapsulating object if
   that comes from a pointer SSA_NAME.  If the true outmost encapsulating type
   can be determined from a declaration OBJ_TYPE_REF_OBJECT(REF), it is used
   regardless of KNOWN_TYPE (which thus can be NULL_TREE).  */

tree
gimple_fold_obj_type_ref (tree ref, tree known_type)
{
  tree obj = OBJ_TYPE_REF_OBJECT (ref);
  tree known_binfo = known_type ? TYPE_BINFO (known_type) : NULL_TREE;
  tree binfo;

  if (TREE_CODE (obj) == ADDR_EXPR)
    obj = TREE_OPERAND (obj, 0);

  binfo = gimple_get_relevant_ref_binfo (obj, known_binfo);
  if (binfo)
    {
      HOST_WIDE_INT token = tree_low_cst (OBJ_TYPE_REF_TOKEN (ref), 1);
      return gimple_fold_obj_type_ref_known_binfo (token, binfo);
    }
  else
    return NULL_TREE;
}

/* Attempt to fold a call statement referenced by the statement iterator GSI.
   The statement may be replaced by another statement, e.g., if the call
   simplifies to a constant value. Return true if any changes were made.
   It is assumed that the operands have been previously folded.  */

static bool
fold_gimple_call (gimple_stmt_iterator *gsi)
{
  gimple stmt = gsi_stmt (*gsi);

  tree callee = gimple_call_fndecl (stmt);

  /* Check for builtins that CCP can handle using information not
     available in the generic fold routines.  */
  if (callee && DECL_BUILT_IN (callee))
    {
      tree result = gimple_fold_builtin (stmt);

      if (result)
	{
          if (!update_call_from_tree (gsi, result))
	    gimplify_and_update_call_from_tree (gsi, result);
	  return true;
	}
    }
  else
    {
      /* ??? Should perhaps do this in fold proper.  However, doing it
         there requires that we create a new CALL_EXPR, and that requires
         copying EH region info to the new node.  Easier to just do it
         here where we can just smash the call operand.  */
      /* ??? Is there a good reason not to do this in fold_stmt_inplace?  */
      callee = gimple_call_fn (stmt);
      if (TREE_CODE (callee) == OBJ_TYPE_REF
          && TREE_CODE (OBJ_TYPE_REF_OBJECT (callee)) == ADDR_EXPR)
        {
          tree t;

          t = gimple_fold_obj_type_ref (callee, NULL_TREE);
          if (t)
            {
              gimple_call_set_fn (stmt, t);
              return true;
            }
        }
    }

  return false;
}

/* Worker for both fold_stmt and fold_stmt_inplace.  The INPLACE argument
   distinguishes both cases.  */

static bool
fold_stmt_1 (gimple_stmt_iterator *gsi, bool inplace)
{
  bool changed = false;
  gimple stmt = gsi_stmt (*gsi);
  unsigned i;

  /* Fold the main computation performed by the statement.  */
  switch (gimple_code (stmt))
    {
    case GIMPLE_ASSIGN:
      {
	unsigned old_num_ops = gimple_num_ops (stmt);
	tree new_rhs = fold_gimple_assign (gsi);
	tree lhs = gimple_assign_lhs (stmt);
	if (new_rhs
	    && !useless_type_conversion_p (TREE_TYPE (lhs),
					   TREE_TYPE (new_rhs)))
	  new_rhs = fold_convert (TREE_TYPE (lhs), new_rhs);
	if (new_rhs
	    && (!inplace
		|| get_gimple_rhs_num_ops (TREE_CODE (new_rhs)) < old_num_ops))
	  {
	    gimple_assign_set_rhs_from_tree (gsi, new_rhs);
	    changed = true;
	  }
	break;
      }

    case GIMPLE_COND:
      changed |= fold_gimple_cond (stmt);
      break;

    case GIMPLE_CALL:
      /* Fold *& in call arguments.  */
      for (i = 0; i < gimple_call_num_args (stmt); ++i)
	if (REFERENCE_CLASS_P (gimple_call_arg (stmt, i)))
	  {
	    tree tmp = maybe_fold_reference (gimple_call_arg (stmt, i), false);
	    if (tmp)
	      {
		gimple_call_set_arg (stmt, i, tmp);
		changed = true;
	      }
	  }
      /* The entire statement may be replaced in this case.  */
      if (!inplace)
	changed |= fold_gimple_call (gsi);
      break;

    case GIMPLE_ASM:
      /* Fold *& in asm operands.  */
      for (i = 0; i < gimple_asm_noutputs (stmt); ++i)
	{
	  tree link = gimple_asm_output_op (stmt, i);
	  tree op = TREE_VALUE (link);
	  if (REFERENCE_CLASS_P (op)
	      && (op = maybe_fold_reference (op, true)) != NULL_TREE)
	    {
	      TREE_VALUE (link) = op;
	      changed = true;
	    }
	}
      for (i = 0; i < gimple_asm_ninputs (stmt); ++i)
	{
	  tree link = gimple_asm_input_op (stmt, i);
	  tree op = TREE_VALUE (link);
	  if (REFERENCE_CLASS_P (op)
	      && (op = maybe_fold_reference (op, false)) != NULL_TREE)
	    {
	      TREE_VALUE (link) = op;
	      changed = true;
	    }
	}
      break;

    default:;
    }

  stmt = gsi_stmt (*gsi);

  /* Fold *& on the lhs.  */
  if (gimple_has_lhs (stmt))
    {
      tree lhs = gimple_get_lhs (stmt);
      if (lhs && REFERENCE_CLASS_P (lhs))
	{
	  tree new_lhs = maybe_fold_reference (lhs, true);
	  if (new_lhs)
	    {
	      gimple_set_lhs (stmt, new_lhs);
	      changed = true;
	    }
	}
    }

  return changed;
}

/* Fold the statement pointed to by GSI.  In some cases, this function may
   replace the whole statement with a new one.  Returns true iff folding
   makes any changes.
   The statement pointed to by GSI should be in valid gimple form but may
   be in unfolded state as resulting from for example constant propagation
   which can produce *&x = 0.  */

bool
fold_stmt (gimple_stmt_iterator *gsi)
{
  return fold_stmt_1 (gsi, false);
}

/* Perform the minimal folding on statement STMT.  Only operations like
   *&x created by constant propagation are handled.  The statement cannot
   be replaced with a new one.  Return true if the statement was
   changed, false otherwise.
   The statement STMT should be in valid gimple form but may
   be in unfolded state as resulting from for example constant propagation
   which can produce *&x = 0.  */

bool
fold_stmt_inplace (gimple stmt)
{
  gimple_stmt_iterator gsi = gsi_for_stmt (stmt);
  bool changed = fold_stmt_1 (&gsi, true);
  gcc_assert (gsi_stmt (gsi) == stmt);
  return changed;
}

/* Canonicalize and possibly invert the boolean EXPR; return NULL_TREE 
   if EXPR is null or we don't know how.
   If non-null, the result always has boolean type.  */

static tree
canonicalize_bool (tree expr, bool invert)
{
  if (!expr)
    return NULL_TREE;
  else if (invert)
    {
      if (integer_nonzerop (expr))
	return boolean_false_node;
      else if (integer_zerop (expr))
	return boolean_true_node;
      else if (TREE_CODE (expr) == SSA_NAME)
	return fold_build2 (EQ_EXPR, boolean_type_node, expr,
			    build_int_cst (TREE_TYPE (expr), 0));
      else if (TREE_CODE_CLASS (TREE_CODE (expr)) == tcc_comparison)
	return fold_build2 (invert_tree_comparison (TREE_CODE (expr), false),
			    boolean_type_node,
			    TREE_OPERAND (expr, 0),
			    TREE_OPERAND (expr, 1));
      else
	return NULL_TREE;
    }
  else
    {
      if (TREE_CODE (TREE_TYPE (expr)) == BOOLEAN_TYPE)
	return expr;
      if (integer_nonzerop (expr))
	return boolean_true_node;
      else if (integer_zerop (expr))
	return boolean_false_node;
      else if (TREE_CODE (expr) == SSA_NAME)
	return fold_build2 (NE_EXPR, boolean_type_node, expr,
			    build_int_cst (TREE_TYPE (expr), 0));
      else if (TREE_CODE_CLASS (TREE_CODE (expr)) == tcc_comparison)
	return fold_build2 (TREE_CODE (expr),
			    boolean_type_node,
			    TREE_OPERAND (expr, 0),
			    TREE_OPERAND (expr, 1));
      else
	return NULL_TREE;
    }
}

/* Check to see if a boolean expression EXPR is logically equivalent to the
   comparison (OP1 CODE OP2).  Check for various identities involving
   SSA_NAMEs.  */

static bool
same_bool_comparison_p (const_tree expr, enum tree_code code,
			const_tree op1, const_tree op2)
{
  gimple s;

  /* The obvious case.  */
  if (TREE_CODE (expr) == code
      && operand_equal_p (TREE_OPERAND (expr, 0), op1, 0)
      && operand_equal_p (TREE_OPERAND (expr, 1), op2, 0))
    return true;

  /* Check for comparing (name, name != 0) and the case where expr
     is an SSA_NAME with a definition matching the comparison.  */
  if (TREE_CODE (expr) == SSA_NAME
      && TREE_CODE (TREE_TYPE (expr)) == BOOLEAN_TYPE)
    {
      if (operand_equal_p (expr, op1, 0))
	return ((code == NE_EXPR && integer_zerop (op2))
		|| (code == EQ_EXPR && integer_nonzerop (op2)));
      s = SSA_NAME_DEF_STMT (expr);
      if (is_gimple_assign (s)
	  && gimple_assign_rhs_code (s) == code
	  && operand_equal_p (gimple_assign_rhs1 (s), op1, 0)
	  && operand_equal_p (gimple_assign_rhs2 (s), op2, 0))
	return true;
    }

  /* If op1 is of the form (name != 0) or (name == 0), and the definition
     of name is a comparison, recurse.  */
  if (TREE_CODE (op1) == SSA_NAME
      && TREE_CODE (TREE_TYPE (op1)) == BOOLEAN_TYPE)
    {
      s = SSA_NAME_DEF_STMT (op1);
      if (is_gimple_assign (s)
	  && TREE_CODE_CLASS (gimple_assign_rhs_code (s)) == tcc_comparison)
	{
	  enum tree_code c = gimple_assign_rhs_code (s);
	  if ((c == NE_EXPR && integer_zerop (op2))
	      || (c == EQ_EXPR && integer_nonzerop (op2)))
	    return same_bool_comparison_p (expr, c,
					   gimple_assign_rhs1 (s),
					   gimple_assign_rhs2 (s));
	  if ((c == EQ_EXPR && integer_zerop (op2))
	      || (c == NE_EXPR && integer_nonzerop (op2)))
	    return same_bool_comparison_p (expr,
					   invert_tree_comparison (c, false),
					   gimple_assign_rhs1 (s),
					   gimple_assign_rhs2 (s));
	}
    }
  return false;
}

/* Check to see if two boolean expressions OP1 and OP2 are logically
   equivalent.  */

static bool
same_bool_result_p (const_tree op1, const_tree op2)
{
  /* Simple cases first.  */
  if (operand_equal_p (op1, op2, 0))
    return true;

  /* Check the cases where at least one of the operands is a comparison.
     These are a bit smarter than operand_equal_p in that they apply some
     identifies on SSA_NAMEs.  */
  if (TREE_CODE_CLASS (TREE_CODE (op2)) == tcc_comparison
      && same_bool_comparison_p (op1, TREE_CODE (op2),
				 TREE_OPERAND (op2, 0),
				 TREE_OPERAND (op2, 1)))
    return true;
  if (TREE_CODE_CLASS (TREE_CODE (op1)) == tcc_comparison
      && same_bool_comparison_p (op2, TREE_CODE (op1),
				 TREE_OPERAND (op1, 0),
				 TREE_OPERAND (op1, 1)))
    return true;

  /* Default case.  */
  return false;
}

/* Forward declarations for some mutually recursive functions.  */

static tree
and_comparisons_1 (enum tree_code code1, tree op1a, tree op1b,
		   enum tree_code code2, tree op2a, tree op2b);
static tree
and_var_with_comparison (tree var, bool invert,
			 enum tree_code code2, tree op2a, tree op2b);
static tree
and_var_with_comparison_1 (gimple stmt, 
			   enum tree_code code2, tree op2a, tree op2b);
static tree
or_comparisons_1 (enum tree_code code1, tree op1a, tree op1b,
		  enum tree_code code2, tree op2a, tree op2b);
static tree
or_var_with_comparison (tree var, bool invert,
			enum tree_code code2, tree op2a, tree op2b);
static tree
or_var_with_comparison_1 (gimple stmt, 
			  enum tree_code code2, tree op2a, tree op2b);

/* Helper function for and_comparisons_1:  try to simplify the AND of the
   ssa variable VAR with the comparison specified by (OP2A CODE2 OP2B).
   If INVERT is true, invert the value of the VAR before doing the AND.
   Return NULL_EXPR if we can't simplify this to a single expression.  */

static tree
and_var_with_comparison (tree var, bool invert,
			 enum tree_code code2, tree op2a, tree op2b)
{
  tree t;
  gimple stmt = SSA_NAME_DEF_STMT (var);

  /* We can only deal with variables whose definitions are assignments.  */
  if (!is_gimple_assign (stmt))
    return NULL_TREE;
  
  /* If we have an inverted comparison, apply DeMorgan's law and rewrite
     !var AND (op2a code2 op2b) => !(var OR !(op2a code2 op2b))
     Then we only have to consider the simpler non-inverted cases.  */
  if (invert)
    t = or_var_with_comparison_1 (stmt, 
				  invert_tree_comparison (code2, false),
				  op2a, op2b);
  else
    t = and_var_with_comparison_1 (stmt, code2, op2a, op2b);
  return canonicalize_bool (t, invert);
}

/* Try to simplify the AND of the ssa variable defined by the assignment
   STMT with the comparison specified by (OP2A CODE2 OP2B).
   Return NULL_EXPR if we can't simplify this to a single expression.  */

static tree
and_var_with_comparison_1 (gimple stmt,
			   enum tree_code code2, tree op2a, tree op2b)
{
  tree var = gimple_assign_lhs (stmt);
  tree true_test_var = NULL_TREE;
  tree false_test_var = NULL_TREE;
  enum tree_code innercode = gimple_assign_rhs_code (stmt);

  /* Check for identities like (var AND (var == 0)) => false.  */
  if (TREE_CODE (op2a) == SSA_NAME
      && TREE_CODE (TREE_TYPE (var)) == BOOLEAN_TYPE)
    {
      if ((code2 == NE_EXPR && integer_zerop (op2b))
	  || (code2 == EQ_EXPR && integer_nonzerop (op2b)))
	{
	  true_test_var = op2a;
	  if (var == true_test_var)
	    return var;
	}
      else if ((code2 == EQ_EXPR && integer_zerop (op2b))
	       || (code2 == NE_EXPR && integer_nonzerop (op2b)))
	{
	  false_test_var = op2a;
	  if (var == false_test_var)
	    return boolean_false_node;
	}
    }

  /* If the definition is a comparison, recurse on it.  */
  if (TREE_CODE_CLASS (innercode) == tcc_comparison)
    {
      tree t = and_comparisons_1 (innercode,
				  gimple_assign_rhs1 (stmt),
				  gimple_assign_rhs2 (stmt),
				  code2,
				  op2a,
				  op2b);
      if (t)
	return t;
    }

  /* If the definition is an AND or OR expression, we may be able to
     simplify by reassociating.  */
  if (innercode == TRUTH_AND_EXPR
      || innercode == TRUTH_OR_EXPR
      || (TREE_CODE (TREE_TYPE (var)) == BOOLEAN_TYPE
	  && (innercode == BIT_AND_EXPR || innercode == BIT_IOR_EXPR)))
    {
      tree inner1 = gimple_assign_rhs1 (stmt);
      tree inner2 = gimple_assign_rhs2 (stmt);
      gimple s;
      tree t;
      tree partial = NULL_TREE;
      bool is_and = (innercode == TRUTH_AND_EXPR || innercode == BIT_AND_EXPR);
      
      /* Check for boolean identities that don't require recursive examination
	 of inner1/inner2:
	 inner1 AND (inner1 AND inner2) => inner1 AND inner2 => var
	 inner1 AND (inner1 OR inner2) => inner1
	 !inner1 AND (inner1 AND inner2) => false
	 !inner1 AND (inner1 OR inner2) => !inner1 AND inner2
         Likewise for similar cases involving inner2.  */
      if (inner1 == true_test_var)
	return (is_and ? var : inner1);
      else if (inner2 == true_test_var)
	return (is_and ? var : inner2);
      else if (inner1 == false_test_var)
	return (is_and
		? boolean_false_node
		: and_var_with_comparison (inner2, false, code2, op2a, op2b));
      else if (inner2 == false_test_var)
	return (is_and
		? boolean_false_node
		: and_var_with_comparison (inner1, false, code2, op2a, op2b));

      /* Next, redistribute/reassociate the AND across the inner tests.
	 Compute the first partial result, (inner1 AND (op2a code op2b))  */
      if (TREE_CODE (inner1) == SSA_NAME
	  && is_gimple_assign (s = SSA_NAME_DEF_STMT (inner1))
	  && TREE_CODE_CLASS (gimple_assign_rhs_code (s)) == tcc_comparison
	  && (t = maybe_fold_and_comparisons (gimple_assign_rhs_code (s),
					      gimple_assign_rhs1 (s),
					      gimple_assign_rhs2 (s),
					      code2, op2a, op2b)))
	{
	  /* Handle the AND case, where we are reassociating:
	     (inner1 AND inner2) AND (op2a code2 op2b)
	     => (t AND inner2)
	     If the partial result t is a constant, we win.  Otherwise
	     continue on to try reassociating with the other inner test.  */
	  if (is_and)
	    {
	      if (integer_onep (t))
		return inner2;
	      else if (integer_zerop (t))
		return boolean_false_node;
	    }

	  /* Handle the OR case, where we are redistributing:
	     (inner1 OR inner2) AND (op2a code2 op2b)
	     => (t OR (inner2 AND (op2a code2 op2b)))  */
	  else
	    {
	      if (integer_onep (t))
		return boolean_true_node;
	      else
		/* Save partial result for later.  */
		partial = t;
	    }
	}
      
      /* Compute the second partial result, (inner2 AND (op2a code op2b)) */
      if (TREE_CODE (inner2) == SSA_NAME
	  && is_gimple_assign (s = SSA_NAME_DEF_STMT (inner2))
	  && TREE_CODE_CLASS (gimple_assign_rhs_code (s)) == tcc_comparison
	  && (t = maybe_fold_and_comparisons (gimple_assign_rhs_code (s),
					      gimple_assign_rhs1 (s),
					      gimple_assign_rhs2 (s),
					      code2, op2a, op2b)))
	{
	  /* Handle the AND case, where we are reassociating:
	     (inner1 AND inner2) AND (op2a code2 op2b)
	     => (inner1 AND t)  */
	  if (is_and)
	    {
	      if (integer_onep (t))
		return inner1;
	      else if (integer_zerop (t))
		return boolean_false_node;
	    }

	  /* Handle the OR case. where we are redistributing:
	     (inner1 OR inner2) AND (op2a code2 op2b)
	     => (t OR (inner1 AND (op2a code2 op2b)))
	     => (t OR partial)  */
	  else
	    {
	      if (integer_onep (t))
		return boolean_true_node;
	      else if (partial)
		{
		  /* We already got a simplification for the other
		     operand to the redistributed OR expression.  The
		     interesting case is when at least one is false.
		     Or, if both are the same, we can apply the identity
		     (x OR x) == x.  */
		  if (integer_zerop (partial))
		    return t;
		  else if (integer_zerop (t))
		    return partial;
		  else if (same_bool_result_p (t, partial))
		    return t;
		}
	    }
	}
    }
  return NULL_TREE;
}

/* Try to simplify the AND of two comparisons defined by
   (OP1A CODE1 OP1B) and (OP2A CODE2 OP2B), respectively.
   If this can be done without constructing an intermediate value,
   return the resulting tree; otherwise NULL_TREE is returned.
   This function is deliberately asymmetric as it recurses on SSA_DEFs
   in the first comparison but not the second.  */

static tree
and_comparisons_1 (enum tree_code code1, tree op1a, tree op1b,
		   enum tree_code code2, tree op2a, tree op2b)
{
  /* First check for ((x CODE1 y) AND (x CODE2 y)).  */
  if (operand_equal_p (op1a, op2a, 0)
      && operand_equal_p (op1b, op2b, 0))
    {
      tree t = combine_comparisons (UNKNOWN_LOCATION,
				    TRUTH_ANDIF_EXPR, code1, code2,
				    boolean_type_node, op1a, op1b);
      if (t)
	return t;
    }

  /* Likewise the swapped case of the above.  */
  if (operand_equal_p (op1a, op2b, 0)
      && operand_equal_p (op1b, op2a, 0))
    {
      tree t = combine_comparisons (UNKNOWN_LOCATION,
				    TRUTH_ANDIF_EXPR, code1,
				    swap_tree_comparison (code2),
				    boolean_type_node, op1a, op1b);
      if (t)
	return t;
    }

  /* If both comparisons are of the same value against constants, we might
     be able to merge them.  */
  if (operand_equal_p (op1a, op2a, 0)
      && TREE_CODE (op1b) == INTEGER_CST
      && TREE_CODE (op2b) == INTEGER_CST)
    {
      int cmp = tree_int_cst_compare (op1b, op2b);

      /* If we have (op1a == op1b), we should either be able to
	 return that or FALSE, depending on whether the constant op1b
	 also satisfies the other comparison against op2b.  */
      if (code1 == EQ_EXPR)
	{
	  bool done = true;
	  bool val;
	  switch (code2)
	    {
	    case EQ_EXPR: val = (cmp == 0); break;
	    case NE_EXPR: val = (cmp != 0); break;
	    case LT_EXPR: val = (cmp < 0); break;
	    case GT_EXPR: val = (cmp > 0); break;
	    case LE_EXPR: val = (cmp <= 0); break;
	    case GE_EXPR: val = (cmp >= 0); break;
	    default: done = false;
	    }
	  if (done)
	    {
	      if (val)
		return fold_build2 (code1, boolean_type_node, op1a, op1b);
	      else
		return boolean_false_node;
	    }
	}
      /* Likewise if the second comparison is an == comparison.  */
      else if (code2 == EQ_EXPR)
	{
	  bool done = true;
	  bool val;
	  switch (code1)
	    {
	    case EQ_EXPR: val = (cmp == 0); break;
	    case NE_EXPR: val = (cmp != 0); break;
	    case LT_EXPR: val = (cmp > 0); break;
	    case GT_EXPR: val = (cmp < 0); break;
	    case LE_EXPR: val = (cmp >= 0); break;
	    case GE_EXPR: val = (cmp <= 0); break;
	    default: done = false;
	    }
	  if (done)
	    {
	      if (val)
		return fold_build2 (code2, boolean_type_node, op2a, op2b);
	      else
		return boolean_false_node;
	    }
	}

      /* Same business with inequality tests.  */
      else if (code1 == NE_EXPR)
	{
	  bool val;
	  switch (code2)
	    {
	    case EQ_EXPR: val = (cmp != 0); break;
	    case NE_EXPR: val = (cmp == 0); break;
	    case LT_EXPR: val = (cmp >= 0); break;
	    case GT_EXPR: val = (cmp <= 0); break;
	    case LE_EXPR: val = (cmp > 0); break;
	    case GE_EXPR: val = (cmp < 0); break;
	    default:
	      val = false;
	    }
	  if (val)
	    return fold_build2 (code2, boolean_type_node, op2a, op2b);
	}
      else if (code2 == NE_EXPR)
	{
	  bool val;
	  switch (code1)
	    {
	    case EQ_EXPR: val = (cmp == 0); break;
	    case NE_EXPR: val = (cmp != 0); break;
	    case LT_EXPR: val = (cmp <= 0); break;
	    case GT_EXPR: val = (cmp >= 0); break;
	    case LE_EXPR: val = (cmp < 0); break;
	    case GE_EXPR: val = (cmp > 0); break;
	    default:
	      val = false;
	    }
	  if (val)
	    return fold_build2 (code1, boolean_type_node, op1a, op1b);
	}

      /* Chose the more restrictive of two < or <= comparisons.  */
      else if ((code1 == LT_EXPR || code1 == LE_EXPR)
	       && (code2 == LT_EXPR || code2 == LE_EXPR))
	{
	  if ((cmp < 0) || (cmp == 0 && code1 == LT_EXPR))
	    return fold_build2 (code1, boolean_type_node, op1a, op1b);
	  else
	    return fold_build2 (code2, boolean_type_node, op2a, op2b);
	}

      /* Likewise chose the more restrictive of two > or >= comparisons.  */
      else if ((code1 == GT_EXPR || code1 == GE_EXPR)
	       && (code2 == GT_EXPR || code2 == GE_EXPR))
	{
	  if ((cmp > 0) || (cmp == 0 && code1 == GT_EXPR))
	    return fold_build2 (code1, boolean_type_node, op1a, op1b);
	  else
	    return fold_build2 (code2, boolean_type_node, op2a, op2b);
	}

      /* Check for singleton ranges.  */
      else if (cmp == 0
	       && ((code1 == LE_EXPR && code2 == GE_EXPR)
		   || (code1 == GE_EXPR && code2 == LE_EXPR)))
	return fold_build2 (EQ_EXPR, boolean_type_node, op1a, op2b);

      /* Check for disjoint ranges. */
      else if (cmp <= 0
	       && (code1 == LT_EXPR || code1 == LE_EXPR)
	       && (code2 == GT_EXPR || code2 == GE_EXPR))
	return boolean_false_node;
      else if (cmp >= 0
	       && (code1 == GT_EXPR || code1 == GE_EXPR)
	       && (code2 == LT_EXPR || code2 == LE_EXPR))
	return boolean_false_node;
    }

  /* Perhaps the first comparison is (NAME != 0) or (NAME == 1) where
     NAME's definition is a truth value.  See if there are any simplifications
     that can be done against the NAME's definition.  */
  if (TREE_CODE (op1a) == SSA_NAME
      && (code1 == NE_EXPR || code1 == EQ_EXPR)
      && (integer_zerop (op1b) || integer_onep (op1b)))
    {
      bool invert = ((code1 == EQ_EXPR && integer_zerop (op1b))
		     || (code1 == NE_EXPR && integer_onep (op1b)));
      gimple stmt = SSA_NAME_DEF_STMT (op1a);
      switch (gimple_code (stmt))
	{
	case GIMPLE_ASSIGN:
	  /* Try to simplify by copy-propagating the definition.  */
	  return and_var_with_comparison (op1a, invert, code2, op2a, op2b);

	case GIMPLE_PHI:
	  /* If every argument to the PHI produces the same result when
	     ANDed with the second comparison, we win.
	     Do not do this unless the type is bool since we need a bool
	     result here anyway.  */
	  if (TREE_CODE (TREE_TYPE (op1a)) == BOOLEAN_TYPE)
	    {
	      tree result = NULL_TREE;
	      unsigned i;
	      for (i = 0; i < gimple_phi_num_args (stmt); i++)
		{
		  tree arg = gimple_phi_arg_def (stmt, i);
		  
		  /* If this PHI has itself as an argument, ignore it.
		     If all the other args produce the same result,
		     we're still OK.  */
		  if (arg == gimple_phi_result (stmt))
		    continue;
		  else if (TREE_CODE (arg) == INTEGER_CST)
		    {
		      if (invert ? integer_nonzerop (arg) : integer_zerop (arg))
			{
			  if (!result)
			    result = boolean_false_node;
			  else if (!integer_zerop (result))
			    return NULL_TREE;
			}
		      else if (!result)
			result = fold_build2 (code2, boolean_type_node,
					      op2a, op2b);
		      else if (!same_bool_comparison_p (result,
							code2, op2a, op2b))
			return NULL_TREE;
		    }
		  else if (TREE_CODE (arg) == SSA_NAME)
		    {
		      tree temp = and_var_with_comparison (arg, invert,
							   code2, op2a, op2b);
		      if (!temp)
			return NULL_TREE;
		      else if (!result)
			result = temp;
		      else if (!same_bool_result_p (result, temp))
			return NULL_TREE;
		    }
		  else
		    return NULL_TREE;
		}
	      return result;
	    }

	default:
	  break;
	}
    }
  return NULL_TREE;
}

/* Try to simplify the AND of two comparisons, specified by
   (OP1A CODE1 OP1B) and (OP2B CODE2 OP2B), respectively.
   If this can be simplified to a single expression (without requiring
   introducing more SSA variables to hold intermediate values),
   return the resulting tree.  Otherwise return NULL_TREE.
   If the result expression is non-null, it has boolean type.  */

tree
maybe_fold_and_comparisons (enum tree_code code1, tree op1a, tree op1b,
			    enum tree_code code2, tree op2a, tree op2b)
{
  tree t = and_comparisons_1 (code1, op1a, op1b, code2, op2a, op2b);
  if (t)
    return t;
  else
    return and_comparisons_1 (code2, op2a, op2b, code1, op1a, op1b);
}

/* Helper function for or_comparisons_1:  try to simplify the OR of the
   ssa variable VAR with the comparison specified by (OP2A CODE2 OP2B).
   If INVERT is true, invert the value of VAR before doing the OR.
   Return NULL_EXPR if we can't simplify this to a single expression.  */

static tree
or_var_with_comparison (tree var, bool invert,
			enum tree_code code2, tree op2a, tree op2b)
{
  tree t;
  gimple stmt = SSA_NAME_DEF_STMT (var);

  /* We can only deal with variables whose definitions are assignments.  */
  if (!is_gimple_assign (stmt))
    return NULL_TREE;
  
  /* If we have an inverted comparison, apply DeMorgan's law and rewrite
     !var OR (op2a code2 op2b) => !(var AND !(op2a code2 op2b))
     Then we only have to consider the simpler non-inverted cases.  */
  if (invert)
    t = and_var_with_comparison_1 (stmt, 
				   invert_tree_comparison (code2, false),
				   op2a, op2b);
  else
    t = or_var_with_comparison_1 (stmt, code2, op2a, op2b);
  return canonicalize_bool (t, invert);
}

/* Try to simplify the OR of the ssa variable defined by the assignment
   STMT with the comparison specified by (OP2A CODE2 OP2B).
   Return NULL_EXPR if we can't simplify this to a single expression.  */

static tree
or_var_with_comparison_1 (gimple stmt,
			  enum tree_code code2, tree op2a, tree op2b)
{
  tree var = gimple_assign_lhs (stmt);
  tree true_test_var = NULL_TREE;
  tree false_test_var = NULL_TREE;
  enum tree_code innercode = gimple_assign_rhs_code (stmt);

  /* Check for identities like (var OR (var != 0)) => true .  */
  if (TREE_CODE (op2a) == SSA_NAME
      && TREE_CODE (TREE_TYPE (var)) == BOOLEAN_TYPE)
    {
      if ((code2 == NE_EXPR && integer_zerop (op2b))
	  || (code2 == EQ_EXPR && integer_nonzerop (op2b)))
	{
	  true_test_var = op2a;
	  if (var == true_test_var)
	    return var;
	}
      else if ((code2 == EQ_EXPR && integer_zerop (op2b))
	       || (code2 == NE_EXPR && integer_nonzerop (op2b)))
	{
	  false_test_var = op2a;
	  if (var == false_test_var)
	    return boolean_true_node;
	}
    }

  /* If the definition is a comparison, recurse on it.  */
  if (TREE_CODE_CLASS (innercode) == tcc_comparison)
    {
      tree t = or_comparisons_1 (innercode,
				 gimple_assign_rhs1 (stmt),
				 gimple_assign_rhs2 (stmt),
				 code2,
				 op2a,
				 op2b);
      if (t)
	return t;
    }
  
  /* If the definition is an AND or OR expression, we may be able to
     simplify by reassociating.  */
  if (innercode == TRUTH_AND_EXPR
      || innercode == TRUTH_OR_EXPR
      || (TREE_CODE (TREE_TYPE (var)) == BOOLEAN_TYPE
	  && (innercode == BIT_AND_EXPR || innercode == BIT_IOR_EXPR)))
    {
      tree inner1 = gimple_assign_rhs1 (stmt);
      tree inner2 = gimple_assign_rhs2 (stmt);
      gimple s;
      tree t;
      tree partial = NULL_TREE;
      bool is_or = (innercode == TRUTH_OR_EXPR || innercode == BIT_IOR_EXPR);
      
      /* Check for boolean identities that don't require recursive examination
	 of inner1/inner2:
	 inner1 OR (inner1 OR inner2) => inner1 OR inner2 => var
	 inner1 OR (inner1 AND inner2) => inner1
	 !inner1 OR (inner1 OR inner2) => true
	 !inner1 OR (inner1 AND inner2) => !inner1 OR inner2
      */
      if (inner1 == true_test_var)
	return (is_or ? var : inner1);
      else if (inner2 == true_test_var)
	return (is_or ? var : inner2);
      else if (inner1 == false_test_var)
	return (is_or
		? boolean_true_node
		: or_var_with_comparison (inner2, false, code2, op2a, op2b));
      else if (inner2 == false_test_var)
	return (is_or
		? boolean_true_node
		: or_var_with_comparison (inner1, false, code2, op2a, op2b));
      
      /* Next, redistribute/reassociate the OR across the inner tests.
	 Compute the first partial result, (inner1 OR (op2a code op2b))  */
      if (TREE_CODE (inner1) == SSA_NAME
	  && is_gimple_assign (s = SSA_NAME_DEF_STMT (inner1))
	  && TREE_CODE_CLASS (gimple_assign_rhs_code (s)) == tcc_comparison
	  && (t = maybe_fold_or_comparisons (gimple_assign_rhs_code (s),
					     gimple_assign_rhs1 (s),
					     gimple_assign_rhs2 (s),
					     code2, op2a, op2b)))
	{
	  /* Handle the OR case, where we are reassociating:
	     (inner1 OR inner2) OR (op2a code2 op2b)
	     => (t OR inner2)
	     If the partial result t is a constant, we win.  Otherwise
	     continue on to try reassociating with the other inner test.  */
	  if (innercode == TRUTH_OR_EXPR)
	    {
	      if (integer_onep (t))
		return boolean_true_node;
	      else if (integer_zerop (t))
		return inner2;
	    }
	  
	  /* Handle the AND case, where we are redistributing:
	     (inner1 AND inner2) OR (op2a code2 op2b)
	     => (t AND (inner2 OR (op2a code op2b)))  */
	  else
	    {
	      if (integer_zerop (t))
		return boolean_false_node;
	      else
		/* Save partial result for later.  */
		partial = t;
	    }
	}
      
      /* Compute the second partial result, (inner2 OR (op2a code op2b)) */
      if (TREE_CODE (inner2) == SSA_NAME
	  && is_gimple_assign (s = SSA_NAME_DEF_STMT (inner2))
	  && TREE_CODE_CLASS (gimple_assign_rhs_code (s)) == tcc_comparison
	  && (t = maybe_fold_or_comparisons (gimple_assign_rhs_code (s),
					     gimple_assign_rhs1 (s),
					     gimple_assign_rhs2 (s),
					     code2, op2a, op2b)))
	{
	  /* Handle the OR case, where we are reassociating:
	     (inner1 OR inner2) OR (op2a code2 op2b)
	     => (inner1 OR t)  */
	  if (innercode == TRUTH_OR_EXPR)
	    {
	      if (integer_zerop (t))
		return inner1;
	      else if (integer_onep (t))
		return boolean_true_node;
	    }
	  
	  /* Handle the AND case, where we are redistributing:
	     (inner1 AND inner2) OR (op2a code2 op2b)
	     => (t AND (inner1 OR (op2a code2 op2b)))
	     => (t AND partial)  */
	  else 
	    {
	      if (integer_zerop (t))
		return boolean_false_node;
	      else if (partial)
		{
		  /* We already got a simplification for the other
		     operand to the redistributed AND expression.  The
		     interesting case is when at least one is true.
		     Or, if both are the same, we can apply the identity
		     (x AND x) == true.  */
		  if (integer_onep (partial))
		    return t;
		  else if (integer_onep (t))
		    return partial;
		  else if (same_bool_result_p (t, partial))
		    return boolean_true_node;
		}
	    }
	}
    }
  return NULL_TREE;
}

/* Try to simplify the OR of two comparisons defined by
   (OP1A CODE1 OP1B) and (OP2A CODE2 OP2B), respectively.
   If this can be done without constructing an intermediate value,
   return the resulting tree; otherwise NULL_TREE is returned.
   This function is deliberately asymmetric as it recurses on SSA_DEFs
   in the first comparison but not the second.  */

static tree
or_comparisons_1 (enum tree_code code1, tree op1a, tree op1b,
		  enum tree_code code2, tree op2a, tree op2b)
{
  /* First check for ((x CODE1 y) OR (x CODE2 y)).  */
  if (operand_equal_p (op1a, op2a, 0)
      && operand_equal_p (op1b, op2b, 0))
    {
      tree t = combine_comparisons (UNKNOWN_LOCATION,
				    TRUTH_ORIF_EXPR, code1, code2,
				    boolean_type_node, op1a, op1b);
      if (t)
	return t;
    }

  /* Likewise the swapped case of the above.  */
  if (operand_equal_p (op1a, op2b, 0)
      && operand_equal_p (op1b, op2a, 0))
    {
      tree t = combine_comparisons (UNKNOWN_LOCATION,
				    TRUTH_ORIF_EXPR, code1,
				    swap_tree_comparison (code2),
				    boolean_type_node, op1a, op1b);
      if (t)
	return t;
    }

  /* If both comparisons are of the same value against constants, we might
     be able to merge them.  */
  if (operand_equal_p (op1a, op2a, 0)
      && TREE_CODE (op1b) == INTEGER_CST
      && TREE_CODE (op2b) == INTEGER_CST)
    {
      int cmp = tree_int_cst_compare (op1b, op2b);

      /* If we have (op1a != op1b), we should either be able to
	 return that or TRUE, depending on whether the constant op1b
	 also satisfies the other comparison against op2b.  */
      if (code1 == NE_EXPR)
	{
	  bool done = true;
	  bool val;
	  switch (code2)
	    {
	    case EQ_EXPR: val = (cmp == 0); break;
	    case NE_EXPR: val = (cmp != 0); break;
	    case LT_EXPR: val = (cmp < 0); break;
	    case GT_EXPR: val = (cmp > 0); break;
	    case LE_EXPR: val = (cmp <= 0); break;
	    case GE_EXPR: val = (cmp >= 0); break;
	    default: done = false;
	    }
	  if (done)
	    {
	      if (val)
		return boolean_true_node;
	      else
		return fold_build2 (code1, boolean_type_node, op1a, op1b);
	    }
	}
      /* Likewise if the second comparison is a != comparison.  */
      else if (code2 == NE_EXPR)
	{
	  bool done = true;
	  bool val;
	  switch (code1)
	    {
	    case EQ_EXPR: val = (cmp == 0); break;
	    case NE_EXPR: val = (cmp != 0); break;
	    case LT_EXPR: val = (cmp > 0); break;
	    case GT_EXPR: val = (cmp < 0); break;
	    case LE_EXPR: val = (cmp >= 0); break;
	    case GE_EXPR: val = (cmp <= 0); break;
	    default: done = false;
	    }
	  if (done)
	    {
	      if (val)
		return boolean_true_node;
	      else
		return fold_build2 (code2, boolean_type_node, op2a, op2b);
	    }
	}

      /* See if an equality test is redundant with the other comparison.  */
      else if (code1 == EQ_EXPR)
	{
	  bool val;
	  switch (code2)
	    {
	    case EQ_EXPR: val = (cmp == 0); break;
	    case NE_EXPR: val = (cmp != 0); break;
	    case LT_EXPR: val = (cmp < 0); break;
	    case GT_EXPR: val = (cmp > 0); break;
	    case LE_EXPR: val = (cmp <= 0); break;
	    case GE_EXPR: val = (cmp >= 0); break;
	    default:
	      val = false;
	    }
	  if (val)
	    return fold_build2 (code2, boolean_type_node, op2a, op2b);
	}
      else if (code2 == EQ_EXPR)
	{
	  bool val;
	  switch (code1)
	    {
	    case EQ_EXPR: val = (cmp == 0); break;
	    case NE_EXPR: val = (cmp != 0); break;
	    case LT_EXPR: val = (cmp > 0); break;
	    case GT_EXPR: val = (cmp < 0); break;
	    case LE_EXPR: val = (cmp >= 0); break;
	    case GE_EXPR: val = (cmp <= 0); break;
	    default:
	      val = false;
	    }
	  if (val)
	    return fold_build2 (code1, boolean_type_node, op1a, op1b);
	}

      /* Chose the less restrictive of two < or <= comparisons.  */
      else if ((code1 == LT_EXPR || code1 == LE_EXPR)
	       && (code2 == LT_EXPR || code2 == LE_EXPR))
	{
	  if ((cmp < 0) || (cmp == 0 && code1 == LT_EXPR))
	    return fold_build2 (code2, boolean_type_node, op2a, op2b);
	  else
	    return fold_build2 (code1, boolean_type_node, op1a, op1b);
	}

      /* Likewise chose the less restrictive of two > or >= comparisons.  */
      else if ((code1 == GT_EXPR || code1 == GE_EXPR)
	       && (code2 == GT_EXPR || code2 == GE_EXPR))
	{
	  if ((cmp > 0) || (cmp == 0 && code1 == GT_EXPR))
	    return fold_build2 (code2, boolean_type_node, op2a, op2b);
	  else
	    return fold_build2 (code1, boolean_type_node, op1a, op1b);
	}

      /* Check for singleton ranges.  */
      else if (cmp == 0
	       && ((code1 == LT_EXPR && code2 == GT_EXPR)
		   || (code1 == GT_EXPR && code2 == LT_EXPR)))
	return fold_build2 (NE_EXPR, boolean_type_node, op1a, op2b);

      /* Check for less/greater pairs that don't restrict the range at all.  */
      else if (cmp >= 0
	       && (code1 == LT_EXPR || code1 == LE_EXPR)
	       && (code2 == GT_EXPR || code2 == GE_EXPR))
	return boolean_true_node;
      else if (cmp <= 0
	       && (code1 == GT_EXPR || code1 == GE_EXPR)
	       && (code2 == LT_EXPR || code2 == LE_EXPR))
	return boolean_true_node;
    }

  /* Perhaps the first comparison is (NAME != 0) or (NAME == 1) where
     NAME's definition is a truth value.  See if there are any simplifications
     that can be done against the NAME's definition.  */
  if (TREE_CODE (op1a) == SSA_NAME
      && (code1 == NE_EXPR || code1 == EQ_EXPR)
      && (integer_zerop (op1b) || integer_onep (op1b)))
    {
      bool invert = ((code1 == EQ_EXPR && integer_zerop (op1b))
		     || (code1 == NE_EXPR && integer_onep (op1b)));
      gimple stmt = SSA_NAME_DEF_STMT (op1a);
      switch (gimple_code (stmt))
	{
	case GIMPLE_ASSIGN:
	  /* Try to simplify by copy-propagating the definition.  */
	  return or_var_with_comparison (op1a, invert, code2, op2a, op2b);

	case GIMPLE_PHI:
	  /* If every argument to the PHI produces the same result when
	     ORed with the second comparison, we win.
	     Do not do this unless the type is bool since we need a bool
	     result here anyway.  */
	  if (TREE_CODE (TREE_TYPE (op1a)) == BOOLEAN_TYPE)
	    {
	      tree result = NULL_TREE;
	      unsigned i;
	      for (i = 0; i < gimple_phi_num_args (stmt); i++)
		{
		  tree arg = gimple_phi_arg_def (stmt, i);
		  
		  /* If this PHI has itself as an argument, ignore it.
		     If all the other args produce the same result,
		     we're still OK.  */
		  if (arg == gimple_phi_result (stmt))
		    continue;
		  else if (TREE_CODE (arg) == INTEGER_CST)
		    {
		      if (invert ? integer_zerop (arg) : integer_nonzerop (arg))
			{
			  if (!result)
			    result = boolean_true_node;
			  else if (!integer_onep (result))
			    return NULL_TREE;
			}
		      else if (!result)
			result = fold_build2 (code2, boolean_type_node,
					      op2a, op2b);
		      else if (!same_bool_comparison_p (result,
							code2, op2a, op2b))
			return NULL_TREE;
		    }
		  else if (TREE_CODE (arg) == SSA_NAME)
		    {
		      tree temp = or_var_with_comparison (arg, invert,
							  code2, op2a, op2b);
		      if (!temp)
			return NULL_TREE;
		      else if (!result)
			result = temp;
		      else if (!same_bool_result_p (result, temp))
			return NULL_TREE;
		    }
		  else
		    return NULL_TREE;
		}
	      return result;
	    }

	default:
	  break;
	}
    }
  return NULL_TREE;
}

/* Try to simplify the OR of two comparisons, specified by
   (OP1A CODE1 OP1B) and (OP2B CODE2 OP2B), respectively.
   If this can be simplified to a single expression (without requiring
   introducing more SSA variables to hold intermediate values),
   return the resulting tree.  Otherwise return NULL_TREE.
   If the result expression is non-null, it has boolean type.  */

tree
maybe_fold_or_comparisons (enum tree_code code1, tree op1a, tree op1b,
			   enum tree_code code2, tree op2a, tree op2b)
{
  tree t = or_comparisons_1 (code1, op1a, op1b, code2, op2a, op2b);
  if (t)
    return t;
  else
    return or_comparisons_1 (code2, op2a, op2b, code1, op1a, op1b);
}
