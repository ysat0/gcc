/* Mudflap: narrow-pointer bounds-checking by tree rewriting.
   Copyright (C) 2002, 2003, 2004 Free Software Foundation, Inc.
   Contributed by Frank Ch. Eigler <fche@redhat.com>
   and Graydon Hoare <graydon@redhat.com>

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
Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */


#include "config.h"
#include "errors.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "hard-reg-set.h"
#include "rtl.h"
#include "tree.h"
#include "tm_p.h"
#include "basic-block.h"
#include "flags.h"
#include "function.h"
#include "tree-inline.h"
#include "tree-gimple.h"
#include "tree-flow.h"
#include "tree-mudflap.h"
#include "tree-dump.h"
#include "tree-pass.h"
#include "hashtab.h"
#include "diagnostic.h"
#include <demangle.h>
#include "langhooks.h"
#include "ggc.h"

/* Internal function decls */

/* Helpers.  */
static tree mf_build_string (const char *string);
static tree mf_varname_tree (tree);
static tree mf_file_function_line_tree (location_t *);

/* Initialization of all the mf-runtime.h extern decls.  */
static void mf_init_extern_trees (void);

/* Indirection-related instrumentation.  */
static void mf_decl_cache_locals (void);
static void mf_decl_clear_locals (void);
static void mf_xform_derefs (void);
static void execute_mudflap_function_ops (void);

/* Addressable variables instrumentation.  */
static void mf_xform_decls (tree, tree);
static tree mx_xfn_xform_decls (tree *, int *, void *);
static void mx_register_decls (tree, tree *);
static void execute_mudflap_function_decls (void);


/* ------------------------------------------------------------------------ */
/* Some generally helpful functions for mudflap instrumentation.  */

/* Build a reference to a literal string.  */
static tree
mf_build_string (const char *string)
{
  size_t len = strlen (string);
  tree result = mf_mark (build_string (len + 1, string));

  TREE_TYPE (result)
      = build_array_type (char_type_node,
                          build_index_type (build_int_2 (len, 0)));
  TREE_CONSTANT (result) = 1;
  TREE_INVARIANT (result) = 1;
  TREE_READONLY (result) = 1;
  TREE_STATIC (result) = 1;

  result = build1 (ADDR_EXPR, build_pointer_type (char_type_node), result);

  return mf_mark (result);
}

/* Create a properly typed STRING_CST node that describes the given
   declaration.  It will be used as an argument for __mf_register().
   Try to construct a helpful string, including file/function/variable
   name.  */

static tree
mf_varname_tree (tree decl)
{
  static pretty_printer buf_rec;
  static int initialized = 0;
  pretty_printer *buf = & buf_rec;
  const char *buf_contents;
  tree result;

  if (decl == NULL_TREE)
    abort ();

  if (!initialized)
    {
      pp_construct (buf, /* prefix */ NULL, /* line-width */ 0);
      initialized = 1;
    }
  pp_clear_output_area (buf);

  /* Add FILENAME[:LINENUMBER].  */
  {
    const char *sourcefile;
    unsigned sourceline;

    sourcefile = DECL_SOURCE_FILE (decl);
    if (sourcefile == NULL && current_function_decl != NULL_TREE)
      sourcefile = DECL_SOURCE_FILE (current_function_decl);
    if (sourcefile == NULL)
      sourcefile = "<unknown file>";

    pp_string (buf, sourcefile);

    sourceline = DECL_SOURCE_LINE (decl);
    if (sourceline != 0)
      {
        pp_string (buf, ":");
        pp_decimal_int (buf, sourceline);
      }
  }

  if (current_function_decl != NULL_TREE)
    {
      /* Add (FUNCTION): */
      pp_string (buf, " (");
      {
        const char *funcname = NULL;
        if (DECL_NAME (current_function_decl))
          funcname = lang_hooks.decl_printable_name (current_function_decl, 1);
        if (funcname == NULL)
          funcname = "anonymous fn";

        pp_string (buf, funcname);
      }
      pp_string (buf, ") ");
    }
  else
    pp_string (buf, " ");

  /* Add <variable-declaration>, possibly demangled.  */
  {
    const char *declname = NULL;

    if (strcmp ("GNU C++", lang_hooks.name) == 0 &&
        DECL_NAME (decl) != NULL)
      {
        /* The gcc/cp decl_printable_name hook doesn't do as good a job as
           the libiberty demangler.  */
        declname = cplus_demangle (IDENTIFIER_POINTER (DECL_NAME (decl)),
                                   DMGL_AUTO | DMGL_VERBOSE);
      }

    if (declname == NULL)
      declname = lang_hooks.decl_printable_name (decl, 3);

    if (declname == NULL)
      declname = "<unnamed variable>";

    pp_string (buf, declname);
  }

  /* Return the lot as a new STRING_CST.  */
  buf_contents = pp_base_formatted_text (buf);
  result = mf_build_string (buf_contents);
  pp_clear_output_area (buf);

  return result;
}


/* And another friend, for producing a simpler message.  */

