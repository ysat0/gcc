/* A pass for lowering trees to RTL.
   Copyright (C) 2004 Free Software Foundation, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GCC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "tree.h"
#include "rtl.h"
#include "tm_p.h"
#include "basic-block.h"
#include "function.h"
#include "expr.h"
#include "langhooks.h"
#include "tree-flow.h"
#include "timevar.h"
#include "tree-dump.h"
#include "tree-pass.h"
#include "except.h"
#include "flags.h"


/* Expand variables in the unexpanded_var_list.  */

static void
expand_used_vars (void)
{
  tree cell;

  cfun->unexpanded_var_list = nreverse (cfun->unexpanded_var_list);

  for (cell = cfun->unexpanded_var_list; cell; cell = TREE_CHAIN (cell))
    expand_var (TREE_VALUE (cell));

  cfun->unexpanded_var_list = NULL_TREE;
}


/* A subroutine of expand_gimple_basic_block.  Expand one COND_EXPR.
   Returns a new basic block if we've terminated the current basic
   block and created a new one.  */

static basic_block
expand_gimple_cond_expr (basic_block bb, tree stmt)
{
  basic_block new_bb, dest;
  edge new_edge;
  edge true_edge;
  edge false_edge;
  tree pred = COND_EXPR_COND (stmt);
  tree then_exp = COND_EXPR_THEN (stmt);
  tree else_exp = COND_EXPR_ELSE (stmt);
  rtx last;

  extract_true_false_edges_from_block (bb, &true_edge, &false_edge);
  if (EXPR_LOCUS (stmt))
    {
      emit_line_note (*(EXPR_LOCUS (stmt)));
      record_block_change (TREE_BLOCK (stmt));
    }

  /* These flags have no purpose in RTL land.  */
  true_edge->flags &= ~EDGE_TRUE_VALUE;
  false_edge->flags &= ~EDGE_FALSE_VALUE;

  /* We can either have a pure conditional jump with one fallthru edge or
     two-way jump that needs to be decomposed into two basic blocks.  */
  if (TREE_CODE (then_exp) == GOTO_EXPR && IS_EMPTY_STMT (else_exp))
    {
      jumpif (pred, label_rtx (GOTO_DESTINATION (then_exp)));
      return NULL;
    }
  if (TREE_CODE (else_exp) == GOTO_EXPR && IS_EMPTY_STMT (then_exp))
    {
      jumpifnot (pred, label_rtx (GOTO_DESTINATION (else_exp)));
      return NULL;
    }
  if (TREE_CODE (then_exp) != GOTO_EXPR || TREE_CODE (else_exp) != GOTO_EXPR)
    abort ();

  jumpif (pred, label_rtx (GOTO_DESTINATION (then_exp)));
  last = get_last_insn ();
  expand_expr (else_exp, const0_rtx, VOIDmode, 0);

  BB_END (bb) = last;
  if (BARRIER_P (BB_END (bb)))
    BB_END (bb) = PREV_INSN (BB_END (bb));
  update_bb_for_insn (bb);

  new_bb = create_basic_block (NEXT_INSN (last), get_last_insn (), bb);
  dest = false_edge->dest;
  redirect_edge_succ (false_edge, new_bb);
  false_edge->flags |= EDGE_FALLTHRU;
  new_bb->count = false_edge->count;
  new_bb->frequency = EDGE_FREQUENCY (false_edge);
  new_edge = make_edge (new_bb, dest, 0);
  new_edge->probability = REG_BR_PROB_BASE;
  new_edge->count = new_bb->count;
  if (BARRIER_P (BB_END (new_bb)))
    BB_END (new_bb) = PREV_INSN (BB_END (new_bb));
  update_bb_for_insn (new_bb);

  if (dump_file)
    {
      dump_bb (bb, dump_file, 0);
      dump_bb (new_bb, dump_file, 0);
    }

  return new_bb;
}

/* A subroutine of expand_gimple_basic_block.  Expand one CALL_EXPR
   that has CALL_EXPR_TAILCALL set.  Returns non-null if we actually
   generated a tail call (something that might be denied by the ABI
   rules governing the call; see calls.c).

   Sets CAN_FALLTHRU if we generated a *conditional* tail call, and
   can still reach the rest of BB.  The case here is __builtin_sqrt,
   where the NaN result goes through the external function (with a
   tailcall) and the normal result happens via a sqrt instruction.  */

