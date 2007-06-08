/* Analysis Utilities for Loop Vectorization.
   Copyright (C) 2003, 2004, 2005, 2006, 2007 Free Software Foundation, Inc.
   Contributed by Dorit Naishlos <dorit@il.ibm.com>

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

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "ggc.h"
#include "tree.h"
#include "basic-block.h"
#include "diagnostic.h"
#include "tree-flow.h"
#include "tree-dump.h"
#include "timevar.h"
#include "cfgloop.h"
#include "expr.h"
#include "optabs.h"
#include "params.h"
#include "tree-chrec.h"
#include "tree-data-ref.h"
#include "tree-scalar-evolution.h"
#include "tree-vectorizer.h"
#include "toplev.h"

/* Main analysis functions.  */
static loop_vec_info vect_analyze_loop_form (struct loop *);
static bool vect_analyze_data_refs (loop_vec_info);
static bool vect_mark_stmts_to_be_vectorized (loop_vec_info);
static void vect_analyze_scalar_cycles (loop_vec_info);
static bool vect_analyze_data_ref_accesses (loop_vec_info);
static bool vect_analyze_data_ref_dependences (loop_vec_info);
static bool vect_analyze_data_refs_alignment (loop_vec_info);
static bool vect_compute_data_refs_alignment (loop_vec_info);
static bool vect_enhance_data_refs_alignment (loop_vec_info);
static bool vect_analyze_operations (loop_vec_info);
static bool vect_determine_vectorization_factor (loop_vec_info);

/* Utility functions for the analyses.  */
static bool exist_non_indexing_operands_for_use_p (tree, tree);
static tree vect_get_loop_niters (struct loop *, tree *);
static bool vect_analyze_data_ref_dependence
  (struct data_dependence_relation *, loop_vec_info);
static bool vect_compute_data_ref_alignment (struct data_reference *); 
static bool vect_analyze_data_ref_access (struct data_reference *);
static bool vect_can_advance_ivs_p (loop_vec_info);
static void vect_update_misalignment_for_peel
  (struct data_reference *, struct data_reference *, int npeel);

/* Function vect_determine_vectorization_factor

   Determine the vectorization factor (VF). VF is the number of data elements
   that are operated upon in parallel in a single iteration of the vectorized
   loop. For example, when vectorizing a loop that operates on 4byte elements,
   on a target with vector size (VS) 16byte, the VF is set to 4, since 4
   elements can fit in a single vector register.

   We currently support vectorization of loops in which all types operated upon
   are of the same size. Therefore this function currently sets VF according to
   the size of the types operated upon, and fails if there are multiple sizes
   in the loop.

   VF is also the factor by which the loop iterations are strip-mined, e.g.:
   original loop:
        for (i=0; i<N; i++){
          a[i] = b[i] + c[i];
        }

   vectorized loop:
        for (i=0; i<N; i+=VF){
          a[i:VF] = b[i:VF] + c[i:VF];
        }
*/

static bool
vect_determine_vectorization_factor (loop_vec_info loop_vinfo)
{
  struct loop *loop = LOOP_VINFO_LOOP (loop_vinfo);
  basic_block *bbs = LOOP_VINFO_BBS (loop_vinfo);
  int nbbs = loop->num_nodes;
  block_stmt_iterator si;
  unsigned int vectorization_factor = 0;
  tree scalar_type;
  tree phi;
  tree vectype;
  unsigned int nunits;
  stmt_vec_info stmt_info;
  int i;

  if (vect_print_dump_info (REPORT_DETAILS))
    fprintf (vect_dump, "=== vect_determine_vectorization_factor ===");

  for (i = 0; i < nbbs; i++)
    {
      basic_block bb = bbs[i];

      for (phi = phi_nodes (bb); phi; phi = PHI_CHAIN (phi))
	{
	  stmt_info = vinfo_for_stmt (phi);
	  if (vect_print_dump_info (REPORT_DETAILS))
	    {
	      fprintf (vect_dump, "==> examining phi: ");
	      print_generic_expr (vect_dump, phi, TDF_SLIM);
	    }

	  gcc_assert (stmt_info);

	  if (STMT_VINFO_RELEVANT_P (stmt_info))
            {
	      gcc_assert (!STMT_VINFO_VECTYPE (stmt_info));
              scalar_type = TREE_TYPE (PHI_RESULT (phi));

	      if (vect_print_dump_info (REPORT_DETAILS))
		{
		  fprintf (vect_dump, "get vectype for scalar type:  ");
		  print_generic_expr (vect_dump, scalar_type, TDF_SLIM);
		}

	      vectype = get_vectype_for_scalar_type (scalar_type);
	      if (!vectype)
		{
		  if (vect_print_dump_info (REPORT_UNVECTORIZED_LOOPS))
		    {
		      fprintf (vect_dump,
		               "not vectorized: unsupported data-type ");
		      print_generic_expr (vect_dump, scalar_type, TDF_SLIM);
		    }
		  return false;
		}
	      STMT_VINFO_VECTYPE (stmt_info) = vectype;

	      if (vect_print_dump_info (REPORT_DETAILS))
		{
		  fprintf (vect_dump, "vectype: ");
		  print_generic_expr (vect_dump, vectype, TDF_SLIM);
		}

	      nunits = TYPE_VECTOR_SUBPARTS (vectype);
	      if (vect_print_dump_info (REPORT_DETAILS))
		fprintf (vect_dump, "nunits = %d", nunits);

	      if (!vectorization_factor
		  || (nunits > vectorization_factor))
		vectorization_factor = nunits;
	    }
	}

      for (si = bsi_start (bb); !bsi_end_p (si); bsi_next (&si))
        {
	  tree stmt = bsi_stmt (si);
	  stmt_info = vinfo_for_stmt (stmt);

	  if (vect_print_dump_info (REPORT_DETAILS))
	    {
	      fprintf (vect_dump, "==> examining statement: ");
	      print_generic_expr (vect_dump, stmt, TDF_SLIM);
	    }

	  gcc_assert (stmt_info);

	  /* skip stmts which do not need to be vectorized.  */
	  if (!STMT_VINFO_RELEVANT_P (stmt_info)
	      && !STMT_VINFO_LIVE_P (stmt_info))
	    {
	      if (vect_print_dump_info (REPORT_DETAILS))
	        fprintf (vect_dump, "skip.");
	      continue;
	    }

	  if (TREE_CODE (stmt) != GIMPLE_MODIFY_STMT)
	    {
	      if (vect_print_dump_info (REPORT_UNVECTORIZED_LOOPS))
		{
	          fprintf (vect_dump, "not vectorized: irregular stmt.");
		  print_generic_expr (vect_dump, stmt, TDF_SLIM);
		}
	      return false;
	    }

	  if (!GIMPLE_STMT_P (stmt)
	      && VECTOR_MODE_P (TYPE_MODE (TREE_TYPE (stmt))))
	    {
	      if (vect_print_dump_info (REPORT_UNVECTORIZED_LOOPS))
	        {
	          fprintf (vect_dump, "not vectorized: vector stmt in loop:");
	          print_generic_expr (vect_dump, stmt, TDF_SLIM);
	        }
	      return false;
	    }

	  if (STMT_VINFO_VECTYPE (stmt_info))
	    {
	      /* The only case when a vectype had been already set is for stmts 
	         that contain a dataref, or for "pattern-stmts" (stmts generated
		 by the vectorizer to represent/replace a certain idiom).  */
	      gcc_assert (STMT_VINFO_DATA_REF (stmt_info) 
			  || is_pattern_stmt_p (stmt_info));
	      vectype = STMT_VINFO_VECTYPE (stmt_info);
	    }
	  else
	    {
	      gcc_assert (! STMT_VINFO_DATA_REF (stmt_info)
			  && !is_pattern_stmt_p (stmt_info));

	      /* We set the vectype according to the type of the result (lhs).
		 For stmts whose result-type is different than the type of the
		 arguments (e.g. demotion, promotion), vectype will be reset 
		 appropriately (later).  Note that we have to visit the smallest 
		 datatype in this function, because that determines the VF.  
		 If the smallest datatype in the loop is present only as the 
		 rhs of a promotion operation - we'd miss it here.
		 However, in such a case, that a variable of this datatype
		 does not appear in the lhs anywhere in the loop, it shouldn't
		 affect the vectorization factor.   */
	      scalar_type = TREE_TYPE (GIMPLE_STMT_OPERAND (stmt, 0));

	      if (vect_print_dump_info (REPORT_DETAILS))
		{
		  fprintf (vect_dump, "get vectype for scalar type:  ");
		  print_generic_expr (vect_dump, scalar_type, TDF_SLIM);
		}

	      vectype = get_vectype_for_scalar_type (scalar_type);
	      if (!vectype)
		{
		  if (vect_print_dump_info (REPORT_UNVECTORIZED_LOOPS))
		    {
		      fprintf (vect_dump, 
			       "not vectorized: unsupported data-type ");
		      print_generic_expr (vect_dump, scalar_type, TDF_SLIM);
		    }
		  return false;
		}
	      STMT_VINFO_VECTYPE (stmt_info) = vectype;
            }

	  if (vect_print_dump_info (REPORT_DETAILS))
	    {
	      fprintf (vect_dump, "vectype: ");
	      print_generic_expr (vect_dump, vectype, TDF_SLIM);
	    }

	  nunits = TYPE_VECTOR_SUBPARTS (vectype);
	  if (vect_print_dump_info (REPORT_DETAILS))
	    fprintf (vect_dump, "nunits = %d", nunits);

	  if (!vectorization_factor
	      || (nunits > vectorization_factor))
	    vectorization_factor = nunits;

        }
    }

  /* TODO: Analyze cost. Decide if worth while to vectorize.  */
  if (vect_print_dump_info (REPORT_DETAILS))
    fprintf (vect_dump, "vectorization factor = %d", vectorization_factor);
  if (vectorization_factor <= 1)
    {
      if (vect_print_dump_info (REPORT_UNVECTORIZED_LOOPS))
        fprintf (vect_dump, "not vectorized: unsupported data-type");
      return false;
    }
  LOOP_VINFO_VECT_FACTOR (loop_vinfo) = vectorization_factor;

  return true;
}


/* Function vect_analyze_operations.

   Scan the loop stmts and make sure they are all vectorizable.  */