static tree
mf_file_function_line_tree (location_t *locus)
{
  const char *file = NULL, *colon, *line, *op, *name, *cp;
  char linebuf[18];
  char *string;
  tree result;

  /* Add FILENAME.  */
  if (locus != NULL)
    file = locus->file;
  if (file == NULL && current_function_decl != NULL_TREE)
    file = DECL_SOURCE_FILE (current_function_decl);
  if (file == NULL)
    file = "<unknown file>";

  /* Add :LINENUMBER.  */
  if (locus != NULL && locus->line > 0)
    {
      sprintf (linebuf, "%d", locus->line);
      colon = ":";
      line = linebuf;
    }
  else
    colon = line = "";

  /* Add (FUNCTION).  */
  name = lang_hooks.decl_printable_name (current_function_decl, 1);
  if (name)
    {
      op = " (";
      cp = ")";
    }
  else
    op = name = cp = "";

  string = concat (file, colon, line, op, name, cp, NULL);
  result = mf_build_string (string);
  free (string);

  return result;
}


/* global tree nodes */

/* Global tree objects for global variables and functions exported by
   mudflap runtime library.  mf_init_extern_trees must be called
   before using these.  */

/* uintptr_t (usually "unsigned long") */
static GTY (()) tree mf_uintptr_type;

/* struct __mf_cache { uintptr_t low; uintptr_t high; }; */
static GTY (()) tree mf_cache_struct_type;

/* struct __mf_cache * const */
static GTY (()) tree mf_cache_structptr_type;

/* extern struct __mf_cache __mf_lookup_cache []; */
static GTY (()) tree mf_cache_array_decl;

/* extern const unsigned char __mf_lc_shift; */
static GTY (()) tree mf_cache_shift_decl;

/* extern const uintptr_t __mf_lc_mask; */
static GTY (()) tree mf_cache_mask_decl;

/* Their function-scope local shadows, used in single-threaded mode only. */

/* auto const unsigned char __mf_lc_shift_l; */
static GTY (()) tree mf_cache_shift_decl_l;

/* auto const uintptr_t __mf_lc_mask_l; */
static GTY (()) tree mf_cache_mask_decl_l;

/* extern void __mf_check (void *ptr, size_t sz, int type, const char *); */
static GTY (()) tree mf_check_fndecl;

/* extern void __mf_register (void *ptr, size_t sz, int type, const char *); */
static GTY (()) tree mf_register_fndecl;

/* extern void __mf_unregister (void *ptr, size_t sz); */
static GTY (()) tree mf_unregister_fndecl;


/* Initialize the global tree nodes that correspond to mf-runtime.h
   declarations.  */
static void
mf_init_extern_trees (void)
{
  static bool done = false;

  if (done)
    return;
  done = true;

  mf_uintptr_type = TREE_TYPE (mflang_lookup_decl ("uintptr_t"));
  mf_cache_array_decl = mf_mark (mflang_lookup_decl ("__mf_lookup_cache"));
  mf_cache_struct_type = TREE_TYPE (TREE_TYPE (mf_cache_array_decl));
  mf_cache_structptr_type = build_pointer_type (mf_cache_struct_type);
  mf_cache_shift_decl = mf_mark (mflang_lookup_decl ("__mf_lc_shift"));
  mf_cache_mask_decl = mf_mark (mflang_lookup_decl ("__mf_lc_mask"));
  mf_check_fndecl = mflang_lookup_decl ("__mf_check");
  mf_register_fndecl = mflang_lookup_decl ("__mf_register");
  mf_unregister_fndecl = mflang_lookup_decl ("__mf_unregister");
}


/* ------------------------------------------------------------------------ */
/* Memory reference transforms. Perform the mudflap indirection-related
   tree transforms on the current function.

   This is the second part of the mudflap instrumentation.  It works on
   low-level GIMPLE using the CFG, because we want to run this pass after
   tree optimizations have been performed, but we have to preserve the CFG
   for expansion from trees to RTL.  */

static void
execute_mudflap_function_ops (void)
{
  if (mf_marked_p (current_function_decl))
    return;

  push_gimplify_context ();

  /* In multithreaded mode, don't cache the lookup cache parameters.  */
  if (! flag_mudflap_threads)
    mf_decl_cache_locals ();

  mf_xform_derefs ();

  if (! flag_mudflap_threads)
    mf_decl_clear_locals ();

  pop_gimplify_context (NULL);
}

/* Create and initialize local shadow variables for the lookup cache
   globals.  Put their decls in the *_l globals for use by
   mf_build_check_statement_for. */

static void
mf_decl_cache_locals (void)
{
  tree t, shift_init_stmts, mask_init_stmts;
  tree_stmt_iterator tsi;

  /* Build the cache vars.  */
  mf_cache_shift_decl_l
    = mf_mark (create_tmp_var (TREE_TYPE (mf_cache_shift_decl),
                               "__mf_lookup_shift_l"));

  mf_cache_mask_decl_l
    = mf_mark (create_tmp_var (TREE_TYPE (mf_cache_mask_decl),
                               "__mf_lookup_mask_l"));

  /* Build initialization nodes for the cache vars.  We just load the
     globals into the cache variables.  */
  t = build (MODIFY_EXPR, TREE_TYPE (mf_cache_shift_decl_l),
             mf_cache_shift_decl_l, mf_cache_shift_decl);
  annotate_with_locus (t, DECL_SOURCE_LOCATION (current_function_decl));
  gimplify_to_stmt_list (&t);
  shift_init_stmts = t;

  t = build (MODIFY_EXPR, TREE_TYPE (mf_cache_mask_decl_l),
             mf_cache_mask_decl_l, mf_cache_mask_decl);
  annotate_with_locus (t, DECL_SOURCE_LOCATION (current_function_decl));
  gimplify_to_stmt_list (&t);
  mask_init_stmts = t;

  /* Anticipating multiple entry points, we insert the cache vars
     initializers in each successor of the ENTRY_BLOCK_PTR.  */
  for (tsi = tsi_start (shift_init_stmts);
       ! tsi_end_p (tsi);
       tsi_next (&tsi))
    insert_edge_copies (tsi_stmt (tsi), ENTRY_BLOCK_PTR);

  for (tsi = tsi_start (mask_init_stmts);
       ! tsi_end_p (tsi);
       tsi_next (&tsi))
    insert_edge_copies (tsi_stmt (tsi), ENTRY_BLOCK_PTR);
  bsi_commit_edge_inserts (NULL);
}