static basic_block
expand_gimple_tailcall (basic_block bb, tree stmt, bool *can_fallthru)
{
  rtx last = get_last_insn ();
  edge e;
  int probability;
  gcov_type count;

  expand_expr_stmt (stmt);

  for (last = NEXT_INSN (last); last; last = NEXT_INSN (last))
    if (CALL_P (last) && SIBLING_CALL_P (last))
      goto found;

  *can_fallthru = true;
  return NULL;

 found:
  /* ??? Wouldn't it be better to just reset any pending stack adjust?
     Any instructions emitted here are about to be deleted.  */
  do_pending_stack_adjust ();

  /* Remove any non-eh, non-abnormal edges that don't go to exit.  */
  /* ??? I.e. the fallthrough edge.  HOWEVER!  If there were to be
     EH or abnormal edges, we shouldn't have created a tail call in
     the first place.  So it seems to me we should just be removing
     all edges here, or redirecting the existing fallthru edge to
     the exit block.  */

  e = bb->succ;
  probability = 0;
  count = 0;
  while (e)
    {
      edge next = e->succ_next;

      if (!(e->flags & (EDGE_ABNORMAL | EDGE_EH)))
	{
	  if (e->dest != EXIT_BLOCK_PTR)
	    {
	      e->dest->count -= e->count;
	      e->dest->frequency -= EDGE_FREQUENCY (e);
	      if (e->dest->count < 0)
	        e->dest->count = 0;
	      if (e->dest->frequency < 0)
	        e->dest->frequency = 0;
	    }
	  count += e->count;
	  probability += e->probability;
	  remove_edge (e);
	}

      e = next;
    }

  /* This is somewhat ugly: the call_expr expander often emits instructions
     after the sibcall (to perform the function return).  These confuse the
     find_sub_basic_blocks code, so we need to get rid of these.  */
  last = NEXT_INSN (last);
  if (!BARRIER_P (last))
    abort ();

  *can_fallthru = false;
  while (NEXT_INSN (last))
    {
      /* For instance an sqrt builtin expander expands if with
	 sibcall in the then and label for `else`.  */
      if (LABEL_P (NEXT_INSN (last)))
	{
	  *can_fallthru = true;
	  break;
	}
      delete_insn (NEXT_INSN (last));
    }

  e = make_edge (bb, EXIT_BLOCK_PTR, EDGE_ABNORMAL | EDGE_SIBCALL);
  e->probability += probability;
  e->count += count;
  BB_END (bb) = last;
  update_bb_for_insn (bb);

  if (NEXT_INSN (last))
    {
      bb = create_basic_block (NEXT_INSN (last), get_last_insn (), bb);

      last = BB_END (bb);
      if (BARRIER_P (last))
	BB_END (bb) = PREV_INSN (last);
    }

  return bb;
}

/* Expand basic block BB from GIMPLE trees to RTL.  */