static bool
vect_analyze_operations (loop_vec_info loop_vinfo)
{
  struct loop *loop = LOOP_VINFO_LOOP (loop_vinfo);
  basic_block *bbs = LOOP_VINFO_BBS (loop_vinfo);
  int nbbs = loop->num_nodes;
  block_stmt_iterator si;
  unsigned int vectorization_factor = 0;
  int i;
  bool ok;
  tree phi;
  stmt_vec_info stmt_info;
  bool need_to_vectorize = false;
  int min_profitable_iters;
  int min_scalar_loop_bound;
  unsigned int th;

  if (vect_print_dump_info (REPORT_DETAILS))
    fprintf (vect_dump, "=== vect_analyze_operations ===");

  gcc_assert (LOOP_VINFO_VECT_FACTOR (loop_vinfo));
  vectorization_factor = LOOP_VINFO_VECT_FACTOR (loop_vinfo);

  for (i = 0; i < nbbs; i++)
    {
      basic_block bb = bbs[i];

      for (phi = phi_nodes (bb); phi; phi = PHI_CHAIN (phi))
        {
	  ok = true;

	  stmt_info = vinfo_for_stmt (phi);
	  if (vect_print_dump_info (REPORT_DETAILS))
	    {
	      fprintf (vect_dump, "examining phi: ");
	      print_generic_expr (vect_dump, phi, TDF_SLIM);
	    }

	  gcc_assert (stmt_info);

	  if (STMT_VINFO_LIVE_P (stmt_info))
	    {
	      /* FORNOW: not yet supported.  */
	      if (vect_print_dump_info (REPORT_UNVECTORIZED_LOOPS))
		fprintf (vect_dump, "not vectorized: value used after loop.");
	      return false;
	    }

	  if (STMT_VINFO_RELEVANT (stmt_info) == vect_used_in_loop
	      && STMT_VINFO_DEF_TYPE (stmt_info) != vect_induction_def)
	    {
	      /* A scalar-dependence cycle that we don't support.  */
	      if (vect_print_dump_info (REPORT_UNVECTORIZED_LOOPS))
		fprintf (vect_dump, "not vectorized: scalar dependence cycle.");
	      return false;
	    }

	  if (STMT_VINFO_RELEVANT_P (stmt_info))
	    {
	      need_to_vectorize = true;
	      if (STMT_VINFO_DEF_TYPE (stmt_info) == vect_induction_def)
		ok = vectorizable_induction (phi, NULL, NULL);
	    }

	  if (!ok)
	    {
	      if (vect_print_dump_info (REPORT_UNVECTORIZED_LOOPS))
		{
		  fprintf (vect_dump,
			   "not vectorized: relevant phi not supported: ");
		  print_generic_expr (vect_dump, phi, TDF_SLIM);
		}
	      return false;
	    }
	}

      for (si = bsi_start (bb); !bsi_end_p (si); bsi_next (&si))
	{
	  tree stmt = bsi_stmt (si);
	  stmt_vec_info stmt_info = vinfo_for_stmt (stmt);
	  enum vect_def_type relevance = STMT_VINFO_RELEVANT (stmt_info);

	  if (vect_print_dump_info (REPORT_DETAILS))
	    {
	      fprintf (vect_dump, "==> examining statement: ");
	      print_generic_expr (vect_dump, stmt, TDF_SLIM);
	    }

	  gcc_assert (stmt_info);

	  /* skip stmts which do not need to be vectorized.
	     this is expected to include:
	     - the COND_EXPR which is the loop exit condition
	     - any LABEL_EXPRs in the loop
	     - computations that are used only for array indexing or loop
	     control  */

	  if (!STMT_VINFO_RELEVANT_P (stmt_info)
	      && !STMT_VINFO_LIVE_P (stmt_info))
	    {
	      if (vect_print_dump_info (REPORT_DETAILS))
	        fprintf (vect_dump, "irrelevant.");
	      continue;
	    }

	  switch (STMT_VINFO_DEF_TYPE (stmt_info))
	    {
 	    case vect_loop_def:
	      break;
	
	    case vect_reduction_def:
	      gcc_assert (relevance == vect_unused_in_loop);
	      break;	

	    case vect_induction_def:
	    case vect_constant_def:
	    case vect_invariant_def:
	    case vect_unknown_def_type:
	    default:
	      gcc_unreachable ();	
	    }

	  if (STMT_VINFO_RELEVANT_P (stmt_info))
	    {
	      gcc_assert (GIMPLE_STMT_P (stmt)
			  || !VECTOR_MODE_P (TYPE_MODE (TREE_TYPE (stmt))));
	      gcc_assert (STMT_VINFO_VECTYPE (stmt_info));
	      need_to_vectorize = true;
	    }

	  ok = (vectorizable_type_promotion (stmt, NULL, NULL)
		|| vectorizable_type_demotion (stmt, NULL, NULL)
		|| vectorizable_conversion (stmt, NULL, NULL)
		|| vectorizable_operation (stmt, NULL, NULL)
		|| vectorizable_assignment (stmt, NULL, NULL)
		|| vectorizable_load (stmt, NULL, NULL)
		|| vectorizable_call (stmt, NULL, NULL)
		|| vectorizable_store (stmt, NULL, NULL)
		|| vectorizable_condition (stmt, NULL, NULL)
		|| vectorizable_reduction (stmt, NULL, NULL));

	  /* Stmts that are (also) "live" (i.e. - that are used out of the loop)
	     need extra handling, except for vectorizable reductions.  */
	  if (STMT_VINFO_LIVE_P (stmt_info)
	      && STMT_VINFO_TYPE (stmt_info) != reduc_vec_info_type) 
	    ok |= vectorizable_live_operation (stmt, NULL, NULL);

	  if (!ok)
	    {
	      if (vect_print_dump_info (REPORT_UNVECTORIZED_LOOPS))
		{
		  fprintf (vect_dump, "not vectorized: stmt not supported: ");
		  print_generic_expr (vect_dump, stmt, TDF_SLIM);
		}
	      return false;
	    }	
	} /* stmts in bb */
    } /* bbs */

  /* All operations in the loop are either irrelevant (deal with loop
     control, or dead), or only used outside the loop and can be moved
     out of the loop (e.g. invariants, inductions).  The loop can be 
     optimized away by scalar optimizations.  We're better off not 
     touching this loop.  */
  if (!need_to_vectorize)
    {
      if (vect_print_dump_info (REPORT_DETAILS))
	fprintf (vect_dump, 
		 "All the computation can be taken out of the loop.");
      if (vect_print_dump_info (REPORT_UNVECTORIZED_LOOPS))
        fprintf (vect_dump, 
		 "not vectorized: redundant loop. no profit to vectorize.");
      return false;
    }

  if (LOOP_VINFO_NITERS_KNOWN_P (loop_vinfo)
      && vect_print_dump_info (REPORT_DETAILS))
    fprintf (vect_dump,
        "vectorization_factor = %d, niters = " HOST_WIDE_INT_PRINT_DEC,
        vectorization_factor, LOOP_VINFO_INT_NITERS (loop_vinfo));

  if (LOOP_VINFO_NITERS_KNOWN_P (loop_vinfo)
      && (LOOP_VINFO_INT_NITERS (loop_vinfo) < vectorization_factor))
    {
      if (vect_print_dump_info (REPORT_UNVECTORIZED_LOOPS))
        fprintf (vect_dump, "not vectorized: iteration count too small.");
      if (vect_print_dump_info (REPORT_DETAILS))
        fprintf (vect_dump,"not vectorized: iteration count smaller than "
                 "vectorization factor.");
      return false;
    }

  /* Analyze cost. Decide if worth while to vectorize.  */

  min_profitable_iters = vect_estimate_min_profitable_iters (loop_vinfo);

  if (min_profitable_iters < 0)
    {
      if (vect_print_dump_info (REPORT_UNVECTORIZED_LOOPS))
        fprintf (vect_dump, "not vectorized: vectorization not profitable.");
      if (vect_print_dump_info (REPORT_DETAILS))
        fprintf (vect_dump, "not vectorized: vector version will never be "
                 "profitable.");
      return false;
    }

  min_scalar_loop_bound = (PARAM_VALUE (PARAM_MIN_VECT_LOOP_BOUND))
                          * vectorization_factor;

  /* Use the cost model only if it is more conservative than user specified
     threshold.  */

  th = (unsigned) min_scalar_loop_bound;
  if (min_profitable_iters 
      && (!min_scalar_loop_bound
          || min_profitable_iters > min_scalar_loop_bound))
    th = (unsigned) min_profitable_iters;

  if (LOOP_VINFO_NITERS_KNOWN_P (loop_vinfo)
      && LOOP_VINFO_INT_NITERS (loop_vinfo) < th)
    {
      if (vect_print_dump_info (REPORT_UNVECTORIZED_LOOPS))	      
        fprintf (vect_dump, "not vectorized: vectorization not "
                 "profitable.");
      if (vect_print_dump_info (REPORT_DETAILS))	      
        fprintf (vect_dump, "not vectorized: iteration count smaller than "
                 "user specified loop bound parameter or minimum "
                 "profitable iterations (whichever is more conservative).");
      return false;
    }  

  if (!LOOP_VINFO_NITERS_KNOWN_P (loop_vinfo)
      || LOOP_VINFO_INT_NITERS (loop_vinfo) % vectorization_factor != 0
      || LOOP_PEELING_FOR_ALIGNMENT (loop_vinfo))
    {
      if (vect_print_dump_info (REPORT_DETAILS))
        fprintf (vect_dump, "epilog loop required.");
      if (!vect_can_advance_ivs_p (loop_vinfo))
        {
          if (vect_print_dump_info (REPORT_UNVECTORIZED_LOOPS))
            fprintf (vect_dump,
                     "not vectorized: can't create epilog loop 1.");
          return false;
        }
      if (!slpeel_can_duplicate_loop_p (loop, single_exit (loop)))
        {
          if (vect_print_dump_info (REPORT_UNVECTORIZED_LOOPS))
            fprintf (vect_dump,
                     "not vectorized: can't create epilog loop 2.");
          return false;
        }
    }

  return true;
}


/* Function exist_non_indexing_operands_for_use_p 

   USE is one of the uses attached to STMT. Check if USE is 
   used in STMT for anything other than indexing an array.  */

static bool
exist_non_indexing_operands_for_use_p (tree use, tree stmt)
{
  tree operand;
  stmt_vec_info stmt_info = vinfo_for_stmt (stmt);
 
  /* USE corresponds to some operand in STMT. If there is no data
     reference in STMT, then any operand that corresponds to USE
     is not indexing an array.  */
  if (!STMT_VINFO_DATA_REF (stmt_info))
    return true;
 
  /* STMT has a data_ref. FORNOW this means that its of one of
     the following forms:
     -1- ARRAY_REF = var
     -2- var = ARRAY_REF
     (This should have been verified in analyze_data_refs).

     'var' in the second case corresponds to a def, not a use,
     so USE cannot correspond to any operands that are not used 
     for array indexing.

     Therefore, all we need to check is if STMT falls into the
     first case, and whether var corresponds to USE.  */
 
  if (TREE_CODE (GIMPLE_STMT_OPERAND (stmt, 0)) == SSA_NAME)
    return false;

  operand = GIMPLE_STMT_OPERAND (stmt, 1);

  if (TREE_CODE (operand) != SSA_NAME)
    return false;

  if (operand == use)
    return true;

  return false;
}


/* Function vect_analyze_scalar_cycles.

   Examine the cross iteration def-use cycles of scalar variables, by
   analyzing the loop (scalar) PHIs; Classify each cycle as one of the
   following: invariant, induction, reduction, unknown.
   
   Some forms of scalar cycles are not yet supported.

   Example1: reduction: (unsupported yet)

              loop1:
              for (i=0; i<N; i++)
                 sum += a[i];

   Example2: induction: (unsupported yet)

              loop2:
              for (i=0; i<N; i++)
                 a[i] = i;

   Note: the following loop *is* vectorizable:

              loop3:
              for (i=0; i<N; i++)
                 a[i] = b[i];

         even though it has a def-use cycle caused by the induction variable i:

              loop: i_2 = PHI (i_0, i_1)
                    a[i_2] = ...;
                    i_1 = i_2 + 1;
                    GOTO loop;

         because the def-use cycle in loop3 is considered "not relevant" - i.e.,
         it does not need to be vectorized because it is only used for array
         indexing (see 'mark_stmts_to_be_vectorized'). The def-use cycle in
         loop2 on the other hand is relevant (it is being written to memory).
*/

static void
vect_analyze_scalar_cycles (loop_vec_info loop_vinfo)
{
  tree phi;
  struct loop *loop = LOOP_VINFO_LOOP (loop_vinfo);
  basic_block bb = loop->header;
  tree dumy;
  VEC(tree,heap) *worklist = VEC_alloc (tree, heap, 64);

  if (vect_print_dump_info (REPORT_DETAILS))
    fprintf (vect_dump, "=== vect_analyze_scalar_cycles ===");

  /* First - identify all inductions.  */
  for (phi = phi_nodes (bb); phi; phi = PHI_CHAIN (phi))
    {
      tree access_fn = NULL;
      tree def = PHI_RESULT (phi);
      stmt_vec_info stmt_vinfo = vinfo_for_stmt (phi);

      if (vect_print_dump_info (REPORT_DETAILS))
	{
	  fprintf (vect_dump, "Analyze phi: ");
	  print_generic_expr (vect_dump, phi, TDF_SLIM);
	}

      /* Skip virtual phi's. The data dependences that are associated with
         virtual defs/uses (i.e., memory accesses) are analyzed elsewhere.  */
      if (!is_gimple_reg (SSA_NAME_VAR (def)))
	continue;

      STMT_VINFO_DEF_TYPE (stmt_vinfo) = vect_unknown_def_type;

      /* Analyze the evolution function.  */
      access_fn = analyze_scalar_evolution (loop, def);
      if (access_fn && vect_print_dump_info (REPORT_DETAILS))
	{
	  fprintf (vect_dump, "Access function of PHI: ");
	  print_generic_expr (vect_dump, access_fn, TDF_SLIM);
	}

      if (!access_fn
	  || !vect_is_simple_iv_evolution (loop->num, access_fn, &dumy, &dumy)) 
	{
	  VEC_safe_push (tree, heap, worklist, phi);	  
	  continue;
	}

      if (vect_print_dump_info (REPORT_DETAILS))
	fprintf (vect_dump, "Detected induction.");
      STMT_VINFO_DEF_TYPE (stmt_vinfo) = vect_induction_def;
    }


  /* Second - identify all reductions.  */
  while (VEC_length (tree, worklist) > 0)
    {
      tree phi = VEC_pop (tree, worklist);
      tree def = PHI_RESULT (phi);
      stmt_vec_info stmt_vinfo = vinfo_for_stmt (phi);
      tree reduc_stmt;

      if (vect_print_dump_info (REPORT_DETAILS))
        { 
          fprintf (vect_dump, "Analyze phi: ");
          print_generic_expr (vect_dump, phi, TDF_SLIM);
        }

      gcc_assert (is_gimple_reg (SSA_NAME_VAR (def)));
      gcc_assert (STMT_VINFO_DEF_TYPE (stmt_vinfo) == vect_unknown_def_type);

      reduc_stmt = vect_is_simple_reduction (loop, phi);
      if (reduc_stmt)
        {
          if (vect_print_dump_info (REPORT_DETAILS))
            fprintf (vect_dump, "Detected reduction.");
          STMT_VINFO_DEF_TYPE (stmt_vinfo) = vect_reduction_def;
          STMT_VINFO_DEF_TYPE (vinfo_for_stmt (reduc_stmt)) =
                                                        vect_reduction_def;
        }
      else
        if (vect_print_dump_info (REPORT_DETAILS))
          fprintf (vect_dump, "Unknown def-use cycle pattern.");
    }

  VEC_free (tree, heap, worklist);
  return;
}


/* Function vect_insert_into_interleaving_chain.

   Insert DRA into the interleaving chain of DRB according to DRA's INIT.  */

static void
vect_insert_into_interleaving_chain (struct data_reference *dra,
				     struct data_reference *drb)
{
  tree prev, next, next_init;
  stmt_vec_info stmtinfo_a = vinfo_for_stmt (DR_STMT (dra)); 
  stmt_vec_info stmtinfo_b = vinfo_for_stmt (DR_STMT (drb));