static void
mf_decl_clear_locals (void)
{
  /* Unset local shadows. */
  mf_cache_shift_decl_l = NULL_TREE;
  mf_cache_mask_decl_l = NULL_TREE;
}

static void
mf_build_check_statement_for (tree addr, tree size,
			      block_stmt_iterator *instr_bsi,
                              location_t *locus, tree dirflag)
{
  tree_stmt_iterator head, tsi;
  tree ptrtype = TREE_TYPE (addr);
  block_stmt_iterator bsi;
  basic_block cond_bb, then_bb, join_bb;
  edge e;
  tree cond, t, u, v, l1, l2;
  tree mf_value;
  tree mf_base;
  tree mf_elem;

  /* We first need to split the current basic block, and start altering
     the CFG.  This allows us to insert the statements we're about to
     construct into the right basic blocks.  The label l1 is the label
     of the block for the THEN clause of the conditional jump we're
     about to construct, and l2 is the ELSE clause, which is just the
     continuation of the old statement stream.  */
  l1 = create_artificial_label ();
  l2 = create_artificial_label ();
  cond_bb = bb_for_stmt (bsi_stmt (*instr_bsi));
  bsi = *instr_bsi;
  bsi_prev (&bsi);
  if (! bsi_end_p (bsi))
    {
      e = split_block (cond_bb, bsi_stmt (bsi));
      cond_bb = e->src;
      join_bb = e->dest;
    }
  else
    {
      join_bb = cond_bb;
      cond_bb = create_empty_bb (join_bb->prev_bb);
      e = make_edge (cond_bb, join_bb, 0);
    }
  e->flags = EDGE_FALSE_VALUE;
  then_bb = create_empty_bb (cond_bb);
  make_edge (cond_bb, then_bb, EDGE_TRUE_VALUE);
  make_edge (then_bb, join_bb, EDGE_FALLTHRU);

  /* We expect that the conditional jump we will construct will not
     be taken very often as it basically is an exception condition.  */
  predict_edge_def (then_bb->pred, PRED_MUDFLAP, NOT_TAKEN);

  /* Update dominance info.  Note that bb_join's data was
     updated by split_block.  */
  if (dom_computed[CDI_DOMINATORS] >= DOM_CONS_OK)
    {
      set_immediate_dominator (CDI_DOMINATORS, then_bb, cond_bb);
      set_immediate_dominator (CDI_DOMINATORS, join_bb, cond_bb);
    }

  /* Build our local variables.  */
  mf_value = create_tmp_var (ptrtype, "__mf_value");
  mf_elem = create_tmp_var (mf_cache_structptr_type, "__mf_elem");
  mf_base = create_tmp_var (mf_uintptr_type, "__mf_base");

  /* Build: __mf_value = <address expression>.  */
  t = build (MODIFY_EXPR, void_type_node, mf_value, unshare_expr (addr));
  SET_EXPR_LOCUS (t, locus);
  gimplify_to_stmt_list (&t);
  head = tsi_start (t);
  tsi = tsi_last (t);

  /* Build: __mf_base = (uintptr_t)__mf_value.  */
  t = build (MODIFY_EXPR, void_type_node, mf_base,
             build1 (NOP_EXPR, mf_uintptr_type, mf_value));
  SET_EXPR_LOCUS (t, locus);
  gimplify_to_stmt_list (&t);
  tsi_link_after (&tsi, t, TSI_CONTINUE_LINKING);

  /* Build: __mf_elem = &__mf_lookup_cache [(__mf_base >> __mf_shift)
                                            & __mf_mask].  */
  t = build (RSHIFT_EXPR, mf_uintptr_type, mf_base,
             (flag_mudflap_threads ? mf_cache_shift_decl : mf_cache_shift_decl_l));
  t = build (BIT_AND_EXPR, mf_uintptr_type, t,
             (flag_mudflap_threads ? mf_cache_mask_decl : mf_cache_mask_decl_l));
  t = build (ARRAY_REF,
             TREE_TYPE (TREE_TYPE (mf_cache_array_decl)),
             mf_cache_array_decl, t, NULL_TREE, NULL_TREE);
  t = build1 (ADDR_EXPR, mf_cache_structptr_type, t);
  t = build (MODIFY_EXPR, void_type_node, mf_elem, t);
  SET_EXPR_LOCUS (t, locus);
  gimplify_to_stmt_list (&t);
  tsi_link_after (&tsi, t, TSI_CONTINUE_LINKING);

  /* Quick validity check.

     if (__mf_elem->low > __mf_base
	 || (__mf_elem_high < __mf_base + sizeof(T) - 1))
	{
	  __mf_check ();
	  ... and only if single-threaded:
	  __mf_lookup_shift_1 = f...;
	  __mf_lookup_mask_l = ...;
	}

     It is expected that this body of code is rarely executed so we mark
     the edge to the THEN clause of the conditional jump as unlikely.  */

  /* Construct t <-- '__mf_elem->low  > __mf_base'.  */
  t = build (COMPONENT_REF, mf_uintptr_type,
             build1 (INDIRECT_REF, mf_cache_struct_type, mf_elem),
             TYPE_FIELDS (mf_cache_struct_type), NULL_TREE);
  t = build (GT_EXPR, boolean_type_node, t, mf_base);

  /* Construct '__mf_elem->high < __mf_base + sizeof(T) - 1'.

     First build:
	1) u <--  '__mf_elem->high'
	2) v <--  '__mf_base + sizeof (T) - 1'.

     Then build 'u <-- (u < v).  */


  u = build (COMPONENT_REF, mf_uintptr_type,
             build1 (INDIRECT_REF, mf_cache_struct_type, mf_elem),
             TREE_CHAIN (TYPE_FIELDS (mf_cache_struct_type)), NULL_TREE);

  v = convert (mf_uintptr_type,
	       size_binop (MINUS_EXPR, size, size_one_node));
  v = fold (build (PLUS_EXPR, mf_uintptr_type, mf_base, v));

  u = build (LT_EXPR, boolean_type_node, u, v);

  /* Build the composed conditional: t <-- 't || u'.  Then store the
     result of the evaluation of 't' in a temporary variable which we
     can use as the condition for the conditional jump.  */
  t = build (TRUTH_OR_EXPR, boolean_type_node, t, u);
  cond = create_tmp_var (boolean_type_node, "__mf_unlikely_cond");
  t = build (MODIFY_EXPR, boolean_type_node, cond, t);
  gimplify_to_stmt_list (&t);
  tsi_link_after (&tsi, t, TSI_CONTINUE_LINKING);

  /* Build the conditional jump.  'cond' is just a temporary so we can
     simply build a void COND_EXPR.  We do need labels in both arms though.  */
  t = build (COND_EXPR, void_type_node, cond,
	     build (GOTO_EXPR, void_type_node, tree_block_label (then_bb)),
	     build (GOTO_EXPR, void_type_node, tree_block_label (join_bb)));
  SET_EXPR_LOCUS (t, locus);
  tsi_link_after (&tsi, t, TSI_CONTINUE_LINKING);

  /* At this point, after so much hard work, we have only constructed
     the conditional jump,

     if (__mf_elem->low > __mf_base
	 || (__mf_elem_high < __mf_base + sizeof(T) - 1))

     The lowered GIMPLE tree representing this code is in the statement
     list starting at 'head'.

     We can insert this now in the current basic block, ie. the one that
     the statement we're instrumenting was originally in.  */
  bsi = bsi_last (cond_bb);
  for (tsi = head; ! tsi_end_p (tsi); tsi_next (&tsi))
    bsi_insert_after (&bsi, tsi_stmt (tsi), BSI_CONTINUE_LINKING);

  /*  Now build up the body of the cache-miss handling:

     __mf_check();
     refresh *_l vars.

     This is the body of the conditional.  */
  
  u = tree_cons (NULL_TREE, mf_file_function_line_tree (locus), NULL_TREE);
  u = tree_cons (NULL_TREE, dirflag, u);
  u = tree_cons (NULL_TREE, size, u);
  u = tree_cons (NULL_TREE, mf_value, u);
  t = build_function_call_expr (mf_check_fndecl, u);
  gimplify_to_stmt_list (&t);
  head = tsi_start (t);
  tsi = tsi_last (t);

  if (! flag_mudflap_threads)
    {
      t = build (MODIFY_EXPR, void_type_node,
                 mf_cache_shift_decl_l, mf_cache_shift_decl);
      tsi_link_after (&tsi, t, TSI_CONTINUE_LINKING);

      t = build (MODIFY_EXPR, void_type_node,
                 mf_cache_mask_decl_l, mf_cache_mask_decl);
      tsi_link_after (&tsi, t, TSI_CONTINUE_LINKING);
    }

  /* Insert the check code in the THEN block.  */
  bsi = bsi_start (then_bb);
  for (tsi = head; ! tsi_end_p (tsi); tsi_next (&tsi))
    bsi_insert_after (&bsi, tsi_stmt (tsi), BSI_CONTINUE_LINKING);

  *instr_bsi = bsi_start (join_bb);
  bsi_next (instr_bsi);
}