static basic_block
expand_gimple_basic_block (basic_block bb, FILE * dump_file)
{
  block_stmt_iterator bsi = bsi_start (bb);
  tree stmt = NULL;
  rtx note, last;
  edge e;

  if (dump_file)
    {
      tree_register_cfg_hooks ();
      dump_bb (bb, dump_file, 0);
      rtl_register_cfg_hooks ();
    }

  if (!bsi_end_p (bsi))
    stmt = bsi_stmt (bsi);

  if (stmt && TREE_CODE (stmt) == LABEL_EXPR)
    {
      last = get_last_insn ();

      expand_expr_stmt (stmt);

      /* Java emits line number notes in the top of labels.
         ??? Make this go away once line number notes are obsoleted.  */
      BB_HEAD (bb) = NEXT_INSN (last);
      if (NOTE_P (BB_HEAD (bb)))
	BB_HEAD (bb) = NEXT_INSN (BB_HEAD (bb));
      bsi_next (&bsi);
      note = emit_note_after (NOTE_INSN_BASIC_BLOCK, BB_HEAD (bb));
    }
  else
    note = BB_HEAD (bb) = emit_note (NOTE_INSN_BASIC_BLOCK);

  NOTE_BASIC_BLOCK (note) = bb;

  e = bb->succ;
  while (e)
    {
      edge next = e->succ_next;

      /* Clear EDGE_EXECUTABLE.  This flag is never used in the backend.  */
      e->flags &= ~EDGE_EXECUTABLE;

      /* At the moment not all abnormal edges match the RTL representation.
         It is safe to remove them here as find_sub_basic_blocks will
         rediscover them.  In the future we should get this fixed properly.  */
      if (e->flags & EDGE_ABNORMAL)
	remove_edge (e);

      e = next;
    }

  for (; !bsi_end_p (bsi); bsi_next (&bsi))
    {
      tree stmt = bsi_stmt (bsi);
      basic_block new_bb;

      if (!stmt)
	continue;

      /* Expand this statement, then evaluate the resulting RTL and
	 fixup the CFG accordingly.  */
      if (TREE_CODE (stmt) == COND_EXPR)
	{
	  new_bb = expand_gimple_cond_expr (bb, stmt);
	  if (new_bb)
	    return new_bb;
	}
      else
	{
	  tree call = get_call_expr_in (stmt);
	  if (call && CALL_EXPR_TAILCALL (call))
	    {
	      bool can_fallthru;
	      new_bb = expand_gimple_tailcall (bb, stmt, &can_fallthru);
	      if (new_bb)
		{
		  if (can_fallthru)
		    bb = new_bb;
		  else
		    return new_bb;
		}
	    }
	  else
	    expand_expr_stmt (stmt);
	}
    }

  do_pending_stack_adjust ();

  /* Find the the block tail.  The last insn is the block is the insn
     before a barrier and/or table jump insn.  */
  last = get_last_insn ();
  if (BARRIER_P (last))
    last = PREV_INSN (last);
  if (JUMP_TABLE_DATA_P (last))
    last = PREV_INSN (PREV_INSN (last));
  BB_END (bb) = last;

  if (dump_file)
    dump_bb (bb, dump_file, 0);
  update_bb_for_insn (bb);

  return bb;
}


/* Create a basic block for initialization code.  */

static basic_block
construct_init_block (void)
{
  basic_block init_block, first_block;
  edge e;

  for (e = ENTRY_BLOCK_PTR->succ; e; e = e->succ_next)
    if (e->dest == ENTRY_BLOCK_PTR->next_bb)
      break;

  init_block = create_basic_block (NEXT_INSN (get_insns ()),
				   get_last_insn (),
				   ENTRY_BLOCK_PTR);
  init_block->frequency = ENTRY_BLOCK_PTR->frequency;
  init_block->count = ENTRY_BLOCK_PTR->count;
  if (e)
    {
      first_block = e->dest;
      redirect_edge_succ (e, init_block);
      e = make_edge (init_block, first_block, EDGE_FALLTHRU);
    }
  else
    e = make_edge (init_block, EXIT_BLOCK_PTR, EDGE_FALLTHRU);
  e->probability = REG_BR_PROB_BASE;
  e->count = ENTRY_BLOCK_PTR->count;

  update_bb_for_insn (init_block);
  return init_block;
}


/* Create a block containing landing pads and similar stuff.  */

static void
construct_exit_block (void)
{
  rtx head = get_last_insn ();
  rtx end;
  basic_block exit_block;
  edge e, e2, next;

  /* Make sure the locus is set to the end of the function, so that
     epilogue line numbers and warnings are set properly.  */
#ifdef USE_MAPPED_LOCATION
  if (cfun->function_end_locus != UNKNOWN_LOCATION)
#else
  if (cfun->function_end_locus.file)
#endif
    input_location = cfun->function_end_locus;

  /* The following insns belong to the top scope.  */
  record_block_change (DECL_INITIAL (current_function_decl));

  /* Generate rtl for function exit.  */
  expand_function_end ();

  end = get_last_insn ();
  if (head == end)
    return;
  while (NEXT_INSN (head) && NOTE_P (NEXT_INSN (head)))
    head = NEXT_INSN (head);
  exit_block = create_basic_block (NEXT_INSN (head), end,
				   EXIT_BLOCK_PTR->prev_bb);
  exit_block->frequency = EXIT_BLOCK_PTR->frequency;
  exit_block->count = EXIT_BLOCK_PTR->count;
  for (e = EXIT_BLOCK_PTR->pred; e; e = next)
    {
      next = e->pred_next;
      if (!(e->flags & EDGE_ABNORMAL))
        redirect_edge_succ (e, exit_block);
    }
  e = make_edge (exit_block, EXIT_BLOCK_PTR, EDGE_FALLTHRU);
  e->probability = REG_BR_PROB_BASE;
  e->count = EXIT_BLOCK_PTR->count;
  for (e2 = EXIT_BLOCK_PTR->pred; e2; e2 = e2->pred_next)
    if (e2 != e)
      {
        e->count -= e2->count;
	exit_block->count -= e2->count;
	exit_block->frequency -= EDGE_FREQUENCY (e2);
      }
  if (e->count < 0)
    e->count = 0;
  if (exit_block->count < 0)
    exit_block->count = 0;
  if (exit_block->frequency < 0)
    exit_block->frequency = 0;
  update_bb_for_insn (exit_block);
}

