/* Data and Control Flow Analysis for Trees.
   Copyright (C) 2001-2013 Free Software Foundation, Inc.
   Contributed by Diego Novillo <dnovillo@redhat.com>

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3, or (at your option)
any later version.

GCC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING3.  If not see
<http://www.gnu.org/licenses/>.  */

#ifndef _TREE_FLOW_H
#define _TREE_FLOW_H 1

#include "bitmap.h"
#include "sbitmap.h"
#include "basic-block.h"
#include "hashtab.h"
#include "gimple.h"
#include "tree-ssa-operands.h"
#include "cgraph.h"
#include "ipa-reference.h"
#include "tree-ssa-alias.h"
#include "tree-cfgcleanup.h"
#include "tree-dfa.h"
#include "tree-pretty-print.h"
#include "gimple-low.h"
#include "tree-into-ssa.h"
#include "tree-ssa-loop.h"

/* This structure is used to map a gimple statement to a label,
   or list of labels to represent transaction restart.  */

struct GTY(()) tm_restart_node {
  gimple stmt;
  tree label_or_list;
};

/* Gimple dataflow datastructure. All publicly available fields shall have
   gimple_ accessor defined in tree-flow-inline.h, all publicly modifiable
   fields should have gimple_set accessor.  */
struct GTY(()) gimple_df {
  /* A vector of all the noreturn calls passed to modify_stmt.
     cleanup_control_flow uses it to detect cases where a mid-block
     indirect call has been turned into a noreturn call.  When this
     happens, all the instructions after the call are no longer
     reachable and must be deleted as dead.  */
  vec<gimple, va_gc> *modified_noreturn_calls;

  /* Array of all SSA_NAMEs used in the function.  */
  vec<tree, va_gc> *ssa_names;

  /* Artificial variable used for the virtual operand FUD chain.  */
  tree vop;

  /* The PTA solution for the ESCAPED artificial variable.  */
  struct pt_solution escaped;

  /* A map of decls to artificial ssa-names that point to the partition
     of the decl.  */
  struct pointer_map_t * GTY((skip(""))) decls_to_pointers;

  /* Free list of SSA_NAMEs.  */
  vec<tree, va_gc> *free_ssanames;

  /* Hashtable holding definition for symbol.  If this field is not NULL, it
     means that the first reference to this variable in the function is a
     USE or a VUSE.  In those cases, the SSA renamer creates an SSA name
     for this variable with an empty defining statement.  */
  htab_t GTY((param_is (union tree_node))) default_defs;

  /* True if there are any symbols that need to be renamed.  */
  unsigned int ssa_renaming_needed : 1;

  /* True if all virtual operands need to be renamed.  */
  unsigned int rename_vops : 1;

  /* True if the code is in ssa form.  */
  unsigned int in_ssa_p : 1;

  /* True if IPA points-to information was computed for this function.  */
  unsigned int ipa_pta : 1;

  struct ssa_operands ssa_operands;

  /* Map gimple stmt to tree label (or list of labels) for transaction
     restart and abort.  */
  htab_t GTY ((param_is (struct tm_restart_node))) tm_restart;
};

static inline int get_lineno (const_gimple);

/*---------------------------------------------------------------------------
			      Global declarations
---------------------------------------------------------------------------*/
struct int_tree_map {
  unsigned int uid;
  tree to;
};

/* Macros for showing usage statistics.  */
#define SCALE(x) ((unsigned long) ((x) < 1024*10	\
		  ? (x)					\
		  : ((x) < 1024*1024*10			\
		     ? (x) / 1024			\
		     : (x) / (1024*1024))))

#define LABEL(x) ((x) < 1024*10 ? 'b' : ((x) < 1024*1024*10 ? 'k' : 'M'))

#define PERCENT(x,y) ((float)(x) * 100.0 / (float)(y))

/*---------------------------------------------------------------------------
			      OpenMP Region Tree
---------------------------------------------------------------------------*/

/* Parallel region information.  Every parallel and workshare
   directive is enclosed between two markers, the OMP_* directive
   and a corresponding OMP_RETURN statement.  */