static void
mf_xform_derefs_1 (block_stmt_iterator *iter, tree *tp,
                   location_t *locus, tree dirflag)
{
  tree type, ptr_type, addr, size, t;

  /* Don't instrument read operations.  */
  if (dirflag == integer_zero_node && flag_mudflap_ignore_reads)
    return;

  t = *tp;
  type = TREE_TYPE (t);
  size = TYPE_SIZE_UNIT (type);

  switch (TREE_CODE (t))
    {
    case ARRAY_REF:
      {
        /* Omit checking if we can statically determine that the access is
           valid.  For non-addressable local arrays this is not optional,
           since we won't have called __mf_register for the object.  */
        tree op0, op1;

        op0 = TREE_OPERAND (t, 0);
        op1 = TREE_OPERAND (t, 1);
        while (TREE_CODE (op1) == INTEGER_CST)
          {
            tree dom = TYPE_DOMAIN (TREE_TYPE (op0));

	    /* Test for index in range.  Break if not.  */
	    if (!dom
		|| (! TYPE_MIN_VALUE (dom)
		    || ! really_constant_p (TYPE_MIN_VALUE (dom)))
		|| (! TYPE_MAX_VALUE (dom)
		    || ! really_constant_p (TYPE_MAX_VALUE (dom)))
		|| (tree_int_cst_lt (op1, TYPE_MIN_VALUE (dom))
		    || tree_int_cst_lt (TYPE_MAX_VALUE (dom), op1)))
              break;

            /* If we're looking at a non-external VAR_DECL, then the 
               access must be ok.  */
            if (TREE_CODE (op0) == VAR_DECL && !DECL_EXTERNAL (op0))
              return;

            /* Only continue if we're still looking at an array.  */
            if (TREE_CODE (op0) != ARRAY_REF)
              break;

            op1 = TREE_OPERAND (op0, 1);
            op0 = TREE_OPERAND (op0, 0);
          }
      
        /* If we got here, we couldn't statically the check.  */
        ptr_type = build_pointer_type (type);
        addr = build1 (ADDR_EXPR, ptr_type, t);
      }
      break;

    case INDIRECT_REF:
      addr = TREE_OPERAND (t, 0);
      ptr_type = TREE_TYPE (addr);
      break;

    case ARRAY_RANGE_REF:
      warning ("mudflap checking not yet implemented for ARRAY_RANGE_REF");
      return;

    case COMPONENT_REF:
      {
        tree field;

        /* If we're not dereferencing something, then the access
           must be ok.  */
        if (TREE_CODE (TREE_OPERAND (t, 0)) != INDIRECT_REF)
          return;

        field = TREE_OPERAND (t, 1);

        /* If we're looking at a bit field, then we can't take its address
           with ADDR_EXPR -- lang_hooks.mark_addressable will error.  Do
           things the hard way with PLUS.  */
        if (DECL_BIT_FIELD_TYPE (field))
          {
	    if (TREE_CODE (DECL_SIZE_UNIT (field)) == INTEGER_CST)
	      size = DECL_SIZE_UNIT (field);

            addr = TREE_OPERAND (TREE_OPERAND (t, 0), 0);
            addr = fold_convert (ptr_type_node, addr);
            addr = fold (build (PLUS_EXPR, ptr_type_node,
                                addr, fold_convert (ptr_type_node,
						    byte_position (field))));
          }
        else
          {
            ptr_type = build_pointer_type (type);
            addr = build1 (ADDR_EXPR, ptr_type, t);
          }
      }
      break;

    case BIT_FIELD_REF:
      {
        tree ofs, rem, bpu;

        /* If we're not dereferencing something, then the access
           must be ok.  */
        if (TREE_CODE (TREE_OPERAND (t, 0)) != INDIRECT_REF)
          return;

        bpu = bitsize_int (BITS_PER_UNIT);
        ofs = convert (bitsizetype, TREE_OPERAND (t, 2));
        rem = size_binop (TRUNC_MOD_EXPR, ofs, bpu);
        ofs = size_binop (TRUNC_DIV_EXPR, ofs, bpu);

        size = convert (bitsizetype, TREE_OPERAND (t, 1));
        size = size_binop (PLUS_EXPR, size, rem);
        size = size_binop (CEIL_DIV_EXPR, size, bpu);
        size = convert (sizetype, size);

        addr = TREE_OPERAND (TREE_OPERAND (t, 0), 0);
        addr = convert (ptr_type_node, addr);
        addr = fold (build (PLUS_EXPR, ptr_type_node, addr, ofs));
      }
      break;

    default:
      return;
    }

  mf_build_check_statement_for (addr, size, iter, locus, dirflag);
}

