/* Inlining decision heuristics.
   Copyright (C) 2003, 2004, 2007, 2008, 2009, 2010, 2011
   Free Software Foundation, Inc.
   Contributed by Jan Hubicka

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

/* Analysis used by the inliner and other passes limiting code size growth.

   We estimate for each function
     - function body size
     - function runtime
     - inlining size benefit (that is how much of function body size
       and its call sequence is expected to disappear by inlining)
     - inlining time benefit
     - function frame size
   For each call
     - call sequence size

   inlinie_summary datastructures store above information locally (i.e.
   parameters of the function itself) and globally (i.e. parameters of
   the function created by applying all the inline decisions already
   present in the callgraph).

   We also provide accestor to the inline_summary datastructure and
   basic logic updating the parameters when inlining is performed. 

   Finally pass_inline_parameters is exported.  This is used to drive
   computation of function parameters used by the early inliner. IPA
   inlined performs analysis via its analyze_function method. */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "tree.h"
#include "tree-inline.h"
#include "langhooks.h"
#include "flags.h"
#include "cgraph.h"
#include "diagnostic.h"
#include "gimple-pretty-print.h"
#include "timevar.h"
#include "params.h"
#include "tree-pass.h"
#include "coverage.h"
#include "ggc.h"
#include "tree-flow.h"
#include "ipa-prop.h"
#include "ipa-inline.h"

#define MAX_TIME 1000000000

/* Holders of ipa cgraph hooks: */
static struct cgraph_node_hook_list *function_insertion_hook_holder;

/* See if statement might disappear after inlining.
   0 - means not eliminated
   1 - half of statements goes away
   2 - for sure it is eliminated.
   We are not terribly sophisticated, basically looking for simple abstraction
   penalty wrappers.  */

static int
eliminated_by_inlining_prob (gimple stmt)
{
  enum gimple_code code = gimple_code (stmt);
  switch (code)
    {
      case GIMPLE_RETURN:
        return 2;
      case GIMPLE_ASSIGN:
	if (gimple_num_ops (stmt) != 2)
	  return 0;

	/* Casts of parameters, loads from parameters passed by reference
	   and stores to return value or parameters are often free after
	   inlining dua to SRA and further combining.
	   Assume that half of statements goes away.  */
	if (gimple_assign_rhs_code (stmt) == CONVERT_EXPR
	    || gimple_assign_rhs_code (stmt) == NOP_EXPR
	    || gimple_assign_rhs_code (stmt) == VIEW_CONVERT_EXPR
	    || gimple_assign_rhs_class (stmt) == GIMPLE_SINGLE_RHS)
	  {
	    tree rhs = gimple_assign_rhs1 (stmt);
            tree lhs = gimple_assign_lhs (stmt);
	    tree inner_rhs = rhs;
	    tree inner_lhs = lhs;
	    bool rhs_free = false;
	    bool lhs_free = false;

 	    while (handled_component_p (inner_lhs)
		   || TREE_CODE (inner_lhs) == MEM_REF)
	      inner_lhs = TREE_OPERAND (inner_lhs, 0);
 	    while (handled_component_p (inner_rhs)
	           || TREE_CODE (inner_rhs) == ADDR_EXPR
		   || TREE_CODE (inner_rhs) == MEM_REF)
	      inner_rhs = TREE_OPERAND (inner_rhs, 0);


	    if (TREE_CODE (inner_rhs) == PARM_DECL
	        || (TREE_CODE (inner_rhs) == SSA_NAME
		    && SSA_NAME_IS_DEFAULT_DEF (inner_rhs)
		    && TREE_CODE (SSA_NAME_VAR (inner_rhs)) == PARM_DECL))
	      rhs_free = true;
	    if (rhs_free && is_gimple_reg (lhs))
	      lhs_free = true;
	    if (((TREE_CODE (inner_lhs) == PARM_DECL
	          || (TREE_CODE (inner_lhs) == SSA_NAME
		      && SSA_NAME_IS_DEFAULT_DEF (inner_lhs)
		      && TREE_CODE (SSA_NAME_VAR (inner_lhs)) == PARM_DECL))
		 && inner_lhs != lhs)
	        || TREE_CODE (inner_lhs) == RESULT_DECL
	        || (TREE_CODE (inner_lhs) == SSA_NAME
		    && TREE_CODE (SSA_NAME_VAR (inner_lhs)) == RESULT_DECL))
	      lhs_free = true;
	    if (lhs_free
		&& (is_gimple_reg (rhs) || is_gimple_min_invariant (rhs)))
	      rhs_free = true;
	    if (lhs_free && rhs_free)
	      return 1;
	  }
	return 0;
      default:
	return 0;
    }
}