struct omp_region
{
  /* The enclosing region.  */
  struct omp_region *outer;

  /* First child region.  */
  struct omp_region *inner;

  /* Next peer region.  */
  struct omp_region *next;

  /* Block containing the omp directive as its last stmt.  */
  basic_block entry;

  /* Block containing the OMP_RETURN as its last stmt.  */
  basic_block exit;

  /* Block containing the OMP_CONTINUE as its last stmt.  */
  basic_block cont;

  /* If this is a combined parallel+workshare region, this is a list
     of additional arguments needed by the combined parallel+workshare
     library call.  */
  vec<tree, va_gc> *ws_args;

  /* The code for the omp directive of this region.  */
  enum gimple_code type;

  /* Schedule kind, only used for OMP_FOR type regions.  */
  enum omp_clause_schedule_kind sched_kind;

  /* True if this is a combined parallel+workshare region.  */
  bool is_combined_parallel;
};

extern struct omp_region *root_omp_region;
extern struct omp_region *new_omp_region (basic_block, enum gimple_code,
					  struct omp_region *);
extern void free_omp_regions (void);
void omp_expand_local (basic_block);
tree copy_var_decl (tree, tree, tree);

/*---------------------------------------------------------------------------
			      Function prototypes
---------------------------------------------------------------------------*/
/* In tree-cfg.c  */

/* Location to track pending stmt for edge insertion.  */
#define PENDING_STMT(e)	((e)->insns.g)

extern void delete_tree_cfg_annotations (void);
extern bool stmt_ends_bb_p (gimple);
extern bool is_ctrl_stmt (gimple);
extern bool is_ctrl_altering_stmt (gimple);
extern bool simple_goto_p (gimple);
extern bool stmt_can_make_abnormal_goto (gimple);
extern basic_block single_noncomplex_succ (basic_block bb);
extern void gimple_dump_bb (FILE *, basic_block, int, int);
extern void gimple_debug_bb (basic_block);
extern basic_block gimple_debug_bb_n (int);
extern void gimple_dump_cfg (FILE *, int);
extern void gimple_debug_cfg (int);
extern void dump_cfg_stats (FILE *);
extern void dot_cfg (void);
extern void debug_cfg_stats (void);
extern void debug_loops (int);
extern void debug_loop (struct loop *, int);
extern void debug (struct loop &ref);
extern void debug (struct loop *ptr);
extern void debug_verbose (struct loop &ref);
extern void debug_verbose (struct loop *ptr);
extern void debug_loop_num (unsigned, int);
extern void print_loops (FILE *, int);
extern void print_loops_bb (FILE *, basic_block, int, int);
extern void cleanup_dead_labels (void);
extern void group_case_labels_stmt (gimple);
extern void group_case_labels (void);
extern gimple first_stmt (basic_block);
extern gimple last_stmt (basic_block);
extern gimple last_and_only_stmt (basic_block);
extern edge find_taken_edge (basic_block, tree);
extern basic_block label_to_block_fn (struct function *, tree);
#define label_to_block(t) (label_to_block_fn (cfun, t))
extern void notice_special_calls (gimple);
extern void clear_special_calls (void);
extern void verify_gimple_in_seq (gimple_seq);
extern void verify_gimple_in_cfg (struct function *);
extern tree gimple_block_label (basic_block);
extern void extract_true_false_edges_from_block (basic_block, edge *, edge *);
extern bool gimple_duplicate_sese_region (edge, edge, basic_block *, unsigned,
					basic_block *, bool);
extern bool gimple_duplicate_sese_tail (edge, edge, basic_block *, unsigned,
				      basic_block *);
extern void gather_blocks_in_sese_region (basic_block entry, basic_block exit,
					  vec<basic_block> *bbs_p);
extern void add_phi_args_after_copy_bb (basic_block);
extern void add_phi_args_after_copy (basic_block *, unsigned, edge);
extern bool gimple_purge_dead_eh_edges (basic_block);
extern bool gimple_purge_all_dead_eh_edges (const_bitmap);
extern bool gimple_purge_dead_abnormal_call_edges (basic_block);
extern bool gimple_purge_all_dead_abnormal_call_edges (const_bitmap);
extern tree gimplify_build1 (gimple_stmt_iterator *, enum tree_code,
			     tree, tree);