  prev = DR_GROUP_FIRST_DR (stmtinfo_b);
  next = DR_GROUP_NEXT_DR (vinfo_for_stmt (prev));		  
  while (next)
    {
      next_init = DR_INIT (STMT_VINFO_DATA_REF (vinfo_for_stmt (next)));
      if (tree_int_cst_compare (next_init, DR_INIT (dra)) > 0)
	{
	  /* Insert here.  */
	  DR_GROUP_NEXT_DR (vinfo_for_stmt (prev)) = DR_STMT (dra);
	  DR_GROUP_NEXT_DR (stmtinfo_a) = next;
	  return;
	}
      prev = next;
      next = DR_GROUP_NEXT_DR (vinfo_for_stmt (prev));
    }

  /* We got to the end of the list. Insert here.  */
  DR_GROUP_NEXT_DR (vinfo_for_stmt (prev)) = DR_STMT (dra);
  DR_GROUP_NEXT_DR (stmtinfo_a) = NULL_TREE;
}


/* Function vect_update_interleaving_chain.
   
   For two data-refs DRA and DRB that are a part of a chain interleaved data 
   accesses, update the interleaving chain. DRB's INIT is smaller than DRA's.

   There are four possible cases:
   1. New stmts - both DRA and DRB are not a part of any chain:
      FIRST_DR = DRB
      NEXT_DR (DRB) = DRA
   2. DRB is a part of a chain and DRA is not:
      no need to update FIRST_DR
      no need to insert DRB
      insert DRA according to init
   3. DRA is a part of a chain and DRB is not:
      if (init of FIRST_DR > init of DRB)
          FIRST_DR = DRB
	  NEXT(FIRST_DR) = previous FIRST_DR
      else
          insert DRB according to its init
   4. both DRA and DRB are in some interleaving chains:
      choose the chain with the smallest init of FIRST_DR
      insert the nodes of the second chain into the first one.  */

static void
vect_update_interleaving_chain (struct data_reference *drb,
				struct data_reference *dra)
{
  stmt_vec_info stmtinfo_a = vinfo_for_stmt (DR_STMT (dra)); 
  stmt_vec_info stmtinfo_b = vinfo_for_stmt (DR_STMT (drb));
  tree next_init, init_dra_chain, init_drb_chain, first_a, first_b;
  tree node, prev, next, node_init, first_stmt;

  /* 1. New stmts - both DRA and DRB are not a part of any chain.   */
  if (!DR_GROUP_FIRST_DR (stmtinfo_a) && !DR_GROUP_FIRST_DR (stmtinfo_b))
    {
      DR_GROUP_FIRST_DR (stmtinfo_a) = DR_STMT (drb);
      DR_GROUP_FIRST_DR (stmtinfo_b) = DR_STMT (drb);
      DR_GROUP_NEXT_DR (stmtinfo_b) = DR_STMT (dra);
      return;
    }

  /* 2. DRB is a part of a chain and DRA is not.  */
  if (!DR_GROUP_FIRST_DR (stmtinfo_a) && DR_GROUP_FIRST_DR (stmtinfo_b))
    {
      DR_GROUP_FIRST_DR (stmtinfo_a) = DR_GROUP_FIRST_DR (stmtinfo_b);
      /* Insert DRA into the chain of DRB.  */
      vect_insert_into_interleaving_chain (dra, drb);
      return;
    }

  /* 3. DRA is a part of a chain and DRB is not.  */  
  if (DR_GROUP_FIRST_DR (stmtinfo_a) && !DR_GROUP_FIRST_DR (stmtinfo_b))
    {
      tree old_first_stmt = DR_GROUP_FIRST_DR (stmtinfo_a);
      tree init_old = DR_INIT (STMT_VINFO_DATA_REF (vinfo_for_stmt (
							      old_first_stmt)));
      tree tmp;

      if (tree_int_cst_compare (init_old, DR_INIT (drb)) > 0)
	{
	  /* DRB's init is smaller than the init of the stmt previously marked 
	     as the first stmt of the interleaving chain of DRA. Therefore, we 
	     update FIRST_STMT and put DRB in the head of the list.  */
	  DR_GROUP_FIRST_DR (stmtinfo_b) = DR_STMT (drb);
	  DR_GROUP_NEXT_DR (stmtinfo_b) = old_first_stmt;
		
	  /* Update all the stmts in the list to point to the new FIRST_STMT.  */
	  tmp = old_first_stmt;
	  while (tmp)
	    {
	      DR_GROUP_FIRST_DR (vinfo_for_stmt (tmp)) = DR_STMT (drb);
	      tmp = DR_GROUP_NEXT_DR (vinfo_for_stmt (tmp));
	    }
	}
      else
	{
	  /* Insert DRB in the list of DRA.  */
	  vect_insert_into_interleaving_chain (drb, dra);
	  DR_GROUP_FIRST_DR (stmtinfo_b) = DR_GROUP_FIRST_DR (stmtinfo_a);	      
	}
      return;
    }
  
  /* 4. both DRA and DRB are in some interleaving chains.  */
  first_a = DR_GROUP_FIRST_DR (stmtinfo_a);
  first_b = DR_GROUP_FIRST_DR (stmtinfo_b);
  if (first_a == first_b)
    return;
  init_dra_chain = DR_INIT (STMT_VINFO_DATA_REF (vinfo_for_stmt (first_a)));
  init_drb_chain = DR_INIT (STMT_VINFO_DATA_REF (vinfo_for_stmt (first_b)));

  if (tree_int_cst_compare (init_dra_chain, init_drb_chain) > 0)
    {
      /* Insert the nodes of DRA chain into the DRB chain.  
	 After inserting a node, continue from this node of the DRB chain (don't
         start from the beginning.  */
      node = DR_GROUP_FIRST_DR (stmtinfo_a);
      prev = DR_GROUP_FIRST_DR (stmtinfo_b);      
      first_stmt = first_b;
    }
  else
    {
      /* Insert the nodes of DRB chain into the DRA chain.  
	 After inserting a node, continue from this node of the DRA chain (don't
         start from the beginning.  */
      node = DR_GROUP_FIRST_DR (stmtinfo_b);
      prev = DR_GROUP_FIRST_DR (stmtinfo_a);      
      first_stmt = first_a;
    }
  
  while (node)
    {
      node_init = DR_INIT (STMT_VINFO_DATA_REF (vinfo_for_stmt (node)));
      next = DR_GROUP_NEXT_DR (vinfo_for_stmt (prev));		  
      while (next)
	{	  
	  next_init = DR_INIT (STMT_VINFO_DATA_REF (vinfo_for_stmt (next)));
	  if (tree_int_cst_compare (next_init, node_init) > 0)
	    {
	      /* Insert here.  */
	      DR_GROUP_NEXT_DR (vinfo_for_stmt (prev)) = node;
	      DR_GROUP_NEXT_DR (vinfo_for_stmt (node)) = next;
	      prev = node;
	      break;
	    }
	  prev = next;
	  next = DR_GROUP_NEXT_DR (vinfo_for_stmt (prev));
	}
      if (!next)
	{
	  /* We got to the end of the list. Insert here.  */
	  DR_GROUP_NEXT_DR (vinfo_for_stmt (prev)) = node;
	  DR_GROUP_NEXT_DR (vinfo_for_stmt (node)) = NULL_TREE;
	  prev = node;
	}			
      DR_GROUP_FIRST_DR (vinfo_for_stmt (node)) = first_stmt;
      node = DR_GROUP_NEXT_DR (vinfo_for_stmt (node));	       
    }
}


/* Function vect_equal_offsets.

   Check if OFFSET1 and OFFSET2 are identical expressions.  */

static bool
vect_equal_offsets (tree offset1, tree offset2)
{
  bool res0, res1;

  STRIP_NOPS (offset1);
  STRIP_NOPS (offset2);

  if (offset1 == offset2)
    return true;

  if (TREE_CODE (offset1) != TREE_CODE (offset2)
      || !BINARY_CLASS_P (offset1)
      || !BINARY_CLASS_P (offset2))    
    return false;
  
  res0 = vect_equal_offsets (TREE_OPERAND (offset1, 0), 
			     TREE_OPERAND (offset2, 0));
  res1 = vect_equal_offsets (TREE_OPERAND (offset1, 1), 
			     TREE_OPERAND (offset2, 1));

  return (res0 && res1);
}


/* Function vect_check_interleaving.

   Check if DRA and DRB are a part of interleaving. In case they are, insert
   DRA and DRB in an interleaving chain.  */

static void
vect_check_interleaving (struct data_reference *dra,
			 struct data_reference *drb)
{
  HOST_WIDE_INT type_size_a, type_size_b, diff_mod_size, step, init_a, init_b;

  /* Check that the data-refs have same first location (except init) and they
     are both either store or load (not load and store).  */
  if ((DR_BASE_ADDRESS (dra) != DR_BASE_ADDRESS (drb)
       && (TREE_CODE (DR_BASE_ADDRESS (dra)) != ADDR_EXPR 
	   || TREE_CODE (DR_BASE_ADDRESS (drb)) != ADDR_EXPR
	   || TREE_OPERAND (DR_BASE_ADDRESS (dra), 0) 
	   != TREE_OPERAND (DR_BASE_ADDRESS (drb),0)))
      || !vect_equal_offsets (DR_OFFSET (dra), DR_OFFSET (drb))
      || !tree_int_cst_compare (DR_INIT (dra), DR_INIT (drb)) 
      || DR_IS_READ (dra) != DR_IS_READ (drb))
    return;

  /* Check:
     1. data-refs are of the same type
     2. their steps are equal
     3. the step is greater than the difference between data-refs' inits  */
  type_size_a = TREE_INT_CST_LOW (TYPE_SIZE_UNIT (TREE_TYPE (DR_REF (dra))));
  type_size_b = TREE_INT_CST_LOW (TYPE_SIZE_UNIT (TREE_TYPE (DR_REF (drb))));

  if (type_size_a != type_size_b
      || tree_int_cst_compare (DR_STEP (dra), DR_STEP (drb)))
    return;

  init_a = TREE_INT_CST_LOW (DR_INIT (dra));
  init_b = TREE_INT_CST_LOW (DR_INIT (drb));
  step = TREE_INT_CST_LOW (DR_STEP (dra));

  if (init_a > init_b)
    {
      /* If init_a == init_b + the size of the type * k, we have an interleaving, 
	 and DRB is accessed before DRA.  */
      diff_mod_size = (init_a - init_b) % type_size_a;

      if ((init_a - init_b) > step)
         return; 

      if (diff_mod_size == 0)
	{
	  vect_update_interleaving_chain (drb, dra);	  
	  if (vect_print_dump_info (REPORT_DR_DETAILS))
	    {
	      fprintf (vect_dump, "Detected interleaving ");
	      print_generic_expr (vect_dump, DR_REF (dra), TDF_SLIM);
	      fprintf (vect_dump, " and ");
	      print_generic_expr (vect_dump, DR_REF (drb), TDF_SLIM);
	    }
	  return;
	} 
    }
  else 
    {
      /* If init_b == init_a + the size of the type * k, we have an 
	 interleaving, and DRA is accessed before DRB.  */
      diff_mod_size = (init_b - init_a) % type_size_a;

      if ((init_b - init_a) > step)
         return;

      if (diff_mod_size == 0)
	{
	  vect_update_interleaving_chain (dra, drb);	  
	  if (vect_print_dump_info (REPORT_DR_DETAILS))
	    {
	      fprintf (vect_dump, "Detected interleaving ");
	      print_generic_expr (vect_dump, DR_REF (dra), TDF_SLIM);
	      fprintf (vect_dump, " and ");
	      print_generic_expr (vect_dump, DR_REF (drb), TDF_SLIM);
	    }
	  return;
	} 
    }
}


/* Function vect_analyze_data_ref_dependence.

   Return TRUE if there (might) exist a dependence between a memory-reference
   DRA and a memory-reference DRB.  */
      