/* Compute function body size parameters for NODE.  */

static void
estimate_function_body_sizes (struct cgraph_node *node)
{
  gcov_type time = 0;
  gcov_type time_inlining_benefit = 0;
  /* Estimate static overhead for function prologue/epilogue and alignment. */
  int size = 2;
  /* Benefits are scaled by probability of elimination that is in range
     <0,2>.  */
  int size_inlining_benefit = 2 * 2;
  basic_block bb;
  gimple_stmt_iterator bsi;
  struct function *my_function = DECL_STRUCT_FUNCTION (node->decl);
  int freq;

  if (dump_file)
    fprintf (dump_file, "Analyzing function body size: %s\n",
	     cgraph_node_name (node));

  gcc_assert (my_function && my_function->cfg);
  FOR_EACH_BB_FN (bb, my_function)
    {
      freq = compute_call_stmt_bb_frequency (node->decl, bb);
      for (bsi = gsi_start_bb (bb); !gsi_end_p (bsi); gsi_next (&bsi))
	{
	  gimple stmt = gsi_stmt (bsi);
	  int this_size = estimate_num_insns (stmt, &eni_size_weights);
	  int this_time = estimate_num_insns (stmt, &eni_time_weights);
	  int prob;

	  if (dump_file && (dump_flags & TDF_DETAILS))
	    {
	      fprintf (dump_file, "  freq:%6i size:%3i time:%3i ",
		       freq, this_size, this_time);
	      print_gimple_stmt (dump_file, stmt, 0, 0);
	    }
	  this_time *= freq;
	  time += this_time;
	  size += this_size;
	  prob = eliminated_by_inlining_prob (stmt);
	  if (prob == 1 && dump_file && (dump_flags & TDF_DETAILS))
	    fprintf (dump_file, "    50%% will be eliminated by inlining\n");
	  if (prob == 2 && dump_file && (dump_flags & TDF_DETAILS))
	    fprintf (dump_file, "    will eliminated by inlining\n");
	  size_inlining_benefit += this_size * prob;
	  time_inlining_benefit += this_time * prob;
	  gcc_assert (time >= 0);
	  gcc_assert (size >= 0);
	}
    }
  time = (time + CGRAPH_FREQ_BASE / 2) / CGRAPH_FREQ_BASE;
  time_inlining_benefit = ((time_inlining_benefit + CGRAPH_FREQ_BASE)
  			   / (CGRAPH_FREQ_BASE * 2));
  size_inlining_benefit = (size_inlining_benefit + 1) / 2;
  if (time_inlining_benefit > MAX_TIME)
    time_inlining_benefit = MAX_TIME;
  if (time > MAX_TIME)
    time = MAX_TIME;
  if (dump_file)
    fprintf (dump_file, "Overall function body time: %i-%i size: %i-%i\n",
	     (int)time, (int)time_inlining_benefit,
	     size, size_inlining_benefit);
  inline_summary (node)->self_time = time;
  inline_summary (node)->self_size = size;
  inline_summary (node)->time_inlining_benefit = time_inlining_benefit;
  inline_summary (node)->size_inlining_benefit = size_inlining_benefit;
}


/* Compute parameters of functions used by inliner.  */