/* Translate the intermediate representation contained in the CFG
   from GIMPLE trees to RTL.

   We do conversion per basic block and preserve/update the tree CFG.
   This implies we have to do some magic as the CFG can simultaneously
   consist of basic blocks containing RTL and GIMPLE trees.  This can
   confuse the CFG hooks, so be careful to not manipulate CFG during
   the expansion.  */

static void
tree_expand_cfg (void)
{
  basic_block bb, init_block;
  sbitmap blocks;

  if (dump_file)
    {
      fprintf (dump_file, "\n;; Function %s",
	       (*lang_hooks.decl_printable_name) (current_function_decl, 2));
      fprintf (dump_file, " (%s)\n",
	       IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (current_function_decl)));
    }

  profile_status = PROFILE_ABSENT;

  /* Some backends want to know that we are expanding to RTL.  */
  currently_expanding_to_rtl = 1;

  /* Prepare the rtl middle end to start recording block changes.  */
  reset_block_changes ();

  /* Expand the variables recorded during gimple lowering.  */
  expand_used_vars ();

  /* Set up parameters and prepare for return, for the function.  */
  expand_function_start (current_function_decl);

  /* If this function is `main', emit a call to `__main'
     to run global initializers, etc.  */
  if (DECL_NAME (current_function_decl)
      && MAIN_NAME_P (DECL_NAME (current_function_decl))
      && DECL_FILE_SCOPE_P (current_function_decl))
    expand_main_function ();

  /* Register rtl specific functions for cfg.  */
  rtl_register_cfg_hooks ();

  init_block = construct_init_block ();

  FOR_BB_BETWEEN (bb, init_block->next_bb, EXIT_BLOCK_PTR, next_bb)
    bb = expand_gimple_basic_block (bb, dump_file);

  construct_exit_block ();

  /* We're done expanding trees to RTL.  */
  currently_expanding_to_rtl = 0;

  /* Convert from NOTE_INSN_EH_REGION style notes, and do other
     sorts of eh initialization.  Delay this until after the
     initial rtl dump so that we can see the original nesting.  */
  convert_from_eh_region_ranges ();

  rebuild_jump_labels (get_insns ());
  find_exception_handler_labels ();

  blocks = sbitmap_alloc (last_basic_block);
  sbitmap_ones (blocks);
  find_many_sub_basic_blocks (blocks);
  purge_all_dead_edges (0);
  sbitmap_free (blocks);

  compact_blocks ();
#ifdef ENABLE_CHECKING
  verify_flow_info();
#endif
}

struct tree_opt_pass pass_expand =
{
  "expand",		                /* name */
  NULL,                                 /* gate */
  tree_expand_cfg,	                /* execute */
  NULL,                                 /* sub */
  NULL,                                 /* next */
  0,                                    /* static_pass_number */
  TV_EXPAND,		                /* tv_id */
  /* ??? If TER is enabled, we actually receive GENERIC.  */
  PROP_gimple_leh | PROP_cfg,           /* properties_required */
  PROP_rtl,                             /* properties_provided */
  PROP_gimple_leh,			/* properties_destroyed */
  0,                                    /* todo_flags_start */
  0                                     /* todo_flags_finish */
};