static void
mf_xform_derefs (void)
{
  basic_block bb, next;
  block_stmt_iterator i;
  int saved_last_basic_block = last_basic_block;

  bb = ENTRY_BLOCK_PTR ->next_bb;
  do
    {
      next = bb->next_bb;
      for (i = bsi_start (bb); !bsi_end_p (i); bsi_next (&i))
	{
	  tree s = bsi_stmt (i);

	  /* Only a few GIMPLE statements can reference memory.  */
	  switch (TREE_CODE (s))
	    {
	    case MODIFY_EXPR:
	      mf_xform_derefs_1 (&i, &TREE_OPERAND (s, 0), EXPR_LOCUS (s),
				 integer_one_node);
	      mf_xform_derefs_1 (&i, &TREE_OPERAND (s, 1), EXPR_LOCUS (s),
				 integer_zero_node);
	      break;

	    case RETURN_EXPR:
	      if (TREE_OPERAND (s, 0) != NULL_TREE)
		{
		  if (TREE_CODE (TREE_OPERAND (s, 0)) == MODIFY_EXPR)
		    mf_xform_derefs_1 (&i, &TREE_OPERAND (TREE_OPERAND (s, 0), 1),
				       EXPR_LOCUS (s), integer_zero_node);
		  else
		    mf_xform_derefs_1 (&i, &TREE_OPERAND (s, 0), EXPR_LOCUS (s),
				       integer_zero_node);
		}
	      break;

	    default:
	      ;
	    }
	}
      bb = next;
    }
  while (bb && bb->index <= saved_last_basic_block);
}