void
compute_inline_parameters (struct cgraph_node *node)
{
  HOST_WIDE_INT self_stack_size;
  struct cgraph_edge *e;

  gcc_assert (!node->global.inlined_to);

  /* Estimate the stack size for the function if we're optimizing.  */
  self_stack_size = optimize ? estimated_stack_frame_size (node) : 0;
  inline_summary (node)->estimated_self_stack_size = self_stack_size;
  node->global.estimated_stack_size = self_stack_size;
  node->global.stack_frame_offset = 0;

  /* Can this function be inlined at all?  */
  node->local.inlinable = tree_inlinable_function_p (node->decl);
  if (!node->local.inlinable)
    node->local.disregard_inline_limits = 0;

  /* Inlinable functions always can change signature.  */
  if (node->local.inlinable)
    node->local.can_change_signature = true;
  else
    {
      /* Functions calling builtin_apply can not change signature.  */
      for (e = node->callees; e; e = e->next_callee)
	if (DECL_BUILT_IN (e->callee->decl)
	    && DECL_BUILT_IN_CLASS (e->callee->decl) == BUILT_IN_NORMAL
	    && DECL_FUNCTION_CODE (e->callee->decl) == BUILT_IN_APPLY_ARGS)
	  break;
      node->local.can_change_signature = !e;
    }
  estimate_function_body_sizes (node);
  /* Compute size of call statements.  We have to do this for callers here,
     those sizes need to be present for edges _to_ us as early as
     we are finished with early opts.  */
  for (e = node->callers; e; e = e->next_caller)
    if (e->call_stmt)
      {
	e->call_stmt_size
	  = estimate_num_insns (e->call_stmt, &eni_size_weights);
	e->call_stmt_time
	  = estimate_num_insns (e->call_stmt, &eni_time_weights);
      }
  /* Inlining characteristics are maintained by the cgraph_mark_inline.  */
  node->global.time = inline_summary (node)->self_time;
  node->global.size = inline_summary (node)->self_size;
}


/* Compute parameters of functions used by inliner using
   current_function_decl.  */

static unsigned int
compute_inline_parameters_for_current (void)
{
  compute_inline_parameters (cgraph_get_node (current_function_decl));
  return 0;
}

struct gimple_opt_pass pass_inline_parameters =
{
 {
  GIMPLE_PASS,
  "inline_param",			/* name */
  NULL,					/* gate */
  compute_inline_parameters_for_current,/* execute */
  NULL,					/* sub */
  NULL,					/* next */
  0,					/* static_pass_number */
  TV_INLINE_HEURISTICS,			/* tv_id */
  0,	                                /* properties_required */
  0,					/* properties_provided */
  0,					/* properties_destroyed */
  0,					/* todo_flags_start */
  0					/* todo_flags_finish */
 }
};


/* Estimate the time cost for the caller when inlining EDGE.  */

static inline int
estimate_edge_time (struct cgraph_edge *edge)
{
  int call_stmt_time;
  /* ???  We throw away cgraph edges all the time so the information
     we store in edges doesn't persist for early inlining.  Ugh.  */
  if (!edge->call_stmt)
    call_stmt_time = edge->call_stmt_time;
  else
    call_stmt_time = estimate_num_insns (edge->call_stmt, &eni_time_weights);
  return (((gcov_type)edge->callee->global.time
	   - inline_summary (edge->callee)->time_inlining_benefit
	   - call_stmt_time) * edge->frequency
	  + CGRAPH_FREQ_BASE / 2) / CGRAPH_FREQ_BASE;
}


/* Estimate self time of the function NODE after inlining EDGE.  */

int
estimate_time_after_inlining (struct cgraph_node *node,
			      struct cgraph_edge *edge)
{
  gcov_type time = node->global.time + estimate_edge_time (edge);
  if (time < 0)
    time = 0;
  if (time > MAX_TIME)
    time = MAX_TIME;
  return time;
}


/* Estimate the size of NODE after inlining EDGE which should be an
   edge to either NODE or a call inlined into NODE.  */

int
estimate_size_after_inlining (struct cgraph_node *node,
				     struct cgraph_edge *edge)
{
  int size = node->global.size + estimate_edge_growth (edge);
  gcc_assert (size >= 0);
  return size;
}