extern tree gimplify_build2 (gimple_stmt_iterator *, enum tree_code,
			     tree, tree, tree);
extern tree gimplify_build3 (gimple_stmt_iterator *, enum tree_code,
			     tree, tree, tree, tree);
extern void init_empty_tree_cfg (void);
extern void init_empty_tree_cfg_for_function (struct function *);
extern void fold_cond_expr_cond (void);
extern void make_abnormal_goto_edges (basic_block, bool);
extern void replace_uses_by (tree, tree);
extern void start_recording_case_labels (void);
extern void end_recording_case_labels (void);
extern basic_block move_sese_region_to_fn (struct function *, basic_block,
				           basic_block, tree);
void remove_edge_and_dominated_blocks (edge);
bool tree_node_can_be_shared (tree);



/* In tree-ssa-loop*.c  */

unsigned tree_predictive_commoning (void);
bool parallelize_loops (void);

bool convert_affine_scev (struct loop *, tree, tree *, tree *, gimple, bool);

enum ev_direction {EV_DIR_GROWS, EV_DIR_DECREASES, EV_DIR_UNKNOWN};
enum ev_direction scev_direction (const_tree);

struct loop *slpeel_tree_duplicate_loop_to_edge_cfg (struct loop *, edge);

/* In tree-ssa-threadedge.c */
extern void threadedge_initialize_values (void);
extern void threadedge_finalize_values (void);
extern vec<tree> ssa_name_values;
#define SSA_NAME_VALUE(x) \
    (SSA_NAME_VERSION (x) < ssa_name_values.length () \
     ? ssa_name_values[SSA_NAME_VERSION (x)] \
     : NULL_TREE)
extern void set_ssa_name_value (tree, tree);
extern bool potentially_threadable_block (basic_block);
extern void thread_across_edge (gimple, edge, bool,
				vec<tree> *, tree (*) (gimple, gimple));
extern void propagate_threaded_block_debug_into (basic_block, basic_block);

/* In tree-loop-linear.c  */
extern void linear_transform_loops (void);
extern unsigned perfect_loop_nest_depth (struct loop *);

/* In graphite.c  */
extern void graphite_transform_loops (void);

/* In tree-data-ref.c  */
extern void tree_check_data_deps (void);

/* In gimplify.c  */
tree force_gimple_operand_1 (tree, gimple_seq *, gimple_predicate, tree);
tree force_gimple_operand (tree, gimple_seq *, bool, tree);
tree force_gimple_operand_gsi_1 (gimple_stmt_iterator *, tree,
				 gimple_predicate, tree,
				 bool, enum gsi_iterator_update);
tree force_gimple_operand_gsi (gimple_stmt_iterator *, tree, bool, tree,
			       bool, enum gsi_iterator_update);
tree gimple_fold_indirect_ref (tree);

/* In tree-ssa-live.c */
extern void remove_unused_locals (void);
extern void dump_scope_blocks (FILE *, int);
extern void debug_scope_blocks (int);
extern void debug_scope_block (tree, int);

/* In tree-ssa-address.c  */

/* Description of a memory address.  */

struct mem_address
{
  tree symbol, base, index, step, offset;
};

struct affine_tree_combination;
tree create_mem_ref (gimple_stmt_iterator *, tree,
		     struct affine_tree_combination *, tree, tree, tree, bool);
rtx addr_for_mem_ref (struct mem_address *, addr_space_t, bool);
void get_address_description (tree, struct mem_address *);
tree maybe_fold_tmr (tree);

unsigned int execute_fixup_cfg (void);

/* In ipa-pure-const.c  */
void warn_function_noreturn (tree);

/* In tree-parloops.c  */
bool parallelized_function_p (tree);

#include "tree-flow-inline.h"

#endif /* _TREE_FLOW_H  */