/* ------------------------------------------------------------------------ */
/* ADDR_EXPR transforms.  Perform the declaration-related mudflap tree
   transforms on the current function.

   This is the first part of the mudflap instrumentation.  It works on
   high-level GIMPLE because after lowering, all variables are moved out
   of their BIND_EXPR binding context, and we lose liveness information
   for the declarations we wish to instrument.  */

static void
execute_mudflap_function_decls (void)
{
  if (mf_marked_p (current_function_decl))
    return;

  push_gimplify_context ();

  mf_init_extern_trees ();
  mf_xform_decls (DECL_SAVED_TREE (current_function_decl),
                  DECL_ARGUMENTS (current_function_decl));

  pop_gimplify_context (NULL);
}

/* This struct is passed between mf_xform_decls to store state needed
   during the traversal searching for objects that have their
   addresses taken.  */
struct mf_xform_decls_data
{
  tree param_decls;
};


/* Synthesize a CALL_EXPR and a TRY_FINALLY_EXPR, for this chain of
   _DECLs if appropriate.  Arrange to call the __mf_register function
   now, and the __mf_unregister function later for each.  */
static void
mx_register_decls (tree decl, tree *stmt_list)
{
  tree finally_stmts = NULL_TREE;
  tree_stmt_iterator initially_stmts = tsi_start (*stmt_list);

  while (decl != NULL_TREE)
    {
      /* Eligible decl?  */
      if ((TREE_CODE (decl) == VAR_DECL || TREE_CODE (decl) == PARM_DECL)
	  /* It must be a non-external, automatic variable.  */
	  && ! DECL_EXTERNAL (decl)
	  && ! TREE_STATIC (decl)
	  /* The decl must have its address taken.  */
	  && TREE_ADDRESSABLE (decl)
	  /* The type of the variable must be complete.  */
	  && COMPLETE_OR_VOID_TYPE_P (TREE_TYPE (decl))
	  /* Don't process the same decl twice.  */
	  && ! mf_marked_p (decl))
        {
          tree size = NULL_TREE, variable_name;
          tree unregister_fncall, unregister_fncall_params;
          tree register_fncall, register_fncall_params;

          if (DECL_DEFER_OUTPUT (decl))
            {
              /* Oh no ... it's probably a variable-length array (VLA).
                 The size and address cannot be computed by merely
                 looking at the DECL.  See gimplify_decl_stmt for the
                 method by which VLA declarations turn into calls to
                 BUILT_IN_STACK_ALLOC.  We assume that multiple
                 VLAs declared later in the same block get allocation 
                 code later than the others.  */
              tree stack_alloc_call = NULL_TREE;

              while(! tsi_end_p (initially_stmts))
                {
                  tree t = tsi_stmt (initially_stmts);

                  tree call = NULL_TREE;
                  if (TREE_CODE (t) == CALL_EXPR)
                    call = t;
                  else if (TREE_CODE (t) == MODIFY_EXPR &&
                           TREE_CODE (TREE_OPERAND (t, 1)) == CALL_EXPR)
                    call = TREE_OPERAND (t, 1);
                  else if (TREE_CODE (t) == TRY_FINALLY_EXPR)
                    {
                      /* We hope that this is the try/finally block sometimes
		         constructed by gimplify_bind_expr() for a BIND_EXPR
			 that contains VLAs.  This very naive recursion
			 appears to be sufficient.  */
                      initially_stmts = tsi_start (TREE_OPERAND (t, 0));
                    }

                  if (call != NULL_TREE)
                    {
                      if (TREE_CODE (TREE_OPERAND(call, 0)) == ADDR_EXPR &&
                          TREE_OPERAND (TREE_OPERAND (call, 0), 0) ==
			  	implicit_built_in_decls [BUILT_IN_STACK_ALLOC])
                        {
                          tree stack_alloc_args = TREE_OPERAND (call, 1);
                          tree stack_alloc_op1 = TREE_VALUE (stack_alloc_args);
                          tree stack_alloc_op2 = TREE_VALUE (TREE_CHAIN (stack_alloc_args));
                          
                          if (TREE_CODE (stack_alloc_op1) == ADDR_EXPR &&
                              TREE_OPERAND (stack_alloc_op1, 0) == decl)
                            {
                              /* Got it! */
                              size = stack_alloc_op2;
                              stack_alloc_call = call;
                              /* Advance iterator to point past this allocation call.  */
                              tsi_next (&initially_stmts);
                              break;
                            }
                        }
                    }

                  tsi_next (&initially_stmts);
                }

              if (stack_alloc_call == NULL_TREE)
                {
                  warning ("mudflap cannot handle variable-sized declaration `%s'",
                         IDENTIFIER_POINTER (DECL_NAME (decl)));
                  break;
                }
            }
          else
            {
              size = convert (size_type_node, TYPE_SIZE_UNIT (TREE_TYPE (decl)));
            }

          /* (& VARIABLE, sizeof (VARIABLE), __MF_TYPE_STACK) */
          unregister_fncall_params =
            tree_cons (NULL_TREE,
                       convert (ptr_type_node,
                                mf_mark (build1 (ADDR_EXPR,
                                                 build_pointer_type (TREE_TYPE (decl)),
                                                 decl))),
                       tree_cons (NULL_TREE, 
                                  size,
                                  tree_cons (NULL_TREE, 
                                             build_int_2 (3, 0), /* __MF_TYPE_STACK */
                                             NULL_TREE)));
          /* __mf_unregister (...) */
          unregister_fncall = build_function_call_expr (mf_unregister_fndecl,
                                                        unregister_fncall_params);

          /* (& VARIABLE, sizeof (VARIABLE), __MF_TYPE_STACK, "name") */
          variable_name = mf_varname_tree (decl);
          register_fncall_params =
            tree_cons (NULL_TREE,
                   convert (ptr_type_node,
                            mf_mark (build1 (ADDR_EXPR,
                                             build_pointer_type (TREE_TYPE (decl)),
                                             decl))),
                       tree_cons (NULL_TREE,
                                  size,
                                  tree_cons (NULL_TREE,
                                             build_int_2 (3, 0), /* __MF_TYPE_STACK */
                                             tree_cons (NULL_TREE,
                                                        variable_name,
                                                        NULL_TREE))));

          /* __mf_register (...) */
          register_fncall = build_function_call_expr (mf_register_fndecl,
                                                      register_fncall_params);

          /* Accumulate the two calls.  */
          /* ??? Set EXPR_LOCUS.  */
          gimplify_stmt (&register_fncall);
          gimplify_stmt (&unregister_fncall);

          /* Add the __mf_register call at the current appending point.  */
          if (tsi_end_p (initially_stmts))
            internal_error ("mudflap ran off end of BIND_EXPR body");
          tsi_link_before (&initially_stmts, register_fncall, TSI_SAME_STMT);

          /* Accumulate the FINALLY piece.  */
          append_to_statement_list (unregister_fncall, &finally_stmts);

          mf_mark (decl);
        }

      decl = TREE_CHAIN (decl);
    }

  /* Actually, (initially_stmts!=NULL) <=> (finally_stmts!=NULL) */
  if (finally_stmts != NULL_TREE)
    {
      tree t = build (TRY_FINALLY_EXPR, void_type_node,
                      *stmt_list, finally_stmts);
      *stmt_list = NULL;
      append_to_statement_list (t, stmt_list);
    }
}