/* Estimate the growth caused by inlining NODE into all callees.  */

int
estimate_growth (struct cgraph_node *node)
{
  int growth = 0;
  struct cgraph_edge *e;
  bool self_recursive = false;

  if (node->global.estimated_growth != INT_MIN)
    return node->global.estimated_growth;

  for (e = node->callers; e; e = e->next_caller)
    {
      if (e->caller == node)
        self_recursive = true;
      if (e->inline_failed)
	growth += estimate_edge_growth (e);
    }

  /* ??? Wrong for non-trivially self recursive functions or cases where
     we decide to not inline for different reasons, but it is not big deal
     as in that case we will keep the body around, but we will also avoid
     some inlining.  */
  if (cgraph_will_be_removed_from_program_if_no_direct_calls (node)
      && !DECL_EXTERNAL (node->decl) && !self_recursive)
    growth -= node->global.size;
  /* COMDAT functions are very often not shared across multiple units since they
     come from various template instantiations.  Take this into account.  */
  else  if (DECL_COMDAT (node->decl) && !self_recursive
	    && cgraph_can_remove_if_no_direct_calls_p (node))
    growth -= (node->global.size
	       * (100 - PARAM_VALUE (PARAM_COMDAT_SHARING_PROBABILITY)) + 50) / 100;

  node->global.estimated_growth = growth;
  return growth;
}

/* This function performs intraprocedural analysis in NODE that is required to
   inline indirect calls.  */
static void
inline_indirect_intraprocedural_analysis (struct cgraph_node *node)
{
  ipa_analyze_node (node);
  if (dump_file && (dump_flags & TDF_DETAILS))
    {
      ipa_print_node_params (dump_file, node);
      ipa_print_node_jump_functions (dump_file, node);
    }
}


/* Note function body size.  */

static void
inline_analyze_function (struct cgraph_node *node)
{
  push_cfun (DECL_STRUCT_FUNCTION (node->decl));
  current_function_decl = node->decl;

  compute_inline_parameters (node);
  /* FIXME: We should remove the optimize check after we ensure we never run
     IPA passes when not optimizing.  */
  if (flag_indirect_inlining && optimize)
    inline_indirect_intraprocedural_analysis (node);

  current_function_decl = NULL;
  pop_cfun ();
}


/* Called when new function is inserted to callgraph late.  */

static void
add_new_function (struct cgraph_node *node, void *data ATTRIBUTE_UNUSED)
{
  inline_analyze_function (node);
}


/* Note function body size.  */

void
inline_generate_summary (void)
{
  struct cgraph_node *node;

  function_insertion_hook_holder =
      cgraph_add_function_insertion_hook (&add_new_function, NULL);

  if (flag_indirect_inlining)
    ipa_register_cgraph_hooks ();

  for (node = cgraph_nodes; node; node = node->next)
    if (node->analyzed)
      inline_analyze_function (node);

  return;
}


/* Read inline summary.  Jump functions are shared among ipa-cp
   and inliner, so when ipa-cp is active, we don't need to write them
   twice.  */

void
inline_read_summary (void)
{
  if (flag_indirect_inlining)
    {
      ipa_register_cgraph_hooks ();
      if (!flag_ipa_cp)
        ipa_prop_read_jump_functions ();
    }
  function_insertion_hook_holder =
      cgraph_add_function_insertion_hook (&add_new_function, NULL);
}


/* Write inline summary for node in SET.
   Jump functions are shared among ipa-cp and inliner, so when ipa-cp is
   active, we don't need to write them twice.  */

void
inline_write_summary (cgraph_node_set set,
		      varpool_node_set vset ATTRIBUTE_UNUSED)
{
  if (flag_indirect_inlining && !flag_ipa_cp)
    ipa_prop_write_jump_functions (set);
}

/* Release inline summary.  */

void
inline_free_summary (void)
{
  cgraph_remove_function_insertion_hook (function_insertion_hook_holder);
}