static bool
vect_analyze_data_ref_dependence (struct data_dependence_relation *ddr,
                                  loop_vec_info loop_vinfo)
{
  unsigned int i;
  struct loop *loop = LOOP_VINFO_LOOP (loop_vinfo);
  int vectorization_factor = LOOP_VINFO_VECT_FACTOR (loop_vinfo);
  struct data_reference *dra = DDR_A (ddr);
  struct data_reference *drb = DDR_B (ddr);
  stmt_vec_info stmtinfo_a = vinfo_for_stmt (DR_STMT (dra)); 
  stmt_vec_info stmtinfo_b = vinfo_for_stmt (DR_STMT (drb));
  int dra_size = GET_MODE_SIZE (TYPE_MODE (TREE_TYPE (DR_REF (dra))));
  int drb_size = GET_MODE_SIZE (TYPE_MODE (TREE_TYPE (DR_REF (drb))));
  lambda_vector dist_v;
  unsigned int loop_depth;
         
  if (DDR_ARE_DEPENDENT (ddr) == chrec_known)
    {
      /* Independent data accesses.  */
      vect_check_interleaving (dra, drb);
      return false;
    }

  if ((DR_IS_READ (dra) && DR_IS_READ (drb)) || dra == drb)
    return false;
  
  if (DDR_ARE_DEPENDENT (ddr) == chrec_dont_know)
    {
      if (vect_print_dump_info (REPORT_UNVECTORIZED_LOOPS))
        {
          fprintf (vect_dump,
                   "not vectorized: can't determine dependence between ");
          print_generic_expr (vect_dump, DR_REF (dra), TDF_SLIM);
          fprintf (vect_dump, " and ");
          print_generic_expr (vect_dump, DR_REF (drb), TDF_SLIM);
        }
      return true;
    }

  if (DDR_NUM_DIST_VECTS (ddr) == 0)
    {
      if (vect_print_dump_info (REPORT_UNVECTORIZED_LOOPS))
        {
          fprintf (vect_dump, "not vectorized: bad dist vector for ");
          print_generic_expr (vect_dump, DR_REF (dra), TDF_SLIM);
          fprintf (vect_dump, " and ");
          print_generic_expr (vect_dump, DR_REF (drb), TDF_SLIM);
        }
      return true;
    }    

  loop_depth = index_in_loop_nest (loop->num, DDR_LOOP_NEST (ddr));
  for (i = 0; VEC_iterate (lambda_vector, DDR_DIST_VECTS (ddr), i, dist_v); i++)
    {
      int dist = dist_v[loop_depth];

      if (vect_print_dump_info (REPORT_DR_DETAILS))
	fprintf (vect_dump, "dependence distance  = %d.", dist);

      /* Same loop iteration.  */
      if (dist % vectorization_factor == 0 && dra_size == drb_size)
	{
	  /* Two references with distance zero have the same alignment.  */
	  VEC_safe_push (dr_p, heap, STMT_VINFO_SAME_ALIGN_REFS (stmtinfo_a), drb);
	  VEC_safe_push (dr_p, heap, STMT_VINFO_SAME_ALIGN_REFS (stmtinfo_b), dra);
	  if (vect_print_dump_info (REPORT_ALIGNMENT))
	    fprintf (vect_dump, "accesses have the same alignment.");
	  if (vect_print_dump_info (REPORT_DR_DETAILS))
	    {
	      fprintf (vect_dump, "dependence distance modulo vf == 0 between ");
	      print_generic_expr (vect_dump, DR_REF (dra), TDF_SLIM);
	      fprintf (vect_dump, " and ");
	      print_generic_expr (vect_dump, DR_REF (drb), TDF_SLIM);
	    }

          /* For interleaving, mark that there is a read-write dependency if
             necessary. We check before that one of the data-refs is store.  */ 
          if (DR_IS_READ (dra))
            DR_GROUP_READ_WRITE_DEPENDENCE (stmtinfo_a) = true;
	  else
            {
              if (DR_IS_READ (drb))
                DR_GROUP_READ_WRITE_DEPENDENCE (stmtinfo_b) = true;
	    }
	  
          continue;
	}

      if (abs (dist) >= vectorization_factor)
	{
	  /* Dependence distance does not create dependence, as far as vectorization
	     is concerned, in this case.  */
	  if (vect_print_dump_info (REPORT_DR_DETAILS))
	    fprintf (vect_dump, "dependence distance >= VF.");
	  continue;
	}

      if (vect_print_dump_info (REPORT_UNVECTORIZED_LOOPS))
	{
	  fprintf (vect_dump,
		   "not vectorized: possible dependence between data-refs ");
	  print_generic_expr (vect_dump, DR_REF (dra), TDF_SLIM);
	  fprintf (vect_dump, " and ");
	  print_generic_expr (vect_dump, DR_REF (drb), TDF_SLIM);
	}

      return true;
    }

  return false;
}


/* Function vect_analyze_data_ref_dependences.
          
   Examine all the data references in the loop, and make sure there do not
   exist any data dependences between them.  */
         
static bool
vect_analyze_data_ref_dependences (loop_vec_info loop_vinfo)
{
  unsigned int i;
  VEC (ddr_p, heap) *ddrs = LOOP_VINFO_DDRS (loop_vinfo);
  struct data_dependence_relation *ddr;

  if (vect_print_dump_info (REPORT_DETAILS)) 
    fprintf (vect_dump, "=== vect_analyze_dependences ===");
     
  for (i = 0; VEC_iterate (ddr_p, ddrs, i, ddr); i++)
    if (vect_analyze_data_ref_dependence (ddr, loop_vinfo))
      return false;

  return true;
}


/* Function vect_compute_data_ref_alignment

   Compute the misalignment of the data reference DR.

   Output:
   1. If during the misalignment computation it is found that the data reference
      cannot be vectorized then false is returned.
   2. DR_MISALIGNMENT (DR) is defined.

   FOR NOW: No analysis is actually performed. Misalignment is calculated
   only for trivial cases. TODO.  */

static bool
vect_compute_data_ref_alignment (struct data_reference *dr)
{
  tree stmt = DR_STMT (dr);
  stmt_vec_info stmt_info = vinfo_for_stmt (stmt);  
  tree ref = DR_REF (dr);
  tree vectype;
  tree base, base_addr;
  bool base_aligned;
  tree misalign;
  tree aligned_to, alignment;
   
  if (vect_print_dump_info (REPORT_DETAILS))
    fprintf (vect_dump, "vect_compute_data_ref_alignment:");

  /* Initialize misalignment to unknown.  */
  SET_DR_MISALIGNMENT (dr, -1);

  misalign = DR_INIT (dr);
  aligned_to = DR_ALIGNED_TO (dr);
  base_addr = DR_BASE_ADDRESS (dr);
  base = build_fold_indirect_ref (base_addr);
  vectype = STMT_VINFO_VECTYPE (stmt_info);
  alignment = ssize_int (TYPE_ALIGN (vectype)/BITS_PER_UNIT);

  if (tree_int_cst_compare (aligned_to, alignment) < 0)
    {
      if (vect_print_dump_info (REPORT_DETAILS))
	{
	  fprintf (vect_dump, "Unknown alignment for access: ");
	  print_generic_expr (vect_dump, base, TDF_SLIM);
	}
      return true;
    }

  if ((DECL_P (base) 
       && tree_int_cst_compare (ssize_int (DECL_ALIGN_UNIT (base)),
				alignment) >= 0)
      || (TREE_CODE (base_addr) == SSA_NAME
	  && tree_int_cst_compare (ssize_int (TYPE_ALIGN_UNIT (TREE_TYPE (
						      TREE_TYPE (base_addr)))),
				   alignment) >= 0))
    base_aligned = true;
  else
    base_aligned = false;   

  if (!base_aligned) 
    {
      /* Do not change the alignment of global variables if 
	 flag_section_anchors is enabled.  */
      if (!vect_can_force_dr_alignment_p (base, TYPE_ALIGN (vectype))
	  || (TREE_STATIC (base) && flag_section_anchors))
	{
	  if (vect_print_dump_info (REPORT_DETAILS))
	    {
	      fprintf (vect_dump, "can't force alignment of ref: ");
	      print_generic_expr (vect_dump, ref, TDF_SLIM);
	    }
	  return true;
	}
      
      /* Force the alignment of the decl.
	 NOTE: This is the only change to the code we make during
	 the analysis phase, before deciding to vectorize the loop.  */
      if (vect_print_dump_info (REPORT_DETAILS))
	fprintf (vect_dump, "force alignment");
      DECL_ALIGN (base) = TYPE_ALIGN (vectype);
      DECL_USER_ALIGN (base) = 1;
    }

  /* At this point we assume that the base is aligned.  */
  gcc_assert (base_aligned
	      || (TREE_CODE (base) == VAR_DECL 
		  && DECL_ALIGN (base) >= TYPE_ALIGN (vectype)));

  /* Modulo alignment.  */
  misalign = size_binop (TRUNC_MOD_EXPR, misalign, alignment);

  if (!host_integerp (misalign, 1))
    {
      /* Negative or overflowed misalignment value.  */
      if (vect_print_dump_info (REPORT_DETAILS))
	fprintf (vect_dump, "unexpected misalign value");
      return false;
    }

  SET_DR_MISALIGNMENT (dr, TREE_INT_CST_LOW (misalign));

  if (vect_print_dump_info (REPORT_DETAILS))
    {
      fprintf (vect_dump, "misalign = %d bytes of ref ", DR_MISALIGNMENT (dr));
      print_generic_expr (vect_dump, ref, TDF_SLIM);
    }

  return true;
}


/* Function vect_compute_data_refs_alignment

   Compute the misalignment of data references in the loop.
   Return FALSE if a data reference is found that cannot be vectorized.  */

static bool
vect_compute_data_refs_alignment (loop_vec_info loop_vinfo)
{
  VEC (data_reference_p, heap) *datarefs = LOOP_VINFO_DATAREFS (loop_vinfo);
  struct data_reference *dr;
  unsigned int i;

  for (i = 0; VEC_iterate (data_reference_p, datarefs, i, dr); i++)
    if (!vect_compute_data_ref_alignment (dr))
      return false;

  return true;
}


/* Function vect_update_misalignment_for_peel

   DR - the data reference whose misalignment is to be adjusted.
   DR_PEEL - the data reference whose misalignment is being made
             zero in the vector loop by the peel.
   NPEEL - the number of iterations in the peel loop if the misalignment
           of DR_PEEL is known at compile time.  */

static void
vect_update_misalignment_for_peel (struct data_reference *dr,
                                   struct data_reference *dr_peel, int npeel)
{
  unsigned int i;
  VEC(dr_p,heap) *same_align_drs;
  struct data_reference *current_dr;
  int dr_size = GET_MODE_SIZE (TYPE_MODE (TREE_TYPE (DR_REF (dr))));
  int dr_peel_size = GET_MODE_SIZE (TYPE_MODE (TREE_TYPE (DR_REF (dr_peel))));
  stmt_vec_info stmt_info = vinfo_for_stmt (DR_STMT (dr));
  stmt_vec_info peel_stmt_info = vinfo_for_stmt (DR_STMT (dr_peel));

 /* For interleaved data accesses the step in the loop must be multiplied by
     the size of the interleaving group.  */
  if (DR_GROUP_FIRST_DR (stmt_info))
    dr_size *= DR_GROUP_SIZE (vinfo_for_stmt (DR_GROUP_FIRST_DR (stmt_info)));
  if (DR_GROUP_FIRST_DR (peel_stmt_info))
    dr_peel_size *= DR_GROUP_SIZE (peel_stmt_info);

  /* It can be assumed that the data refs with the same alignment as dr_peel
     are aligned in the vector loop.  */
  same_align_drs
    = STMT_VINFO_SAME_ALIGN_REFS (vinfo_for_stmt (DR_STMT (dr_peel)));
  for (i = 0; VEC_iterate (dr_p, same_align_drs, i, current_dr); i++)
    {
      if (current_dr != dr)
        continue;
      gcc_assert (DR_MISALIGNMENT (dr) / dr_size ==
                  DR_MISALIGNMENT (dr_peel) / dr_peel_size);
      SET_DR_MISALIGNMENT (dr, 0);
      return;
    }

  if (known_alignment_for_access_p (dr)
      && known_alignment_for_access_p (dr_peel))
    {
      int misal = DR_MISALIGNMENT (dr);
      misal += npeel * dr_size;
      misal %= UNITS_PER_SIMD_WORD;
      SET_DR_MISALIGNMENT (dr, misal);
      return;
    }

  if (vect_print_dump_info (REPORT_DETAILS))
    fprintf (vect_dump, "Setting misalignment to -1.");
  SET_DR_MISALIGNMENT (dr, -1);
}


/* Function vect_verify_datarefs_alignment

   Return TRUE if all data references in the loop can be
   handled with respect to alignment.  */

static bool
vect_verify_datarefs_alignment (loop_vec_info loop_vinfo)
{
  VEC (data_reference_p, heap) *datarefs = LOOP_VINFO_DATAREFS (loop_vinfo);
  struct data_reference *dr;
  enum dr_alignment_support supportable_dr_alignment;
  unsigned int i;

  for (i = 0; VEC_iterate (data_reference_p, datarefs, i, dr); i++)
    {
      tree stmt = DR_STMT (dr);
      stmt_vec_info stmt_info = vinfo_for_stmt (stmt);

      /* For interleaving, only the alignment of the first access matters.  */
      if (DR_GROUP_FIRST_DR (stmt_info)
          && DR_GROUP_FIRST_DR (stmt_info) != stmt)
        continue;

      supportable_dr_alignment = vect_supportable_dr_alignment (dr);
      if (!supportable_dr_alignment)
        {
          if (vect_print_dump_info (REPORT_UNVECTORIZED_LOOPS))
            {
              if (DR_IS_READ (dr))
                fprintf (vect_dump, 
                         "not vectorized: unsupported unaligned load.");
              else
                fprintf (vect_dump, 
                         "not vectorized: unsupported unaligned store.");
            }
          return false;
        }
      if (supportable_dr_alignment != dr_aligned
          && vect_print_dump_info (REPORT_ALIGNMENT))
        fprintf (vect_dump, "Vectorizing an unaligned access.");
    }
  return true;
}