/* Process every variable mentioned in BIND_EXPRs.  */
static tree
mx_xfn_xform_decls (tree *t, int *continue_p, void *data)
{
  struct mf_xform_decls_data* d = (struct mf_xform_decls_data*) data;

  if (*t == NULL_TREE || *t == error_mark_node)
    {
      *continue_p = 0;
      return NULL_TREE;
    }

  *continue_p = 1;

  switch (TREE_CODE (*t))
    {
    case BIND_EXPR:
      {
        /* Process function parameters now (but only once).  */
        mx_register_decls (d->param_decls, &BIND_EXPR_BODY (*t));
        d->param_decls = NULL_TREE;

        mx_register_decls (BIND_EXPR_VARS (*t), &BIND_EXPR_BODY (*t));
      }
      break;

    default:
      break;
    }

  return NULL;
}

/* Perform the object lifetime tracking mudflap transform on the given function
   tree.  The tree is mutated in place, with possibly copied subtree nodes.

   For every auto variable declared, if its address is ever taken
   within the function, then supply its lifetime to the mudflap
   runtime with the __mf_register and __mf_unregister calls.
*/

static void
mf_xform_decls (tree fnbody, tree fnparams)
{
  struct mf_xform_decls_data d;
  d.param_decls = fnparams;
  walk_tree_without_duplicates (&fnbody, mx_xfn_xform_decls, &d);
}


/* ------------------------------------------------------------------------ */
/* Externally visible mudflap functions.  */


/* Mark and return the given tree node to prevent further mudflap
   transforms.  */
static GTY ((param_is (union tree_node))) htab_t marked_trees = NULL;

tree
mf_mark (tree t)
{
  void **slot;

  if (marked_trees == NULL)
    marked_trees = htab_create_ggc (31, htab_hash_pointer, htab_eq_pointer, NULL);

  slot = htab_find_slot (marked_trees, t, INSERT);
  *slot = t;
  return t;
}

int
mf_marked_p (tree t)
{
  void *entry;

  if (marked_trees == NULL)
    return 0;

  entry = htab_find (marked_trees, t);
  return (entry != NULL);
}

/* Remember given node as a static of some kind: global data,
   function-scope static, or an anonymous constant.  Its assembler
   label is given.  */

/* A list of globals whose incomplete declarations we encountered.
   Instead of emitting the __mf_register call for them here, it's
   delayed until program finish time.  If they're still incomplete by
   then, warnings are emitted.  */

static GTY (()) varray_type deferred_static_decls;

/* A list of statements for calling __mf_register() at startup time.  */
static GTY (()) tree enqueued_call_stmt_chain;