/* Function vect_enhance_data_refs_alignment

   This pass will use loop versioning and loop peeling in order to enhance
   the alignment of data references in the loop.

   FOR NOW: we assume that whatever versioning/peeling takes place, only the
   original loop is to be vectorized; Any other loops that are created by
   the transformations performed in this pass - are not supposed to be
   vectorized. This restriction will be relaxed.

   This pass will require a cost model to guide it whether to apply peeling
   or versioning or a combination of the two. For example, the scheme that
   intel uses when given a loop with several memory accesses, is as follows:
   choose one memory access ('p') which alignment you want to force by doing
   peeling. Then, either (1) generate a loop in which 'p' is aligned and all
   other accesses are not necessarily aligned, or (2) use loop versioning to
   generate one loop in which all accesses are aligned, and another loop in
   which only 'p' is necessarily aligned.

   ("Automatic Intra-Register Vectorization for the Intel Architecture",
   Aart J.C. Bik, Milind Girkar, Paul M. Grey and Ximmin Tian, International
   Journal of Parallel Programming, Vol. 30, No. 2, April 2002.)

   Devising a cost model is the most critical aspect of this work. It will
   guide us on which access to peel for, whether to use loop versioning, how
   many versions to create, etc. The cost model will probably consist of
   generic considerations as well as target specific considerations (on
   powerpc for example, misaligned stores are more painful than misaligned
   loads).

   Here are the general steps involved in alignment enhancements:

     -- original loop, before alignment analysis:
	for (i=0; i<N; i++){
	  x = q[i];			# DR_MISALIGNMENT(q) = unknown
	  p[i] = y;			# DR_MISALIGNMENT(p) = unknown
	}

     -- After vect_compute_data_refs_alignment:
	for (i=0; i<N; i++){
	  x = q[i];			# DR_MISALIGNMENT(q) = 3
	  p[i] = y;			# DR_MISALIGNMENT(p) = unknown
	}

     -- Possibility 1: we do loop versioning:
     if (p is aligned) {
	for (i=0; i<N; i++){	# loop 1A
	  x = q[i];			# DR_MISALIGNMENT(q) = 3
	  p[i] = y;			# DR_MISALIGNMENT(p) = 0
	}
     }
     else {
	for (i=0; i<N; i++){	# loop 1B
	  x = q[i];			# DR_MISALIGNMENT(q) = 3
	  p[i] = y;			# DR_MISALIGNMENT(p) = unaligned
	}
     }

     -- Possibility 2: we do loop peeling:
     for (i = 0; i < 3; i++){	# (scalar loop, not to be vectorized).
	x = q[i];
	p[i] = y;
     }
     for (i = 3; i < N; i++){	# loop 2A
	x = q[i];			# DR_MISALIGNMENT(q) = 0
	p[i] = y;			# DR_MISALIGNMENT(p) = unknown
     }

     -- Possibility 3: combination of loop peeling and versioning:
     for (i = 0; i < 3; i++){	# (scalar loop, not to be vectorized).
	x = q[i];
	p[i] = y;
     }
     if (p is aligned) {
	for (i = 3; i<N; i++){	# loop 3A
	  x = q[i];			# DR_MISALIGNMENT(q) = 0
	  p[i] = y;			# DR_MISALIGNMENT(p) = 0
	}
     }
     else {
	for (i = 3; i<N; i++){	# loop 3B
	  x = q[i];			# DR_MISALIGNMENT(q) = 0
	  p[i] = y;			# DR_MISALIGNMENT(p) = unaligned
	}
     }

     These loops are later passed to loop_transform to be vectorized. The
     vectorizer will use the alignment information to guide the transformation
     (whether to generate regular loads/stores, or with special handling for
     misalignment).  */

static bool
vect_enhance_data_refs_alignment (loop_vec_info loop_vinfo)
{
  VEC (data_reference_p, heap) *datarefs = LOOP_VINFO_DATAREFS (loop_vinfo);
  struct loop *loop = LOOP_VINFO_LOOP (loop_vinfo);
  enum dr_alignment_support supportable_dr_alignment;
  struct data_reference *dr0 = NULL;
  struct data_reference *dr;
  unsigned int i;
  bool do_peeling = false;
  bool do_versioning = false;
  bool stat;
  tree stmt;
  stmt_vec_info stmt_info;

  if (vect_print_dump_info (REPORT_DETAILS))
    fprintf (vect_dump, "=== vect_enhance_data_refs_alignment ===");

  /* While cost model enhancements are expected in the future, the high level
     view of the code at this time is as follows:

     A) If there is a misaligned write then see if peeling to align this write
        can make all data references satisfy vect_supportable_dr_alignment.
        If so, update data structures as needed and return true.  Note that
        at this time vect_supportable_dr_alignment is known to return false
        for a misaligned write.

     B) If peeling wasn't possible and there is a data reference with an
        unknown misalignment that does not satisfy vect_supportable_dr_alignment
        then see if loop versioning checks can be used to make all data
        references satisfy vect_supportable_dr_alignment.  If so, update
        data structures as needed and return true.

     C) If neither peeling nor versioning were successful then return false if
        any data reference does not satisfy vect_supportable_dr_alignment.

     D) Return true (all data references satisfy vect_supportable_dr_alignment).

     Note, Possibility 3 above (which is peeling and versioning together) is not
     being done at this time.  */

  /* (1) Peeling to force alignment.  */

  /* (1.1) Decide whether to perform peeling, and how many iterations to peel:
     Considerations:
     + How many accesses will become aligned due to the peeling
     - How many accesses will become unaligned due to the peeling,
       and the cost of misaligned accesses.
     - The cost of peeling (the extra runtime checks, the increase 
       in code size).

     The scheme we use FORNOW: peel to force the alignment of the first
     misaligned store in the loop.
     Rationale: misaligned stores are not yet supported.

     TODO: Use a cost model.  */

  for (i = 0; VEC_iterate (data_reference_p, datarefs, i, dr); i++)
    {
      stmt = DR_STMT (dr);
      stmt_info = vinfo_for_stmt (stmt);

      /* For interleaving, only the alignment of the first access
         matters.  */
      if (DR_GROUP_FIRST_DR (stmt_info)
          && DR_GROUP_FIRST_DR (stmt_info) != stmt)
        continue;

      if (!DR_IS_READ (dr) && !aligned_access_p (dr))
        {
	  if (DR_GROUP_FIRST_DR (stmt_info))
	    {
	      /* For interleaved access we peel only if number of iterations in
		 the prolog loop ({VF - misalignment}), is a multiple of the
		 number of the interleaved accesses.  */
	      int elem_size, mis_in_elements;
	      tree vectype = STMT_VINFO_VECTYPE (stmt_info);
	      int nelements = TYPE_VECTOR_SUBPARTS (vectype);

	      /* FORNOW: handle only known alignment.  */
	      if (!known_alignment_for_access_p (dr))
		{
		  do_peeling = false;
		  break;
		}

	      elem_size = UNITS_PER_SIMD_WORD / nelements;
	      mis_in_elements = DR_MISALIGNMENT (dr) / elem_size;

	      if ((nelements - mis_in_elements) % DR_GROUP_SIZE (stmt_info))
		{
		  do_peeling = false;
		  break;
		}
	    }
	  dr0 = dr;
	  do_peeling = true;
	  break;
	}
    }

  /* Often peeling for alignment will require peeling for loop-bound, which in 
     turn requires that we know how to adjust the loop ivs after the loop.  */
  if (!vect_can_advance_ivs_p (loop_vinfo)
      || !slpeel_can_duplicate_loop_p (loop, single_exit (loop)))
    do_peeling = false;

  if (do_peeling)
    {
      int mis;
      int npeel = 0;
      tree stmt = DR_STMT (dr0);
      stmt_vec_info stmt_info = vinfo_for_stmt (stmt);
      tree vectype = STMT_VINFO_VECTYPE (stmt_info);
      int nelements = TYPE_VECTOR_SUBPARTS (vectype);

      if (known_alignment_for_access_p (dr0))
        {
          /* Since it's known at compile time, compute the number of iterations
             in the peeled loop (the peeling factor) for use in updating
             DR_MISALIGNMENT values.  The peeling factor is the vectorization
             factor minus the misalignment as an element count.  */
          mis = DR_MISALIGNMENT (dr0);
          mis /= GET_MODE_SIZE (TYPE_MODE (TREE_TYPE (DR_REF (dr0))));
          npeel = nelements - mis;

	  /* For interleaved data access every iteration accesses all the 
	     members of the group, therefore we divide the number of iterations
	     by the group size.  */
	  stmt_info = vinfo_for_stmt (DR_STMT (dr0));	  
	  if (DR_GROUP_FIRST_DR (stmt_info))
	    npeel /= DR_GROUP_SIZE (stmt_info);

          if (vect_print_dump_info (REPORT_DETAILS))
            fprintf (vect_dump, "Try peeling by %d", npeel);
        }

      /* Ensure that all data refs can be vectorized after the peel.  */
      for (i = 0; VEC_iterate (data_reference_p, datarefs, i, dr); i++)
        {
          int save_misalignment;

	  if (dr == dr0)
	    continue;

	  stmt = DR_STMT (dr);
	  stmt_info = vinfo_for_stmt (stmt);
	  /* For interleaving, only the alignment of the first access
            matters.  */
	  if (DR_GROUP_FIRST_DR (stmt_info)
	      && DR_GROUP_FIRST_DR (stmt_info) != stmt)
	    continue;

	  save_misalignment = DR_MISALIGNMENT (dr);
	  vect_update_misalignment_for_peel (dr, dr0, npeel);
	  supportable_dr_alignment = vect_supportable_dr_alignment (dr);
	  SET_DR_MISALIGNMENT (dr, save_misalignment);
	  
	  if (!supportable_dr_alignment)
	    {
	      do_peeling = false;
	      break;
	    }
	}

      if (do_peeling)
        {
          /* (1.2) Update the DR_MISALIGNMENT of each data reference DR_i.
             If the misalignment of DR_i is identical to that of dr0 then set
             DR_MISALIGNMENT (DR_i) to zero.  If the misalignment of DR_i and
             dr0 are known at compile time then increment DR_MISALIGNMENT (DR_i)
             by the peeling factor times the element size of DR_i (MOD the
             vectorization factor times the size).  Otherwise, the
             misalignment of DR_i must be set to unknown.  */
	  for (i = 0; VEC_iterate (data_reference_p, datarefs, i, dr); i++)
	    if (dr != dr0)
	      vect_update_misalignment_for_peel (dr, dr0, npeel);

          LOOP_VINFO_UNALIGNED_DR (loop_vinfo) = dr0;
          LOOP_PEELING_FOR_ALIGNMENT (loop_vinfo) = DR_MISALIGNMENT (dr0);
	  SET_DR_MISALIGNMENT (dr0, 0);
	  if (vect_print_dump_info (REPORT_ALIGNMENT))
            fprintf (vect_dump, "Alignment of access forced using peeling.");

          if (vect_print_dump_info (REPORT_DETAILS))
            fprintf (vect_dump, "Peeling for alignment will be applied.");

	  stat = vect_verify_datarefs_alignment (loop_vinfo);
	  gcc_assert (stat);
          return stat;
        }
    }


  /* (2) Versioning to force alignment.  */

  /* Try versioning if:
     1) flag_tree_vect_loop_version is TRUE
     2) optimize_size is FALSE
     3) there is at least one unsupported misaligned data ref with an unknown
        misalignment, and
     4) all misaligned data refs with a known misalignment are supported, and
     5) the number of runtime alignment checks is within reason.  */

  do_versioning = flag_tree_vect_loop_version && (!optimize_size);

  if (do_versioning)
    {
      for (i = 0; VEC_iterate (data_reference_p, datarefs, i, dr); i++)
        {
	  stmt = DR_STMT (dr);
	  stmt_info = vinfo_for_stmt (stmt);

	  /* For interleaving, only the alignment of the first access
	     matters.  */
	  if (aligned_access_p (dr)
	      || (DR_GROUP_FIRST_DR (stmt_info)
		  && DR_GROUP_FIRST_DR (stmt_info) != stmt))
	    continue;

	  supportable_dr_alignment = vect_supportable_dr_alignment (dr);

          if (!supportable_dr_alignment)
            {
              tree stmt;
              int mask;
              tree vectype;

              if (known_alignment_for_access_p (dr)
                  || VEC_length (tree,
                                 LOOP_VINFO_MAY_MISALIGN_STMTS (loop_vinfo))
                     >= (unsigned) PARAM_VALUE (PARAM_VECT_MAX_VERSION_CHECKS))
                {
                  do_versioning = false;
                  break;
                }

              stmt = DR_STMT (dr);
              vectype = STMT_VINFO_VECTYPE (vinfo_for_stmt (stmt));
              gcc_assert (vectype);
  
              /* The rightmost bits of an aligned address must be zeros.
                 Construct the mask needed for this test.  For example,
                 GET_MODE_SIZE for the vector mode V4SI is 16 bytes so the
                 mask must be 15 = 0xf. */
              mask = GET_MODE_SIZE (TYPE_MODE (vectype)) - 1;

              /* FORNOW: use the same mask to test all potentially unaligned
                 references in the loop.  The vectorizer currently supports
                 a single vector size, see the reference to
                 GET_MODE_NUNITS (TYPE_MODE (vectype)) where the
                 vectorization factor is computed.  */
              gcc_assert (!LOOP_VINFO_PTR_MASK (loop_vinfo)
                          || LOOP_VINFO_PTR_MASK (loop_vinfo) == mask);
              LOOP_VINFO_PTR_MASK (loop_vinfo) = mask;
              VEC_safe_push (tree, heap,
                             LOOP_VINFO_MAY_MISALIGN_STMTS (loop_vinfo),
                             DR_STMT (dr));
            }
        }
      
      /* Versioning requires at least one misaligned data reference.  */
      if (VEC_length (tree, LOOP_VINFO_MAY_MISALIGN_STMTS (loop_vinfo)) == 0)
        do_versioning = false;
      else if (!do_versioning)
        VEC_truncate (tree, LOOP_VINFO_MAY_MISALIGN_STMTS (loop_vinfo), 0);
    }

  if (do_versioning)
    {
      VEC(tree,heap) *may_misalign_stmts
        = LOOP_VINFO_MAY_MISALIGN_STMTS (loop_vinfo);
      tree stmt;

      /* It can now be assumed that the data references in the statements
         in LOOP_VINFO_MAY_MISALIGN_STMTS will be aligned in the version
         of the loop being vectorized.  */
      for (i = 0; VEC_iterate (tree, may_misalign_stmts, i, stmt); i++)
        {
          stmt_vec_info stmt_info = vinfo_for_stmt (stmt);
          dr = STMT_VINFO_DATA_REF (stmt_info);
	  SET_DR_MISALIGNMENT (dr, 0);
	  if (vect_print_dump_info (REPORT_ALIGNMENT))
            fprintf (vect_dump, "Alignment of access forced using versioning.");
        }

      if (vect_print_dump_info (REPORT_DETAILS))
        fprintf (vect_dump, "Versioning for alignment will be applied.");

      /* Peeling and versioning can't be done together at this time.  */
      gcc_assert (! (do_peeling && do_versioning));

      stat = vect_verify_datarefs_alignment (loop_vinfo);
      gcc_assert (stat);
      return stat;
    }

  /* This point is reached if neither peeling nor versioning is being done.  */
  gcc_assert (! (do_peeling || do_versioning));

  stat = vect_verify_datarefs_alignment (loop_vinfo);
  return stat;
}


/* Function vect_analyze_data_refs_alignment

   Analyze the alignment of the data-references in the loop.
   Return FALSE if a data reference is found that cannot be vectorized.  */

static bool
vect_analyze_data_refs_alignment (loop_vec_info loop_vinfo)
{
  if (vect_print_dump_info (REPORT_DETAILS))
    fprintf (vect_dump, "=== vect_analyze_data_refs_alignment ===");

  if (!vect_compute_data_refs_alignment (loop_vinfo))
    {
      if (vect_print_dump_info (REPORT_UNVECTORIZED_LOOPS))
	fprintf (vect_dump, 
		 "not vectorized: can't calculate alignment for data ref.");
      return false;
    }

  return true;
}


/* Function vect_analyze_data_ref_access.

   Analyze the access pattern of the data-reference DR. For now, a data access
   has to be consecutive to be considered vectorizable.  */

static bool
vect_analyze_data_ref_access (struct data_reference *dr)
{
  tree step = DR_STEP (dr);
  HOST_WIDE_INT dr_step = TREE_INT_CST_LOW (step);
  tree scalar_type = TREE_TYPE (DR_REF (dr));
  HOST_WIDE_INT type_size = TREE_INT_CST_LOW (TYPE_SIZE_UNIT (scalar_type));
  tree stmt = DR_STMT (dr);
  /* For interleaving, STRIDE is STEP counted in elements, i.e., the size of the 
     interleaving group (including gaps).  */
  HOST_WIDE_INT stride = dr_step / type_size;

  if (!step)
    {
      if (vect_print_dump_info (REPORT_DETAILS))
	fprintf (vect_dump, "bad data-ref access");
      return false;
    }

  /* Consecutive?  */
  if (!tree_int_cst_compare (step, TYPE_SIZE_UNIT (scalar_type)))
    {
      /* Mark that it is not interleaving.  */
      DR_GROUP_FIRST_DR (vinfo_for_stmt (stmt)) = NULL_TREE;
      return true;
    }

  /* Not consecutive access is possible only if it is a part of interleaving.  */
  if (!DR_GROUP_FIRST_DR (vinfo_for_stmt (stmt)))
    {
      /* Check if it this DR is a part of interleaving, and is a single
	 element of the group that is accessed in the loop.  */
      
      /* Gaps are supported only for loads. STEP must be a multiple of the type
	 size.  The size of the group must be a power of 2.  */
      if (DR_IS_READ (dr)
	  && (dr_step % type_size) == 0
	  && stride > 0
	  && exact_log2 (stride) != -1)
	{
	  DR_GROUP_FIRST_DR (vinfo_for_stmt (stmt)) = stmt;
	  DR_GROUP_SIZE (vinfo_for_stmt (stmt)) = stride;
	  if (vect_print_dump_info (REPORT_DR_DETAILS))
	    {
	      fprintf (vect_dump, "Detected single element interleaving %d ",
		       DR_GROUP_SIZE (vinfo_for_stmt (stmt)));
	      print_generic_expr (vect_dump, DR_REF (dr), TDF_SLIM);
	      fprintf (vect_dump, " step ");
	      print_generic_expr (vect_dump, step, TDF_SLIM);
	    }
	  return true;
	}
      if (vect_print_dump_info (REPORT_DETAILS))
	fprintf (vect_dump, "not consecutive access");
      return false;
    }

  if (DR_GROUP_FIRST_DR (vinfo_for_stmt (stmt)) == stmt)
    {
      /* First stmt in the interleaving chain. Check the chain.  */
      tree next = DR_GROUP_NEXT_DR (vinfo_for_stmt (stmt));
      struct data_reference *data_ref = dr;
      unsigned int count = 1;
      tree next_step;
      tree prev_init = DR_INIT (data_ref);
      tree prev = stmt;
      HOST_WIDE_INT diff, count_in_bytes;

      while (next)
	{
	  /* Skip same data-refs. In case that two or more stmts share data-ref
	     (supported only for loads), we vectorize only the first stmt, and
	     the rest get their vectorized loads from the first one.  */
	  if (!tree_int_cst_compare (DR_INIT (data_ref),
				     DR_INIT (STMT_VINFO_DATA_REF (
						      vinfo_for_stmt (next)))))
	    {
              if (!DR_IS_READ (data_ref))
                { 
                  if (vect_print_dump_info (REPORT_DETAILS))
                    fprintf (vect_dump, "Two store stmts share the same dr.");
                  return false; 
                }

              /* Check that there is no load-store dependencies for this loads 
                 to prevent a case of load-store-load to the same location.  */
              if (DR_GROUP_READ_WRITE_DEPENDENCE (vinfo_for_stmt (next))
                  || DR_GROUP_READ_WRITE_DEPENDENCE (vinfo_for_stmt (prev)))
                {
                  if (vect_print_dump_info (REPORT_DETAILS))
                    fprintf (vect_dump, 
                             "READ_WRITE dependence in interleaving.");
                  return false;
                }

	      /* For load use the same data-ref load.  */
	      DR_GROUP_SAME_DR_STMT (vinfo_for_stmt (next)) = prev;

	      prev = next;
	      next = DR_GROUP_NEXT_DR (vinfo_for_stmt (next));
	      continue;
	    }
	  prev = next;

	  /* Check that all the accesses have the same STEP.  */
	  next_step = DR_STEP (STMT_VINFO_DATA_REF (vinfo_for_stmt (next)));
	  if (tree_int_cst_compare (step, next_step))
	    {
	      if (vect_print_dump_info (REPORT_DETAILS))
		fprintf (vect_dump, "not consecutive access in interleaving");
	      return false;
	    }

	  data_ref = STMT_VINFO_DATA_REF (vinfo_for_stmt (next));
	  /* Check that the distance between two accesses is equal to the type
	     size. Otherwise, we have gaps.  */
	  diff = (TREE_INT_CST_LOW (DR_INIT (data_ref)) 
		  - TREE_INT_CST_LOW (prev_init)) / type_size;
	  if (!DR_IS_READ (data_ref) && diff != 1)
	    {
	      if (vect_print_dump_info (REPORT_DETAILS))
		fprintf (vect_dump, "interleaved store with gaps");
	      return false;
	    }
	  /* Store the gap from the previous member of the group. If there is no
             gap in the access, DR_GROUP_GAP is always 1.  */
	  DR_GROUP_GAP (vinfo_for_stmt (next)) = diff;

	  prev_init = DR_INIT (data_ref);
	  next = DR_GROUP_NEXT_DR (vinfo_for_stmt (next));
	  /* Count the number of data-refs in the chain.  */
	  count++;
	}

      /* COUNT is the number of accesses found, we multiply it by the size of 
	 the type to get COUNT_IN_BYTES.  */
      count_in_bytes = type_size * count;

      /* Check that the size of the interleaving is not greater than STEP.  */
      if (dr_step < count_in_bytes) 
	{
	  if (vect_print_dump_info (REPORT_DETAILS))
	    {
	      fprintf (vect_dump, "interleaving size is greater than step for ");
	      print_generic_expr (vect_dump, DR_REF (dr), TDF_SLIM); 
	    }
	  return false;
	}

      /* Check that the size of the interleaving is equal to STEP for stores, 
         i.e., that there are no gaps.  */ 
      if (!DR_IS_READ (dr) && dr_step != count_in_bytes) 
	{
	  if (vect_print_dump_info (REPORT_DETAILS))
	    fprintf (vect_dump, "interleaved store with gaps");
	  return false;
	}

      /* Check that STEP is a multiple of type size.  */
      if ((dr_step % type_size) != 0)
	{
	  if (vect_print_dump_info (REPORT_DETAILS)) 
            {
              fprintf (vect_dump, "step is not a multiple of type size: step ");
              print_generic_expr (vect_dump, step, TDF_SLIM);
              fprintf (vect_dump, " size ");
              print_generic_expr (vect_dump, TYPE_SIZE_UNIT (scalar_type),
                                  TDF_SLIM);
            }
	  return false;
	}

      /* FORNOW: we handle only interleaving that is a power of 2.  */
      if (exact_log2 (stride) == -1)
	{
	  if (vect_print_dump_info (REPORT_DETAILS))
	    fprintf (vect_dump, "interleaving is not a power of 2");
	  return false;
	}
      DR_GROUP_SIZE (vinfo_for_stmt (stmt)) = stride;
    }
  return true;
}


/* Function vect_analyze_data_ref_accesses.

   Analyze the access pattern of all the data references in the loop.

   FORNOW: the only access pattern that is considered vectorizable is a
	   simple step 1 (consecutive) access.

   FORNOW: handle only arrays and pointer accesses.  */

static bool
vect_analyze_data_ref_accesses (loop_vec_info loop_vinfo)
{
  unsigned int i;
  VEC (data_reference_p, heap) *datarefs = LOOP_VINFO_DATAREFS (loop_vinfo);
  struct data_reference *dr;

  if (vect_print_dump_info (REPORT_DETAILS))
    fprintf (vect_dump, "=== vect_analyze_data_ref_accesses ===");

  for (i = 0; VEC_iterate (data_reference_p, datarefs, i, dr); i++)
    if (!vect_analyze_data_ref_access (dr))
      {
	if (vect_print_dump_info (REPORT_UNVECTORIZED_LOOPS))
	  fprintf (vect_dump, "not vectorized: complicated access pattern.");
	return false;
      }

  return true;
}


/* Function vect_analyze_data_refs.

  Find all the data references in the loop.

   The general structure of the analysis of data refs in the vectorizer is as
   follows:
   1- vect_analyze_data_refs(loop): call compute_data_dependences_for_loop to
      find and analyze all data-refs in the loop and their dependences.
   2- vect_analyze_dependences(): apply dependence testing using ddrs.
   3- vect_analyze_drs_alignment(): check that ref_stmt.alignment is ok.
   4- vect_analyze_drs_access(): check that ref_stmt.step is ok.

*/

static bool
vect_analyze_data_refs (loop_vec_info loop_vinfo)  
{
  struct loop *loop = LOOP_VINFO_LOOP (loop_vinfo);
  unsigned int i;
  VEC (data_reference_p, heap) *datarefs;
  struct data_reference *dr;
  tree scalar_type;

  if (vect_print_dump_info (REPORT_DETAILS))
    fprintf (vect_dump, "=== vect_analyze_data_refs ===\n");

  compute_data_dependences_for_loop (loop, true,
                                     &LOOP_VINFO_DATAREFS (loop_vinfo),
                                     &LOOP_VINFO_DDRS (loop_vinfo));

  /* Go through the data-refs, check that the analysis succeeded. Update pointer
     from stmt_vec_info struct to DR and vectype.  */
  datarefs = LOOP_VINFO_DATAREFS (loop_vinfo);

  for (i = 0; VEC_iterate (data_reference_p, datarefs, i, dr); i++)
    {
      tree stmt;
      stmt_vec_info stmt_info;
   
      if (!dr || !DR_REF (dr))
        {
          if (vect_print_dump_info (REPORT_UNVECTORIZED_LOOPS))
	    fprintf (vect_dump, "not vectorized: unhandled data-ref ");
          return false;
        }
 
      /* Update DR field in stmt_vec_info struct.  */
      stmt = DR_STMT (dr);
      stmt_info = vinfo_for_stmt (stmt);

      if (STMT_VINFO_DATA_REF (stmt_info))
        {
          if (vect_print_dump_info (REPORT_UNVECTORIZED_LOOPS))
            {
              fprintf (vect_dump,
                       "not vectorized: more than one data ref in stmt: ");
              print_generic_expr (vect_dump, stmt, TDF_SLIM);
            }
          return false;
        }
      STMT_VINFO_DATA_REF (stmt_info) = dr;
     
      /* Check that analysis of the data-ref succeeded.  */
      if (!DR_BASE_ADDRESS (dr) || !DR_OFFSET (dr) || !DR_INIT (dr)
          || !DR_STEP (dr))   
        {
          if (vect_print_dump_info (REPORT_UNVECTORIZED_LOOPS))
            {
              fprintf (vect_dump, "not vectorized: data ref analysis failed ");
              print_generic_expr (vect_dump, stmt, TDF_SLIM);
            }
          return false;
        }
      if (!DR_SYMBOL_TAG (dr))
        {
          if (vect_print_dump_info (REPORT_UNVECTORIZED_LOOPS))
            {
              fprintf (vect_dump, "not vectorized: no memory tag for ");
              print_generic_expr (vect_dump, DR_REF (dr), TDF_SLIM);
            }
          return false;
        }
                       
      /* Set vectype for STMT.  */
      scalar_type = TREE_TYPE (DR_REF (dr));
      STMT_VINFO_VECTYPE (stmt_info) =
                get_vectype_for_scalar_type (scalar_type);
      if (!STMT_VINFO_VECTYPE (stmt_info)) 
        {
          if (vect_print_dump_info (REPORT_UNVECTORIZED_LOOPS))
            {
              fprintf (vect_dump,
                       "not vectorized: no vectype for stmt: ");
              print_generic_expr (vect_dump, stmt, TDF_SLIM);
              fprintf (vect_dump, " scalar_type: ");
              print_generic_expr (vect_dump, scalar_type, TDF_DETAILS);
            }
          return false;
        }
    }
      
  return true;
}