static void
mudflap_register_call (tree obj, tree object_size, tree varname)
{
  tree arg, args, call_stmt;

  args = tree_cons (NULL_TREE, varname, NULL_TREE);

  arg = build_int_2 (4, 0); /* __MF_TYPE_STATIC */
  args = tree_cons (NULL_TREE, arg, args);

  arg = convert (size_type_node, object_size);
  args = tree_cons (NULL_TREE, arg, args);

  arg = build1 (ADDR_EXPR, build_pointer_type (TREE_TYPE (obj)), obj);
  arg = convert (ptr_type_node, arg);
  args = tree_cons (NULL_TREE, arg, args);

  mf_init_extern_trees ();
  call_stmt = build_function_call_expr (mf_register_fndecl, args);

  append_to_statement_list (call_stmt, &enqueued_call_stmt_chain);
}

void
mudflap_enqueue_decl (tree obj)
{
  if (mf_marked_p (obj))
    return;

  /* We don't need to process variable decls that are internally
     generated extern.  If we did, we'd end up with warnings for them
     during mudflap_finish_file ().  That would confuse the user,
     since the text would refer to variables that don't show up in the
     user's source code.  */
  if (DECL_P (obj) && DECL_EXTERNAL (obj) && DECL_ARTIFICIAL (obj))
    return;

  if (COMPLETE_TYPE_P (TREE_TYPE (obj)))
    {
      tree object_size;

      mf_mark (obj);

      object_size = size_in_bytes (TREE_TYPE (obj));

      if (dump_file)
        {
          fprintf (dump_file, "enqueue_decl obj=`");
          print_generic_expr (dump_file, obj, dump_flags);
          fprintf (dump_file, "' size=");
          print_generic_expr (dump_file, object_size, dump_flags);
          fprintf (dump_file, "\n");
        }

      /* NB: the above condition doesn't require TREE_USED or
         TREE_ADDRESSABLE.  That's because this object may be a global
         only used from other compilation units.  XXX: Maybe static
         objects could require those attributes being set.  */

      mudflap_register_call (obj, object_size, mf_varname_tree (obj));
    }
  else
    {
      size_t i;

      if (! deferred_static_decls)
        VARRAY_TREE_INIT (deferred_static_decls, 10, "deferred static list");

      /* Ugh, linear search... */
      for (i = 0; i < VARRAY_ACTIVE_SIZE (deferred_static_decls); i++)
        if (VARRAY_TREE (deferred_static_decls, i) == obj)
          {
            warning ("mudflap cannot track lifetime of `%s'",
                     IDENTIFIER_POINTER (DECL_NAME (obj)));
            return;
          }

      VARRAY_PUSH_TREE (deferred_static_decls, obj);
    }
}

void
mudflap_enqueue_constant (tree obj)
{
  tree object_size, varname;

  if (mf_marked_p (obj))
    return;

  if (TREE_CODE (obj) == STRING_CST)
    object_size = build_int_2 (TREE_STRING_LENGTH (obj), 0);
  else
    object_size = size_in_bytes (TREE_TYPE (obj));

  if (dump_file)
    {
      fprintf (dump_file, "enqueue_constant obj=`");
      print_generic_expr (dump_file, obj, dump_flags);
      fprintf (dump_file, "' size=");
      print_generic_expr (dump_file, object_size, dump_flags);
      fprintf (dump_file, "\n");
    }

  if (TREE_CODE (obj) == STRING_CST)
    varname = mf_build_string ("string literal");
  else
    varname = mf_build_string ("constant");

  mudflap_register_call (obj, object_size, varname);
}


/* Emit any file-wide instrumentation.  */
void
mudflap_finish_file (void)
{
  /* Try to give the deferred objects one final try.  */
  if (deferred_static_decls)
    {
      size_t i;

      for (i = 0; i < VARRAY_ACTIVE_SIZE (deferred_static_decls); i++)
        {
          tree obj = VARRAY_TREE (deferred_static_decls, i);

          /* Call enqueue_decl again on the same object it has previously
             put into the table.  (It won't modify the table this time, so
             infinite iteration is not a problem.)  */
          mudflap_enqueue_decl (obj);
        }

      VARRAY_CLEAR (deferred_static_decls);
    }

  mflang_flush_calls (enqueued_call_stmt_chain);
}


static bool
gate_mudflap (void)
{
  return flag_mudflap != 0;
}

struct tree_opt_pass pass_mudflap_1 = 
{
  "mudflap1",                           /* name */
  gate_mudflap,                         /* gate */
  execute_mudflap_function_decls,       /* execute */
  NULL,                                 /* sub */
  NULL,                                 /* next */
  0,                                    /* static_pass_number */
  0,                                    /* tv_id */
  PROP_gimple_any,                      /* properties_required */
  0,                                    /* properties_provided */
  0,                                    /* properties_destroyed */
  0,                                    /* todo_flags_start */
  TODO_dump_func                        /* todo_flags_finish */
};

struct tree_opt_pass pass_mudflap_2 = 
{
  "mudflap2",                           /* name */
  gate_mudflap,                         /* gate */
  execute_mudflap_function_ops,         /* execute */
  NULL,                                 /* sub */
  NULL,                                 /* next */
  0,                                    /* static_pass_number */
  0,                                    /* tv_id */
  PROP_gimple_leh,                      /* properties_required */
  0,                                    /* properties_provided */
  0,                                    /* properties_destroyed */
  0,                                    /* todo_flags_start */
  TODO_verify_flow | TODO_verify_stmts
  | TODO_dump_func                      /* todo_flags_finish */
};

#include "gt-tree-mudflap.h"