/* Utility functions used by vect_mark_stmts_to_be_vectorized.  */

/* Function vect_mark_relevant.

   Mark STMT as "relevant for vectorization" and add it to WORKLIST.  */

static void
vect_mark_relevant (VEC(tree,heap) **worklist, tree stmt,
		    enum vect_relevant relevant, bool live_p)
{
  stmt_vec_info stmt_info = vinfo_for_stmt (stmt);
  enum vect_relevant save_relevant = STMT_VINFO_RELEVANT (stmt_info);
  bool save_live_p = STMT_VINFO_LIVE_P (stmt_info);

  if (vect_print_dump_info (REPORT_DETAILS))
    fprintf (vect_dump, "mark relevant %d, live %d.", relevant, live_p);

  if (STMT_VINFO_IN_PATTERN_P (stmt_info))
    {
      tree pattern_stmt;

      /* This is the last stmt in a sequence that was detected as a 
         pattern that can potentially be vectorized.  Don't mark the stmt
         as relevant/live because it's not going to vectorized.
         Instead mark the pattern-stmt that replaces it.  */
      if (vect_print_dump_info (REPORT_DETAILS))
        fprintf (vect_dump, "last stmt in pattern. don't mark relevant/live.");
      pattern_stmt = STMT_VINFO_RELATED_STMT (stmt_info);
      stmt_info = vinfo_for_stmt (pattern_stmt);
      gcc_assert (STMT_VINFO_RELATED_STMT (stmt_info) == stmt);
      save_relevant = STMT_VINFO_RELEVANT (stmt_info);
      save_live_p = STMT_VINFO_LIVE_P (stmt_info);
      stmt = pattern_stmt;
    }

  STMT_VINFO_LIVE_P (stmt_info) |= live_p;
  if (relevant > STMT_VINFO_RELEVANT (stmt_info))
    STMT_VINFO_RELEVANT (stmt_info) = relevant;

  if (STMT_VINFO_RELEVANT (stmt_info) == save_relevant
      && STMT_VINFO_LIVE_P (stmt_info) == save_live_p)
    {
      if (vect_print_dump_info (REPORT_DETAILS))
        fprintf (vect_dump, "already marked relevant/live.");
      return;
    }

  VEC_safe_push (tree, heap, *worklist, stmt);
}


/* Function vect_stmt_relevant_p.

   Return true if STMT in loop that is represented by LOOP_VINFO is
   "relevant for vectorization".

   A stmt is considered "relevant for vectorization" if:
   - it has uses outside the loop.
   - it has vdefs (it alters memory).
   - control stmts in the loop (except for the exit condition).

   CHECKME: what other side effects would the vectorizer allow?  */

static bool
vect_stmt_relevant_p (tree stmt, loop_vec_info loop_vinfo,
		      enum vect_relevant *relevant, bool *live_p)
{
  struct loop *loop = LOOP_VINFO_LOOP (loop_vinfo);
  ssa_op_iter op_iter;
  imm_use_iterator imm_iter;
  use_operand_p use_p;
  def_operand_p def_p;

  *relevant = vect_unused_in_loop;
  *live_p = false;

  /* cond stmt other than loop exit cond.  */
  if (is_ctrl_stmt (stmt) && (stmt != LOOP_VINFO_EXIT_COND (loop_vinfo)))
    *relevant = vect_used_in_loop;

  /* changing memory.  */
  if (TREE_CODE (stmt) != PHI_NODE)
    if (!ZERO_SSA_OPERANDS (stmt, SSA_OP_VIRTUAL_DEFS))
      {
	if (vect_print_dump_info (REPORT_DETAILS))
	  fprintf (vect_dump, "vec_stmt_relevant_p: stmt has vdefs.");
	*relevant = vect_used_in_loop;
      }

  /* uses outside the loop.  */
  FOR_EACH_PHI_OR_STMT_DEF (def_p, stmt, op_iter, SSA_OP_DEF)
    {
      FOR_EACH_IMM_USE_FAST (use_p, imm_iter, DEF_FROM_PTR (def_p))
	{
	  basic_block bb = bb_for_stmt (USE_STMT (use_p));
	  if (!flow_bb_inside_loop_p (loop, bb))
	    {
	      if (vect_print_dump_info (REPORT_DETAILS))
		fprintf (vect_dump, "vec_stmt_relevant_p: used out of loop.");

	      /* We expect all such uses to be in the loop exit phis
		 (because of loop closed form)   */
	      gcc_assert (TREE_CODE (USE_STMT (use_p)) == PHI_NODE);
	      gcc_assert (bb == single_exit (loop)->dest);

              *live_p = true;
	    }
	}
    }

  return (*live_p || *relevant);
}


/* 
   Function process_use.

   Inputs:
   - a USE in STMT in a loop represented by LOOP_VINFO
   - LIVE_P, RELEVANT - enum values to be set in the STMT_VINFO of the stmt 
     that defined USE. This is dont by calling mark_relevant and passing it
     the WORKLIST (to add DEF_STMT to the WORKlist in case itis relevant). 

   Outputs:
   Generally, LIVE_P and RELEVANT are used to define the liveness and
   relevance info of the DEF_STMT of this USE:
       STMT_VINFO_LIVE_P (DEF_STMT_info) <-- live_p
       STMT_VINFO_RELEVANT (DEF_STMT_info) <-- relevant
   Exceptions:
   - case 1: If USE is used only for address computations (e.g. array indexing),
   which does not need to be directly vectorized, then the liveness/relevance 
   of the respective DEF_STMT is left unchanged.
   - case 2: If STMT is a reduction phi and DEF_STMT is a reduction stmt, we 
   skip DEF_STMT cause it had already been processed.  

   Return true if everything is as expected. Return false otherwise.  */

static bool
process_use (tree stmt, tree use, loop_vec_info loop_vinfo, bool live_p, 
	     enum vect_relevant relevant, VEC(tree,heap) **worklist)
{
  struct loop *loop = LOOP_VINFO_LOOP (loop_vinfo);
  stmt_vec_info stmt_vinfo = vinfo_for_stmt (stmt);
  stmt_vec_info dstmt_vinfo;
  basic_block def_bb;
  tree def, def_stmt;
  enum vect_def_type dt;

  /* case 1: we are only interested in uses that need to be vectorized.  Uses 
     that are used for address computation are not considered relevant.  */
  if (!exist_non_indexing_operands_for_use_p (use, stmt))
     return true;

  if (!vect_is_simple_use (use, loop_vinfo, &def_stmt, &def, &dt))
    { 
      if (vect_print_dump_info (REPORT_UNVECTORIZED_LOOPS))
        fprintf (vect_dump, "not vectorized: unsupported use in stmt.");
      return false;
    }

  if (!def_stmt || IS_EMPTY_STMT (def_stmt))
    return true;

  def_bb = bb_for_stmt (def_stmt);
  if (!flow_bb_inside_loop_p (loop, def_bb))
    return true;

  /* case 2: A reduction phi defining a reduction stmt (DEF_STMT). DEF_STMT 
     must have already been processed, so we just check that everything is as 
     expected, and we are done.  */
  dstmt_vinfo = vinfo_for_stmt (def_stmt);
  if (TREE_CODE (stmt) == PHI_NODE
      && STMT_VINFO_DEF_TYPE (stmt_vinfo) == vect_reduction_def
      && TREE_CODE (def_stmt) != PHI_NODE
      && STMT_VINFO_DEF_TYPE (dstmt_vinfo) == vect_reduction_def)
    {
      if (STMT_VINFO_IN_PATTERN_P (dstmt_vinfo))
	dstmt_vinfo = vinfo_for_stmt (STMT_VINFO_RELATED_STMT (dstmt_vinfo));
      gcc_assert (STMT_VINFO_RELEVANT (dstmt_vinfo) < vect_used_by_reduction);
      gcc_assert (STMT_VINFO_LIVE_P (dstmt_vinfo) 
		  || STMT_VINFO_RELEVANT (dstmt_vinfo) > vect_unused_in_loop);
      return true;
    }

  vect_mark_relevant (worklist, def_stmt, relevant, live_p);
  return true;
}


/* Function vect_mark_stmts_to_be_vectorized.

   Not all stmts in the loop need to be vectorized. For example:

     for i...
       for j...
   1.    T0 = i + j
   2.	 T1 = a[T0]

   3.    j = j + 1

   Stmt 1 and 3 do not need to be vectorized, because loop control and
   addressing of vectorized data-refs are handled differently.

   This pass detects such stmts.  */

static bool
vect_mark_stmts_to_be_vectorized (loop_vec_info loop_vinfo)
{
  VEC(tree,heap) *worklist;
  struct loop *loop = LOOP_VINFO_LOOP (loop_vinfo);
  basic_block *bbs = LOOP_VINFO_BBS (loop_vinfo);
  unsigned int nbbs = loop->num_nodes;
  block_stmt_iterator si;
  tree stmt;
  stmt_ann_t ann;
  unsigned int i;
  stmt_vec_info stmt_vinfo;
  basic_block bb;
  tree phi;
  bool live_p;
  enum vect_relevant relevant;

  if (vect_print_dump_info (REPORT_DETAILS))
    fprintf (vect_dump, "=== vect_mark_stmts_to_be_vectorized ===");

  worklist = VEC_alloc (tree, heap, 64);

  /* 1. Init worklist.  */
  for (i = 0; i < nbbs; i++)
    {
      bb = bbs[i];
      for (phi = phi_nodes (bb); phi; phi = PHI_CHAIN (phi))
	{ 
	  if (vect_print_dump_info (REPORT_DETAILS))
	    {
	      fprintf (vect_dump, "init: phi relevant? ");
	      print_generic_expr (vect_dump, phi, TDF_SLIM);
	    }

	  if (vect_stmt_relevant_p (phi, loop_vinfo, &relevant, &live_p))
	    vect_mark_relevant (&worklist, phi, relevant, live_p);
	}
      for (si = bsi_start (bb); !bsi_end_p (si); bsi_next (&si))
	{
	  stmt = bsi_stmt (si);
	  if (vect_print_dump_info (REPORT_DETAILS))
	    {
	      fprintf (vect_dump, "init: stmt relevant? ");
	      print_generic_expr (vect_dump, stmt, TDF_SLIM);
	    } 

	  if (vect_stmt_relevant_p (stmt, loop_vinfo, &relevant, &live_p))
            vect_mark_relevant (&worklist, stmt, relevant, live_p);
	}
    }

  /* 2. Process_worklist */
  while (VEC_length (tree, worklist) > 0)
    {
      use_operand_p use_p;
      ssa_op_iter iter;

      stmt = VEC_pop (tree, worklist);
      if (vect_print_dump_info (REPORT_DETAILS))
	{
          fprintf (vect_dump, "worklist: examine stmt: ");
          print_generic_expr (vect_dump, stmt, TDF_SLIM);
	}

      /* Examine the USEs of STMT. For each USE, mark the stmt that defines it 
	 (DEF_STMT) as relevant/irrelevant and live/dead according to the 
	 liveness and relevance properties of STMT.  */
      ann = stmt_ann (stmt);
      stmt_vinfo = vinfo_for_stmt (stmt);
      relevant = STMT_VINFO_RELEVANT (stmt_vinfo);
      live_p = STMT_VINFO_LIVE_P (stmt_vinfo);

      /* Generally, the liveness and relevance properties of STMT are
	 propagated as is to the DEF_STMTs of its USEs:
	  live_p <-- STMT_VINFO_LIVE_P (STMT_VINFO)
	  relevant <-- STMT_VINFO_RELEVANT (STMT_VINFO)

	 One exception is when STMT has been identified as defining a reduction
	 variable; in this case we set the liveness/relevance as follows:
	   live_p = false
	   relevant = vect_used_by_reduction
	 This is because we distinguish between two kinds of relevant stmts -
	 those that are used by a reduction computation, and those that are 
	 (also) used by a regular computation. This allows us later on to 
	 identify stmts that are used solely by a reduction, and therefore the 
	 order of the results that they produce does not have to be kept.

         Reduction phis are expected to be used by a reduction stmt;  Other 
	 reduction stmts are expected to be unused in the loop.  These are the 
	 expected values of "relevant" for reduction phis/stmts in the loop:

	 relevance:				phi	stmt
	 vect_unused_in_loop				ok
	 vect_used_by_reduction			ok
	 vect_used_in_loop 						  */

      if (STMT_VINFO_DEF_TYPE (stmt_vinfo) == vect_reduction_def)
        {
	  switch (relevant)
	    {
	    case vect_unused_in_loop:
	      gcc_assert (TREE_CODE (stmt) != PHI_NODE);
	      break;
	    case vect_used_by_reduction:
	      if (TREE_CODE (stmt) == PHI_NODE)
		break;
	    case vect_used_in_loop:
	    default:
	      if (vect_print_dump_info (REPORT_DETAILS))
	        fprintf (vect_dump, "unsupported use of reduction.");
	      VEC_free (tree, heap, worklist);
	      return false;
	    }
	  relevant = vect_used_by_reduction;
	  live_p = false;	
	}

      FOR_EACH_PHI_OR_STMT_USE (use_p, stmt, iter, SSA_OP_USE)
	{
	  tree op = USE_FROM_PTR (use_p);
	  if (!process_use (stmt, op, loop_vinfo, live_p, relevant, &worklist))
	    {
	      VEC_free (tree, heap, worklist);
	      return false;
	    }
	}
    } /* while worklist */

  VEC_free (tree, heap, worklist);
  return true;
}


/* Function vect_can_advance_ivs_p

   In case the number of iterations that LOOP iterates is unknown at compile 
   time, an epilog loop will be generated, and the loop induction variables 
   (IVs) will be "advanced" to the value they are supposed to take just before 
   the epilog loop.  Here we check that the access function of the loop IVs
   and the expression that represents the loop bound are simple enough.
   These restrictions will be relaxed in the future.  */

static bool 
vect_can_advance_ivs_p (loop_vec_info loop_vinfo)
{
  struct loop *loop = LOOP_VINFO_LOOP (loop_vinfo);
  basic_block bb = loop->header;
  tree phi;

  /* Analyze phi functions of the loop header.  */

  if (vect_print_dump_info (REPORT_DETAILS))
    fprintf (vect_dump, "vect_can_advance_ivs_p:");

  for (phi = phi_nodes (bb); phi; phi = PHI_CHAIN (phi))
    {
      tree access_fn = NULL;
      tree evolution_part;

      if (vect_print_dump_info (REPORT_DETAILS))
	{
          fprintf (vect_dump, "Analyze phi: ");
          print_generic_expr (vect_dump, phi, TDF_SLIM);
	}

      /* Skip virtual phi's. The data dependences that are associated with
         virtual defs/uses (i.e., memory accesses) are analyzed elsewhere.  */

      if (!is_gimple_reg (SSA_NAME_VAR (PHI_RESULT (phi))))
	{
	  if (vect_print_dump_info (REPORT_DETAILS))
	    fprintf (vect_dump, "virtual phi. skip.");
	  continue;
	}

      /* Skip reduction phis.  */

      if (STMT_VINFO_DEF_TYPE (vinfo_for_stmt (phi)) == vect_reduction_def)
        {
          if (vect_print_dump_info (REPORT_DETAILS))
            fprintf (vect_dump, "reduc phi. skip.");
          continue;
        }

      /* Analyze the evolution function.  */

      access_fn = instantiate_parameters
	(loop, analyze_scalar_evolution (loop, PHI_RESULT (phi)));

      if (!access_fn)
	{
	  if (vect_print_dump_info (REPORT_DETAILS))
	    fprintf (vect_dump, "No Access function.");
	  return false;
	}

      if (vect_print_dump_info (REPORT_DETAILS))
        {
	  fprintf (vect_dump, "Access function of PHI: ");
	  print_generic_expr (vect_dump, access_fn, TDF_SLIM);
        }

      evolution_part = evolution_part_in_loop_num (access_fn, loop->num);
      
      if (evolution_part == NULL_TREE)
        {
	  if (vect_print_dump_info (REPORT_DETAILS))
	    fprintf (vect_dump, "No evolution.");
	  return false;
        }
  
      /* FORNOW: We do not transform initial conditions of IVs 
	 which evolution functions are a polynomial of degree >= 2.  */

      if (tree_is_chrec (evolution_part))
	return false;  
    }

  return true;
}


/* Function vect_get_loop_niters.

   Determine how many iterations the loop is executed.
   If an expression that represents the number of iterations
   can be constructed, place it in NUMBER_OF_ITERATIONS.
   Return the loop exit condition.  */

static tree
vect_get_loop_niters (struct loop *loop, tree *number_of_iterations)
{
  tree niters;

  if (vect_print_dump_info (REPORT_DETAILS))
    fprintf (vect_dump, "=== get_loop_niters ===");

  niters = number_of_exit_cond_executions (loop);

  if (niters != NULL_TREE
      && niters != chrec_dont_know)
    {
      *number_of_iterations = niters;

      if (vect_print_dump_info (REPORT_DETAILS))
	{
	  fprintf (vect_dump, "==> get_loop_niters:" );
	  print_generic_expr (vect_dump, *number_of_iterations, TDF_SLIM);
	}
    }

  return get_loop_exit_condition (loop);
}


/* Function vect_analyze_loop_form.

   Verify the following restrictions (some may be relaxed in the future):
   - it's an inner-most loop
   - number of BBs = 2 (which are the loop header and the latch)
   - the loop has a pre-header
   - the loop has a single entry and exit
   - the loop exit condition is simple enough, and the number of iterations
     can be analyzed (a countable loop).  */

static loop_vec_info
vect_analyze_loop_form (struct loop *loop)
{
  loop_vec_info loop_vinfo;
  tree loop_cond;
  tree number_of_iterations = NULL;

  if (vect_print_dump_info (REPORT_DETAILS))
    fprintf (vect_dump, "=== vect_analyze_loop_form ===");

  if (loop->inner)
    {
      if (vect_print_dump_info (REPORT_OUTER_LOOPS))
        fprintf (vect_dump, "not vectorized: nested loop.");
      return NULL;
    }
  
  if (!single_exit (loop) 
      || loop->num_nodes != 2
      || EDGE_COUNT (loop->header->preds) != 2)
    {
      if (vect_print_dump_info (REPORT_BAD_FORM_LOOPS))
        {
          if (!single_exit (loop))
            fprintf (vect_dump, "not vectorized: multiple exits.");
          else if (loop->num_nodes != 2)
            fprintf (vect_dump, "not vectorized: too many BBs in loop.");
          else if (EDGE_COUNT (loop->header->preds) != 2)
            fprintf (vect_dump, "not vectorized: too many incoming edges.");
        }

      return NULL;
    }

  /* We assume that the loop exit condition is at the end of the loop. i.e,
     that the loop is represented as a do-while (with a proper if-guard
     before the loop if needed), where the loop header contains all the
     executable statements, and the latch is empty.  */
  if (!empty_block_p (loop->latch)
        || phi_nodes (loop->latch))
    {
      if (vect_print_dump_info (REPORT_BAD_FORM_LOOPS))
        fprintf (vect_dump, "not vectorized: unexpected loop form.");
      return NULL;
    }

  /* Make sure there exists a single-predecessor exit bb:  */
  if (!single_pred_p (single_exit (loop)->dest))
    {
      edge e = single_exit (loop);
      if (!(e->flags & EDGE_ABNORMAL))
	{
	  split_loop_exit_edge (e);
	  if (vect_print_dump_info (REPORT_DETAILS))
	    fprintf (vect_dump, "split exit edge.");
	}
      else
	{
	  if (vect_print_dump_info (REPORT_BAD_FORM_LOOPS))
	    fprintf (vect_dump, "not vectorized: abnormal loop exit edge.");
	  return NULL;
	}
    }

  if (empty_block_p (loop->header))
    {
      if (vect_print_dump_info (REPORT_BAD_FORM_LOOPS))
        fprintf (vect_dump, "not vectorized: empty loop.");
      return NULL;
    }

  loop_cond = vect_get_loop_niters (loop, &number_of_iterations);
  if (!loop_cond)
    {
      if (vect_print_dump_info (REPORT_BAD_FORM_LOOPS))
	fprintf (vect_dump, "not vectorized: complicated exit condition.");
      return NULL;
    }
  
  if (!number_of_iterations) 
    {
      if (vect_print_dump_info (REPORT_BAD_FORM_LOOPS))
	fprintf (vect_dump, 
		 "not vectorized: number of iterations cannot be computed.");
      return NULL;
    }

  if (chrec_contains_undetermined (number_of_iterations))
    {
      if (vect_print_dump_info (REPORT_BAD_FORM_LOOPS))
        fprintf (vect_dump, "Infinite number of iterations.");
      return false;
    }

  if (!NITERS_KNOWN_P (number_of_iterations))
    {
      if (vect_print_dump_info (REPORT_DETAILS))
        {
          fprintf (vect_dump, "Symbolic number of iterations is ");
          print_generic_expr (vect_dump, number_of_iterations, TDF_DETAILS);
        }
    }
  else if (TREE_INT_CST_LOW (number_of_iterations) == 0)
    {
      if (vect_print_dump_info (REPORT_UNVECTORIZED_LOOPS))
        fprintf (vect_dump, "not vectorized: number of iterations = 0.");
      return NULL;
    }

  loop_vinfo = new_loop_vec_info (loop);
  LOOP_VINFO_NITERS (loop_vinfo) = number_of_iterations;
  LOOP_VINFO_EXIT_COND (loop_vinfo) = loop_cond;

  gcc_assert (!loop->aux);
  loop->aux = loop_vinfo;
  return loop_vinfo;
}


/* Function vect_analyze_loop.

   Apply a set of analyses on LOOP, and create a loop_vec_info struct
   for it. The different analyses will record information in the
   loop_vec_info struct.  */
loop_vec_info
vect_analyze_loop (struct loop *loop)
{
  bool ok;
  loop_vec_info loop_vinfo;

  if (vect_print_dump_info (REPORT_DETAILS))
    fprintf (vect_dump, "===== analyze_loop_nest =====");

  /* Check the CFG characteristics of the loop (nesting, entry/exit, etc.  */

  loop_vinfo = vect_analyze_loop_form (loop);
  if (!loop_vinfo)
    {
      if (vect_print_dump_info (REPORT_DETAILS))
	fprintf (vect_dump, "bad loop form.");
      return NULL;
    }

  /* Find all data references in the loop (which correspond to vdefs/vuses)
     and analyze their evolution in the loop.

     FORNOW: Handle only simple, array references, which
     alignment can be forced, and aligned pointer-references.  */

  ok = vect_analyze_data_refs (loop_vinfo);
  if (!ok)
    {
      if (vect_print_dump_info (REPORT_DETAILS))
	fprintf (vect_dump, "bad data references.");
      destroy_loop_vec_info (loop_vinfo);
      return NULL;
    }

  /* Classify all cross-iteration scalar data-flow cycles.
     Cross-iteration cycles caused by virtual phis are analyzed separately.  */

  vect_analyze_scalar_cycles (loop_vinfo);

  vect_pattern_recog (loop_vinfo);

  /* Data-flow analysis to detect stmts that do not need to be vectorized.  */

  ok = vect_mark_stmts_to_be_vectorized (loop_vinfo);
  if (!ok)
    {
      if (vect_print_dump_info (REPORT_DETAILS))
	fprintf (vect_dump, "unexpected pattern.");
      destroy_loop_vec_info (loop_vinfo);
      return NULL;
    }

  /* Analyze the alignment of the data-refs in the loop.
     Fail if a data reference is found that cannot be vectorized.  */

  ok = vect_analyze_data_refs_alignment (loop_vinfo);
  if (!ok)
    {
      if (vect_print_dump_info (REPORT_DETAILS))
	fprintf (vect_dump, "bad data alignment.");
      destroy_loop_vec_info (loop_vinfo);
      return NULL;
    }

  ok = vect_determine_vectorization_factor (loop_vinfo);
  if (!ok)
    {
      if (vect_print_dump_info (REPORT_DETAILS))
        fprintf (vect_dump, "can't determine vectorization factor.");
      destroy_loop_vec_info (loop_vinfo);
      return NULL;
    }

  /* Analyze data dependences between the data-refs in the loop. 
     FORNOW: fail at the first data dependence that we encounter.  */

  ok = vect_analyze_data_ref_dependences (loop_vinfo);
  if (!ok)
    {
      if (vect_print_dump_info (REPORT_DETAILS))
	fprintf (vect_dump, "bad data dependence.");
      destroy_loop_vec_info (loop_vinfo);
      return NULL;
    }

  /* Analyze the access patterns of the data-refs in the loop (consecutive,
     complex, etc.). FORNOW: Only handle consecutive access pattern.  */

  ok = vect_analyze_data_ref_accesses (loop_vinfo);
  if (!ok)
    {
      if (vect_print_dump_info (REPORT_DETAILS))
	fprintf (vect_dump, "bad data access.");
      destroy_loop_vec_info (loop_vinfo);
      return NULL;
    }

  /* This pass will decide on using loop versioning and/or loop peeling in
     order to enhance the alignment of data references in the loop.  */

  ok = vect_enhance_data_refs_alignment (loop_vinfo);
  if (!ok)
    {
      if (vect_print_dump_info (REPORT_DETAILS))
	fprintf (vect_dump, "bad data alignment.");
      destroy_loop_vec_info (loop_vinfo);
      return NULL;
    }

  /* Scan all the operations in the loop and make sure they are
     vectorizable.  */

  ok = vect_analyze_operations (loop_vinfo);
  if (!ok)
    {
      if (vect_print_dump_info (REPORT_DETAILS))
	fprintf (vect_dump, "bad operation or unsupported loop bound.");
      destroy_loop_vec_info (loop_vinfo);
      return NULL;
    }

  LOOP_VINFO_VECTORIZABLE_P (loop_vinfo) = 1;

  return loop_vinfo;
}
