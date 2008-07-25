/* Interprocedural analyses.
   Copyright (C) 2005, 2007 Free Software Foundation, Inc.

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
#include "tree.h"
#include "langhooks.h"
#include "ggc.h"
#include "target.h"
#include "cgraph.h"
#include "ipa-prop.h"
#include "tree-flow.h"
#include "tree-pass.h"
#include "tree-inline.h"
#include "flags.h"
#include "timevar.h"
#include "flags.h"
#include "diagnostic.h"

/* Vector where the parameter infos are actually stored. */
VEC (ipa_node_params_t, heap) *ipa_node_params_vector;
/* Vector where the parameter infos are actually stored. */
VEC (ipa_edge_args_t, heap) *ipa_edge_args_vector;

/* Holders of ipa cgraph hooks: */
struct cgraph_edge_hook_list *edge_removal_hook_holder;
struct cgraph_node_hook_list *node_removal_hook_holder;
struct cgraph_2edge_hook_list *edge_duplication_hook_holder;
struct cgraph_2node_hook_list *node_duplication_hook_holder;

/* Initialize worklist to contain all functions.  */
struct ipa_func_list *
ipa_init_func_list (void)
{
  struct cgraph_node *node;
  struct ipa_func_list * wl;

  wl = NULL;
  for (node = cgraph_nodes; node; node = node->next)
    if (node->analyzed)
      {
	/* Unreachable nodes should have been eliminated before ipcp and
	   inlining.  */
	gcc_assert (node->needed || node->reachable);
	ipa_push_func_to_list (&wl, node);
      }

  return wl;
}

/* Add cgraph node MT to the worklist. Set worklist element WL
   to point to MT.  */
void
ipa_push_func_to_list (struct ipa_func_list **wl, struct cgraph_node *mt)
{
  struct ipa_func_list *temp;

  temp = XCNEW (struct ipa_func_list);
  temp->node = mt;
  temp->next = *wl;
  *wl = temp;
}

/* Remove a function from the worklist. WL points to the first
   element in the list, which is removed.  */
struct cgraph_node *
ipa_pop_func_from_list (struct ipa_func_list ** wl)
{
  struct ipa_func_list *first;
  struct cgraph_node *return_func;

  first = *wl;
  *wl = (*wl)->next;
  return_func = first->node;
  free (first);
  return return_func;
}

/* Return index of the formal whose tree is ptree in function which corresponds
   to info.  */
static int
ipa_get_param_decl_index (struct ipa_node_params *info, tree ptree)
{
  int i, count;

  count = ipa_get_param_count (info);
  for (i = 0; i < count; i++)
    if (ipa_get_ith_param(info, i) == ptree)
      return i;

  return -1;
}

/* Insert the formal trees to the param_decls array in function MT.  */
void
ipa_create_param_decls_array (struct cgraph_node *mt)
{
  tree fndecl;
  tree fnargs;
  tree parm;
  int param_num;
  struct ipa_node_params *info = IPA_NODE_REF (mt);

  if (info->param_decls)
    return;

  info->param_decls = XCNEWVEC (tree, ipa_get_param_count (info));
  fndecl = mt->decl;
  fnargs = DECL_ARGUMENTS (fndecl);
  param_num = 0;
  for (parm = fnargs; parm; parm = TREE_CHAIN (parm))
    {
      info->param_decls[param_num] = parm;
      param_num++;
    }
}

/* Count number of formals in MT. Insert the result to the 
   ipa_node_params.  */
void
ipa_count_formal_params (struct cgraph_node *mt)
{
  tree fndecl;
  tree fnargs;
  tree parm;
  int param_num;

  fndecl = mt->decl;
  fnargs = DECL_ARGUMENTS (fndecl);
  param_num = 0;
  for (parm = fnargs; parm; parm = TREE_CHAIN (parm))
    param_num++;
  ipa_set_param_count (IPA_NODE_REF (mt), param_num);
}

/* Check STMT to detect whether a formal parameter is directly modified within
   STMT, the appropriate entry is updated in the modified flags of INFO.
   Directly means that this function does not check for modifications through
   pointers or escaping addresses because all TREE_ADDRESSABLE parameters are
   considered modified anyway.  */
static void
ipa_check_stmt_modifications (struct ipa_node_params *info, tree stmt)
{
  int j;
  int index;
  tree lhs;

  switch (TREE_CODE (stmt))
    {
    case GIMPLE_MODIFY_STMT:
      lhs = GIMPLE_STMT_OPERAND (stmt, 0);

      while (handled_component_p (lhs))
	lhs = TREE_OPERAND (lhs, 0);
      if (TREE_CODE (lhs) == SSA_NAME)
	lhs = SSA_NAME_VAR (lhs);
      index = ipa_get_param_decl_index (info, lhs);
      if (index >= 0)
	info->param_flags[index].modified = true;
      break;

    case ASM_EXPR:
      /* Asm code could modify any of the parameters.  */
      for (j = 0; j < ipa_get_param_count (info); j++)
	info->param_flags[j].modified = true;
      break;

    default:
      break;
    }
}

/* Compute which formal parameters of function associated with NODE are locally
   modified.  Parameters may be modified in NODE if they are TREE_ADDRESSABLE,
   if they appear on the left hand side of an assignment or if there is an
   ASM_EXPR in the function.  */
void
ipa_detect_param_modifications (struct cgraph_node *node)
{
  tree decl = node->decl;
  basic_block bb;
  struct function *func;
  block_stmt_iterator bsi;
  tree stmt;
  struct ipa_node_params *info = IPA_NODE_REF (node);
  int i, count;

  if (ipa_get_param_count (info) == 0 || info->modification_analysis_done)
    return;

  if (!info->param_flags)
    info->param_flags = XCNEWVEC (struct ipa_param_flags,
				  ipa_get_param_count (info));

  func = DECL_STRUCT_FUNCTION (decl);
  FOR_EACH_BB_FN (bb, func)
    {
      for (bsi = bsi_start (bb); !bsi_end_p (bsi); bsi_next (&bsi))
	{
	  stmt = bsi_stmt (bsi);
	  ipa_check_stmt_modifications (info, stmt);
	}
    }

  count = ipa_get_param_count (info);
  for (i = 0; i < count; i++)
    if (TREE_ADDRESSABLE (ipa_get_ith_param (info, i)))
      info->param_flags[i].modified = true;

  info->modification_analysis_done = 1;
}

/* Count number of arguments callsite CS has and store it in
   ipa_edge_args structure corresponding to this callsite.  */
void
ipa_count_arguments (struct cgraph_edge *cs)
{
  tree call_tree;
  int arg_num;

  call_tree = get_call_expr_in (cs->call_stmt);
  gcc_assert (TREE_CODE (call_tree) == CALL_EXPR);
  arg_num = call_expr_nargs (call_tree);
  ipa_set_cs_argument_count (IPA_EDGE_REF (cs), arg_num);
}

/* The following function prints the jump functions of all arguments on all
   call graph edges going from NODE to file F.  */
void
ipa_print_node_jump_functions (FILE *f, struct cgraph_node *node)
{
  int i, count;
  struct cgraph_edge *cs;
  struct ipa_jump_func *jump_func;
  enum jump_func_type type;

  fprintf (f, "JUMP FUNCTIONS OF CALLER  %s:\n", cgraph_node_name (node));
  for (cs = node->callees; cs; cs = cs->next_callee)
    {
      if (!ipa_edge_args_info_available_for_edge_p (cs))
	continue;

      fprintf (f, "callsite  %s ", cgraph_node_name (node));
      fprintf (f, "-> %s :: \n", cgraph_node_name (cs->callee));

      count = ipa_get_cs_argument_count (IPA_EDGE_REF (cs));
      for (i = 0; i < count; i++)
	{
	  jump_func = ipa_get_ith_jump_func (IPA_EDGE_REF (cs), i);
	  type = jump_func->type;

	  fprintf (f, "  param %d: ", i);
	  if (type == IPA_UNKNOWN)
	    fprintf (f, "UNKNOWN\n");
	  else if (type == IPA_CONST || type == IPA_CONST_REF)
 	    {
	      tree val = jump_func->value.constant;
	      fprintf (f, "CONST: ");
	      print_generic_expr (f, val, 0);
	      fprintf (f, "\n");
	    }
	  else if (type == IPA_CONST_MEMBER_PTR)
	    {
	      fprintf (f, "CONST MEMBER PTR: ");
	      print_generic_expr (f, jump_func->value.member_cst.pfn, 0);
	      fprintf (f, ", ");
	      print_generic_expr (f, jump_func->value.member_cst.delta, 0);
	      fprintf (f, "\n");
	    }
	  else if (type == IPA_PASS_THROUGH)
 	    {
	      fprintf (f, "PASS THROUGH: ");
	      fprintf (f, "%d\n", jump_func->value.formal_id);
 	    }
	}
    }
}

/* Print ipa_jump_func data structures of all nodes in the call graph to F.  */
void
ipa_print_all_jump_functions (FILE *f)
{
  struct cgraph_node *node;

  fprintf (f, "\nCALLSITE PARAM PRINT\n");
  for (node = cgraph_nodes; node; node = node->next)
    {
      ipa_print_node_jump_functions (f, node);
    }
}

/* The following function determines the jump functions of scalar arguments.
   Scalar means SSA names and constants of a number of selected types.  INFO is
   the ipa_node_params structure associated with the caller, FUNCTIONS is a
   pointer to an array of jump function structures associated with CALL which
   is the call statement being examined.*/
static void
compute_scalar_jump_functions (struct ipa_node_params *info,
			       struct ipa_jump_func *functions,
			       tree call)
{
  call_expr_arg_iterator iter;
  tree arg;
  int num = 0;

  FOR_EACH_CALL_EXPR_ARG (arg, iter, call)
    {
      if (TREE_CODE (arg) == INTEGER_CST
	  || TREE_CODE (arg) == REAL_CST
	  || TREE_CODE (arg) == FIXED_CST)
	{
	  functions[num].type = IPA_CONST;
	  functions[num].value.constant = arg;
	}
      else if (TREE_CODE (arg) == ADDR_EXPR)
	{
	  if (TREE_CODE (TREE_OPERAND (arg, 0)) == FUNCTION_DECL)
	    {
	      functions[num].type = IPA_CONST;
	      functions[num].value.constant = TREE_OPERAND (arg, 0);
	    }
	  else if (TREE_CODE (TREE_OPERAND (arg, 0)) == CONST_DECL)
	    {
	      tree cst_decl = TREE_OPERAND (arg, 0);

	      if (TREE_CODE (DECL_INITIAL (cst_decl)) == INTEGER_CST
		  || TREE_CODE (DECL_INITIAL (cst_decl)) == REAL_CST
		  || TREE_CODE (DECL_INITIAL (cst_decl)) == FIXED_CST)
		{
		  functions[num].type = IPA_CONST_REF;
		  functions[num].value.constant = cst_decl;
		}
	    }
 	}
      else if ((TREE_CODE (arg) == SSA_NAME) && SSA_NAME_IS_DEFAULT_DEF (arg))
	{
	  int index = ipa_get_param_decl_index (info, SSA_NAME_VAR (arg));

	  if (index >= 0)
	    {
	      functions[num].type = IPA_PASS_THROUGH;
	      functions[num].value.formal_id = index;
	    }
	}

      num++;
    }
}

/* This function inspects the given TYPE and returns true iff it has the same
   structure (the same number of fields of the same types) as a C++ member
   pointer.  If METHOD_PTR and DELTA are non-NULL, the trees representing the
   corresponding fields are stored there.  */
static bool
type_like_member_ptr_p (tree type, tree *method_ptr, tree *delta)
{
  tree fld;

  if (TREE_CODE (type) != RECORD_TYPE)
    return false;

  fld = TYPE_FIELDS (type);
  if (!fld || !POINTER_TYPE_P (TREE_TYPE (fld))
      || TREE_CODE (TREE_TYPE (TREE_TYPE (fld))) != METHOD_TYPE)
    return false;

  if (method_ptr)
    *method_ptr = fld;

  fld = TREE_CHAIN (fld);
  if (!fld || INTEGRAL_TYPE_P (fld))
    return false;
  if (delta)
    *delta = fld;

  if (TREE_CHAIN (fld))
    return false;

  return true;
}

/* This function goes through arguments of the CALL and for every one that
   looks like a member pointer, it checks whether it can be safely declared
   pass-through and if so, marks that to the corresponding item of jum
   FUNCTIONS .  It returns true iff there were non-pass-through member pointers
   within the arguments.  INFO describes formal parameters of the caller.  */
static bool
compute_pass_through_member_ptrs (struct ipa_node_params *info,
				  struct ipa_jump_func *functions,
				  tree call)
{
  call_expr_arg_iterator iter;
  bool undecided_members = false;
  int num = 0;
  tree arg;

  FOR_EACH_CALL_EXPR_ARG (arg, iter, call)
    {
      if (type_like_member_ptr_p (TREE_TYPE (arg), NULL, NULL))
	{
	  if (TREE_CODE (arg) == PARM_DECL)
	    {
	      int index = ipa_get_param_decl_index (info, arg);

	      gcc_assert (index >=0);
	      if (!ipa_is_ith_param_modified (info, index))
		{
		  functions[num].type = IPA_PASS_THROUGH;
		  functions[num].value.formal_id = index;
		}
	      else
		undecided_members = true;
	    }
	  else
	    undecided_members = true;
	}

      num++;
    }

  return undecided_members;
}

/* Simple function filling in a member pointer constant jump function (with PFN
   and DELTA as the constant value) into JFUNC.  */
static void
fill_member_ptr_cst_jump_function (struct ipa_jump_func *jfunc,
				   tree pfn, tree delta)
{
  jfunc->type = IPA_CONST_MEMBER_PTR;
  jfunc->value.member_cst.pfn = pfn;
  jfunc->value.member_cst.delta = delta;
}

/* Traverse statements from CALL_STMT backwards, scanning whether the argument
   ARG which is a member pointer is filled in with constant values.  If it is,
   fill the jump function JFUNC in appropriately.  METHOD_FIELD and DELTA_FIELD
   are fields of the record type of the member pointer.  To give an example, we
   look for a pattern looking like the following:  

     D.2515.__pfn ={v} printStuff;
     D.2515.__delta ={v} 0;
     i_1 = doprinting (D.2515);  */
static void
determine_cst_member_ptr (tree call_stmt, tree arg, tree method_field,
			  tree delta_field, struct ipa_jump_func *jfunc)
{
  block_stmt_iterator bsi;
  tree method = NULL_TREE;
  tree delta = NULL_TREE;

  bsi = bsi_for_stmt (call_stmt);

  bsi_prev (&bsi);
  for (; !bsi_end_p (bsi); bsi_prev (&bsi))
    {
      tree stmt = bsi_stmt (bsi);
      tree lhs, rhs, fld;

      if (TREE_CODE (stmt) != GIMPLE_MODIFY_STMT)
	return;

      rhs = GIMPLE_STMT_OPERAND (stmt, 1);
      if (TREE_CODE (rhs) == CALL_EXPR)
	return;

      lhs = GIMPLE_STMT_OPERAND (stmt, 0);

      if (TREE_CODE (lhs) != COMPONENT_REF
	  || TREE_OPERAND (lhs, 0) != arg)
	continue;

      fld = TREE_OPERAND (lhs, 1);
      if (!method && fld == method_field)
	{
	  if (TREE_CODE (rhs) == ADDR_EXPR
	      && TREE_CODE (TREE_OPERAND (rhs, 0)) == FUNCTION_DECL
	      && TREE_CODE (TREE_TYPE (TREE_OPERAND (rhs, 0))) == METHOD_TYPE)
	    {
	      method = TREE_OPERAND (rhs, 0);
	      if (delta)
		{
		  fill_member_ptr_cst_jump_function (jfunc, method, delta);
		  return;
		}
	    }
	  else
	    return;
	}

      if (!delta && fld == delta_field)
	{
	  if (TREE_CODE (rhs) == INTEGER_CST)
	    {
	      delta = rhs;
	      if (method)
		{
		  fill_member_ptr_cst_jump_function (jfunc, method, delta);
		  return;
		}
	    }
	  else
	    return;
	}
    }

  return;
}

/* Go through the arguments of the call in CALL_STMT and for every member
   pointer within tries determine whether it is a constant.  If it is, create a
   corresponding constant jump function in FUNCTIONS which is an array of jump
   functions associated with the call.  */
static void
compute_cst_member_ptr_arguments (struct ipa_jump_func *functions,
				  tree call_stmt)
{
  call_expr_arg_iterator iter;
  int num = 0;
  tree call = get_call_expr_in (call_stmt);
  tree arg, method_field, delta_field;

  FOR_EACH_CALL_EXPR_ARG (arg, iter, call)
    {
      if (functions[num].type == IPA_UNKNOWN
	  && type_like_member_ptr_p (TREE_TYPE (arg), &method_field,
				     &delta_field))
	determine_cst_member_ptr (call_stmt, arg, method_field,
				  delta_field, &functions[num]);

      num++;
    }
}

/* Compute jump function for all arguments of callsite CS and insert the
   information in the jump_functions array in the ipa_edge_args corresponding
   to this callsite.  */
void
ipa_compute_jump_functions (struct cgraph_edge *cs)
{
  struct ipa_node_params *info = IPA_NODE_REF (cs->caller);
  struct ipa_edge_args *arguments = IPA_EDGE_REF (cs);
  tree call;

  if (ipa_get_cs_argument_count (arguments) == 0 || arguments->jump_functions)
    return;
  arguments->jump_functions = XCNEWVEC (struct ipa_jump_func,
					ipa_get_cs_argument_count (arguments));
  call = get_call_expr_in (cs->call_stmt);

  /* We will deal with constants and SSA scalars first:  */
  compute_scalar_jump_functions (info, arguments->jump_functions, call);

  /* Let's check whether there are any potential member pointers and if so,
     whether we can determine their functions as pass_through.  */
  if (!compute_pass_through_member_ptrs (info, arguments->jump_functions, call))
    return;

  /* Finally, let's check whether we actually pass a new constant membeer
     pointer here...  */
  compute_cst_member_ptr_arguments (arguments->jump_functions, cs->call_stmt);
}

/* If RHS looks like a rhs of a statement loading pfn from a member pointer
   formal parameter, return the parameter, otherwise return NULL.  */
static tree
ipa_get_member_ptr_load_param (tree rhs)
{
  tree rec, fld;
  tree ptr_field;

  if (TREE_CODE (rhs) != COMPONENT_REF)
    return NULL_TREE;

  rec = TREE_OPERAND (rhs, 0);
  if (TREE_CODE (rec) != PARM_DECL
      || !type_like_member_ptr_p (TREE_TYPE (rec), &ptr_field, NULL))
    return NULL_TREE;

  fld = TREE_OPERAND (rhs, 1);
  if (fld == ptr_field)
    return rec;
  else
    return NULL_TREE;
}

/* If STMT looks like a statement loading a value from a member pointer formal
   parameter, this function retuns that parameter.  */
static tree
ipa_get_stmt_member_ptr_load_param (tree stmt)
{
  tree rhs;

  if (TREE_CODE (stmt) != GIMPLE_MODIFY_STMT)
    return NULL_TREE;

  rhs = GIMPLE_STMT_OPERAND (stmt, 1);
  return ipa_get_member_ptr_load_param (rhs);
}

/* Returns true iff T is an SSA_NAME defined by a statement.  */
static bool
ipa_is_ssa_with_stmt_def (tree t)
{
  if (TREE_CODE (t) == SSA_NAME
      && !SSA_NAME_IS_DEFAULT_DEF (t))
    return true;
  else
    return false;
}

/* Creates a new note describing a call to a parameter number FORMAL_ID and
   attaches it to the linked list of INFO.  It also sets the called flag of the
   parameter.  STMT is the corresponding call statement.  */
static void
ipa_note_param_call (struct ipa_node_params *info, int formal_id,
		     tree stmt)
{
  struct ipa_param_call_note *note;
  basic_block bb = bb_for_stmt (stmt);

  info->param_flags[formal_id].called = 1;

  note = XCNEW (struct ipa_param_call_note);
  note->formal_id = formal_id;
  note->stmt = stmt;
  note->count = bb->count;
  note->frequency = compute_call_stmt_bb_frequency (bb);

  note->next = info->param_calls;
  info->param_calls = note;

  return;
}

/* Analyze the CALL (which itself must be a part of statement STMT) and examine
   uses of formal parameters of the caller (described by INFO).  Currently it
   checks whether the call calls a pointer that is a formal parameter and if
   so, the parameter is marked with the called flag and a note describing the
   call is created.  This is very simple for ordinary pointers represented in
   SSA but not-so-nice when it comes to member pointers.  The ugly part of this
   function does nothing more than tries to match the pattern of such a call.
   An example of such a pattern is the gimple dump below, the call is on the
   last line:

     <bb 2>:
       f$__delta_5 = f.__delta;
       f$__pfn_24 = f.__pfn;
       D.2496_3 = (int) f$__pfn_24;
       D.2497_4 = D.2496_3 & 1;
       if (D.2497_4 != 0)
         goto <bb 3>;
       else
         goto <bb 4>;

     <bb 3>:
       D.2500_7 = (unsigned int) f$__delta_5;
       D.2501_8 = &S + D.2500_7;
       D.2502_9 = (int (*__vtbl_ptr_type) (void) * *) D.2501_8;
       D.2503_10 = *D.2502_9;
       D.2504_12 = f$__pfn_24 + -1;
       D.2505_13 = (unsigned int) D.2504_12;
       D.2506_14 = D.2503_10 + D.2505_13;
       D.2507_15 = *D.2506_14;
       iftmp.11_16 = (String:: *) D.2507_15;

     <bb 4>:
       # iftmp.11_1 = PHI <iftmp.11_16(3), f$__pfn_24(2)>
       D.2500_19 = (unsigned int) f$__delta_5;
       D.2508_20 = &S + D.2500_19;
       D.2493_21 = iftmp.11_1 (D.2508_20, 4);

   Such patterns are results of simple calls to a member pointer:

     int doprinting (int (MyString::* f)(int) const)
     {
       MyString S ("somestring");

       return (S.*f)(4);
     }
*/

static void
ipa_analyze_call_uses (struct ipa_node_params *info, tree call, tree stmt)
{
  tree target = CALL_EXPR_FN (call);
  tree var, def;
  tree n1, n2;
  tree d1, d2;
  tree rec, rec2;
  tree branch, cond;
  int index;

  basic_block bb, virt_bb, join;

  if (TREE_CODE (target) != SSA_NAME)
    return;

  var = SSA_NAME_VAR (target);
  if (SSA_NAME_IS_DEFAULT_DEF (target))
    {
      /* assuming TREE_CODE (var) == PARM_DECL */
      index = ipa_get_param_decl_index (info, var);
      if (index >= 0)
	ipa_note_param_call (info, index, stmt);
      return;
    }

  /* Now we need to try to match the complex pattern of calling a member
     pointer. */

  if (!POINTER_TYPE_P (TREE_TYPE (target))
      || TREE_CODE (TREE_TYPE (TREE_TYPE (target))) != METHOD_TYPE)
    return;

  def = SSA_NAME_DEF_STMT (target);
  if (TREE_CODE (def) != PHI_NODE)
    return;

  if (PHI_NUM_ARGS (def) != 2)
    return;

  /* First, we need to check whether one of these is a load from a member
     pointer that is a parameter to this function. */
  n1 = PHI_ARG_DEF (def, 0);
  n2 = PHI_ARG_DEF (def, 1);
  if (SSA_NAME_IS_DEFAULT_DEF (n1) || SSA_NAME_IS_DEFAULT_DEF (n2))
    return;
  d1 = SSA_NAME_DEF_STMT (n1);
  d2 = SSA_NAME_DEF_STMT (n2);

  if ((rec = ipa_get_stmt_member_ptr_load_param (d1)))
    {
      if (ipa_get_stmt_member_ptr_load_param (d2))
	return;

      bb = bb_for_stmt (d1);
      virt_bb = bb_for_stmt (d2);
    }
  else if ((rec = ipa_get_stmt_member_ptr_load_param (d2)))
    {
      bb = bb_for_stmt (d2);
      virt_bb = bb_for_stmt (d1);
    }
  else
    return;

  /* Second, we need to check that the basic blocks are laid out in the way
     corresponding to the pattern. */

  join = bb_for_stmt (def);
  if (!single_pred_p (virt_bb) || !single_succ_p (virt_bb)
      || single_pred (virt_bb) != bb
      || single_succ (virt_bb) != join)
    return;

  /* Third, let's see that the branching is done depending on the least
     significant bit of the pfn. */

  branch = last_stmt (bb);
  if (TREE_CODE (branch) != COND_EXPR)
    return;

  cond = TREE_OPERAND (branch, 0);
  if (TREE_CODE (cond) != NE_EXPR
      || !integer_zerop (TREE_OPERAND (cond, 1)))
    return;
  cond = TREE_OPERAND (cond, 0);

  if (!ipa_is_ssa_with_stmt_def (cond))
    return;

  cond = SSA_NAME_DEF_STMT (cond);
  if (TREE_CODE (cond) != GIMPLE_MODIFY_STMT)
    return;
  cond = GIMPLE_STMT_OPERAND (cond, 1);
  if (TREE_CODE (cond) != BIT_AND_EXPR
      || !integer_onep (TREE_OPERAND (cond, 1)))
    return;
  cond = TREE_OPERAND (cond, 0);
  if (!ipa_is_ssa_with_stmt_def (cond))
    return;

  cond = SSA_NAME_DEF_STMT (cond);
  if (TREE_CODE (cond) != GIMPLE_MODIFY_STMT)
    return;
  cond = GIMPLE_STMT_OPERAND (cond, 1);

  if (TREE_CODE (cond) == NOP_EXPR)
    {
      cond = TREE_OPERAND (cond, 0);
      if (!ipa_is_ssa_with_stmt_def (cond))
	return;
      cond = SSA_NAME_DEF_STMT (cond);
      if (TREE_CODE (cond) != GIMPLE_MODIFY_STMT)
	return;
      cond = GIMPLE_STMT_OPERAND (cond, 1);
    }

  rec2 = ipa_get_member_ptr_load_param (cond);
  if (rec != rec2)
    return;

  index = ipa_get_param_decl_index (info, rec);
  if (index >= 0 && !ipa_is_ith_param_modified (info, index))
    ipa_note_param_call (info, index, stmt);

  return;
}

/* Analyze the statement STMT with respect to formal parameters (described in
   INFO) and their uses.  Currently it only checks whether formal parameters
   are called.  */
static void
ipa_analyze_stmt_uses (struct ipa_node_params *info, tree stmt)
{
  tree call = get_call_expr_in (stmt);

  if (call)
    ipa_analyze_call_uses (info, call, stmt);
}

/* Scan the function body of NODE and inspect the uses of formal parameters.
   Store the findings in various structures of the associated ipa_node_params
   structure, such as parameter flags, notes etc.  */
void
ipa_analyze_params_uses (struct cgraph_node *node)
{
  tree decl = node->decl;
  basic_block bb;
  struct function *func;
  block_stmt_iterator bsi;
  struct ipa_node_params *info = IPA_NODE_REF (node);

  if (ipa_get_param_count (info) == 0 || info->uses_analysis_done
      || !DECL_SAVED_TREE (decl))
    return;
  if (!info->param_flags)
    info->param_flags = XCNEWVEC (struct ipa_param_flags,
				  ipa_get_param_count (info));

  func = DECL_STRUCT_FUNCTION (decl);
  FOR_EACH_BB_FN (bb, func)
    {
      for (bsi = bsi_start (bb); !bsi_end_p (bsi); bsi_next (&bsi))
	{
	  tree stmt = bsi_stmt (bsi);
	  ipa_analyze_stmt_uses (info, stmt);
	}
    }

  info->uses_analysis_done = 1;
}

/* Update the jump functions assocated with call graph edge E when the call
   graph edge CS is being inlined, assuming that E->caller is already (possibly
   indirectly) inlined into CS->callee and that E has not been inlined.  */
static void
update_jump_functions_after_inlining (struct cgraph_edge *cs,
				      struct cgraph_edge *e)
{
  struct ipa_edge_args *top = IPA_EDGE_REF (cs);
  struct ipa_edge_args *args = IPA_EDGE_REF (e);
  int count = ipa_get_cs_argument_count (args);
  int i;

  for (i = 0; i < count; i++)
    {
      struct ipa_jump_func *src, *dst = ipa_get_ith_jump_func (args, i);

      if (dst->type != IPA_PASS_THROUGH)
	continue;

      /* We must check range due to calls with variable number of arguments:  */
      if (dst->value.formal_id >= (unsigned) ipa_get_cs_argument_count (top))
	{
	  dst->type = IPA_BOTTOM;
	  continue;
	}

      src = ipa_get_ith_jump_func (top, dst->value.formal_id);
      *dst = *src;
    }
}

/* Print out a debug message to file F that we have discovered that an indirect
   call descibed by NT is in fact a call of a known constant function descibed
   by JFUNC.  NODE is the node where the call is.  */
static void
print_edge_addition_message (FILE *f, struct ipa_param_call_note *nt,
			     struct ipa_jump_func *jfunc,
			     struct cgraph_node *node)
{
  fprintf (f, "ipa-prop: Discovered an indirect call to a known target (");
  if (jfunc->type == IPA_CONST_MEMBER_PTR)
    {
      print_node_brief (f, "", jfunc->value.member_cst.pfn, 0);
      print_node_brief (f, ", ", jfunc->value.member_cst.delta, 0);
    }
  else
    print_node_brief(f, "", jfunc->value.constant, 0);

  fprintf (f, ") in %s: ", cgraph_node_name (node));
  print_generic_stmt (f, nt->stmt, 2);
}

/* Update the param called notes associated with NODE when CS is being inlined,
   assuming NODE is (potentially indirectly) inlined into CS->callee.
   Moreover, if the callee is discovered to be constant, create a new cgraph
   edge for it.  Newly discovered indirect edges will be added to NEW_EDGES,
   unless it is NULL.  */
static void
update_call_notes_after_inlining (struct cgraph_edge *cs,
				  struct cgraph_node *node,
				  VEC (cgraph_edge_p, heap) *new_edges)
{
  struct ipa_node_params *info = IPA_NODE_REF (node);
  struct ipa_edge_args *top = IPA_EDGE_REF (cs);
  struct ipa_param_call_note *nt;

  for (nt = info->param_calls; nt; nt = nt->next)
    {
      struct ipa_jump_func *jfunc;

      if (nt->processed)
	continue;

      /* We must check range due to calls with variable number of arguments:  */
      if (nt->formal_id >= (unsigned) ipa_get_cs_argument_count (top))
	{
	  nt->processed = true;
	  continue;
	}

      jfunc = ipa_get_ith_jump_func (top, nt->formal_id);
      if (jfunc->type == IPA_PASS_THROUGH)
	nt->formal_id = jfunc->value.formal_id;
      else if (jfunc->type == IPA_CONST || jfunc->type == IPA_CONST_MEMBER_PTR)
	{
	  struct cgraph_node *callee;
	  struct cgraph_edge *new_indirect_edge;
	  tree decl;

	  nt->processed = true;
	  if (jfunc->type == IPA_CONST_MEMBER_PTR)
	    decl = jfunc->value.member_cst.pfn;
	  else
	    decl = jfunc->value.constant;

	  if (TREE_CODE (decl) != FUNCTION_DECL)
	    continue;
	  callee = cgraph_node (decl);
	  if (!callee || !callee->local.inlinable)
	    continue;

	  if (dump_file)
	    print_edge_addition_message (dump_file, nt, jfunc, node);

	  new_indirect_edge = cgraph_create_edge (node, callee, nt->stmt,
						  nt->count, nt->frequency,
						  nt->loop_nest);
	  new_indirect_edge->indirect_call = 1;
	  ipa_check_create_edge_args ();
	  if (new_edges)
	    VEC_safe_push (cgraph_edge_p, heap, new_edges, new_indirect_edge);
	}
    }
}

/* Recursively traverse subtree of NODE (including node) made of inlined
   cgraph_edges when CS has been inlined and invoke
   update_call_notes_after_inlining on all nodes and
   update_jump_functions_after_inlining on all non-inlined edges that lead out
   of this subtree.  Newly discovered indirect edges will be added to
   NEW_EDGES, unless it is NULL.  */
static void
propagate_info_to_inlined_callees (struct cgraph_edge *cs,
				   struct cgraph_node *node,
				   VEC (cgraph_edge_p, heap) *new_edges)
{
  struct cgraph_edge *e;

  update_call_notes_after_inlining (cs, node, new_edges);

  for (e = node->callees; e; e = e->next_callee)
    if (!e->inline_failed)
      propagate_info_to_inlined_callees (cs, e->callee, new_edges);
    else
      update_jump_functions_after_inlining (cs, e);
}

/* Update jump functions and call note functions on inlining the call site CS.
   CS is expected to lead to a node already cloned by
   cgraph_clone_inline_nodes.  Newly discovered indirect edges will be added to
   NEW_EDGES, unless it is NULL.  */
void
ipa_propagate_indirect_call_infos (struct cgraph_edge *cs,
				   VEC (cgraph_edge_p, heap) *new_edges)
{
  propagate_info_to_inlined_callees (cs, cs->callee, new_edges);
}

/* Frees all dynamically allocated structures that the argument info points
   to.  */
void
ipa_free_edge_args_substructures (struct ipa_edge_args *args)
{
  if (args->jump_functions)
    free (args->jump_functions);

  memset (args, 0, sizeof (*args));
}

/* Free all ipa_edge structures.  */
void
ipa_free_all_edge_args (void)
{
  int i;
  struct ipa_edge_args *args;

  for (i = 0;
       VEC_iterate (ipa_edge_args_t, ipa_edge_args_vector, i, args);
       i++)
    ipa_free_edge_args_substructures (args);

  VEC_free (ipa_edge_args_t, heap, ipa_edge_args_vector);
  ipa_edge_args_vector = NULL;
}

/* Frees all dynamically allocated structures that the param info points
   to.  */
void
ipa_free_node_params_substructures (struct ipa_node_params *info)
{
  if (info->ipcp_lattices)
    free (info->ipcp_lattices);
  if (info->param_decls)
    free (info->param_decls);
  if (info->param_flags)
    free (info->param_flags);

  while (info->param_calls)
    {
      struct ipa_param_call_note *note = info->param_calls;
      info->param_calls = note->next;
      free (note);
    }

  memset (info, 0, sizeof (*info));
}

/* Free all ipa_node_params structures.  */
void
ipa_free_all_node_params (void)
{
  int i;
  struct ipa_node_params *info;

  for (i = 0;
       VEC_iterate (ipa_node_params_t, ipa_node_params_vector, i, info);
       i++)
    ipa_free_node_params_substructures (info);

  VEC_free (ipa_node_params_t, heap, ipa_node_params_vector);
  ipa_node_params_vector = NULL;
}

/* Hook that is called by cgraph.c when an edge is removed.  */
static void
ipa_edge_removal_hook (struct cgraph_edge *cs,
		       void *data __attribute__ ((unused)))
{
  ipa_free_edge_args_substructures (IPA_EDGE_REF (cs));
}

/* Hook that is called by cgraph.c when a node is removed.  */
static void
ipa_node_removal_hook (struct cgraph_node *node,
		       void *data __attribute__ ((unused)))
{
  ipa_free_node_params_substructures (IPA_NODE_REF (node));
}

/* Helper function to duplicate an array of size N that is at SRC and store a
   pointer to it to DST.  Nothing is done if SRC is NULL.  */
static void *
duplicate_array (void *src, size_t n)
{
  void *p;

  if (!src)
    return NULL;

  p = xcalloc (1, n);
  memcpy (p, src, n);
  return p;
}

/* Hook that is called by cgraph.c when a node is duplicated.  */
static void
ipa_edge_duplication_hook (struct cgraph_edge *src, struct cgraph_edge *dst,
			   void *data)
{
  struct ipa_edge_args *old_args, *new_args;
  int arg_count;

  ipa_check_create_edge_args ();

  old_args = IPA_EDGE_REF (src);
  new_args = IPA_EDGE_REF (dst);

  arg_count = ipa_get_cs_argument_count (old_args);
  ipa_set_cs_argument_count (new_args, arg_count);
  new_args->jump_functions = (struct ipa_jump_func *)
    duplicate_array (old_args->jump_functions,
		     sizeof (struct ipa_jump_func) * arg_count);
  data = data; 			/* Suppressing compiler warning.  */
}

/* Hook that is called by cgraph.c when a node is duplicated.  */
static void
ipa_node_duplication_hook (struct cgraph_node *src, struct cgraph_node *dst,
			   void *data)
{
  struct ipa_node_params *old_info, *new_info;
  struct ipa_param_call_note *note;
  int param_count;

  ipa_check_create_node_params ();
  old_info = IPA_NODE_REF (src);
  new_info = IPA_NODE_REF (dst);
  param_count = ipa_get_param_count (old_info);

  ipa_set_param_count (new_info, param_count);
  new_info->ipcp_lattices = (struct ipcp_lattice *)
    duplicate_array (old_info->ipcp_lattices,
		     sizeof (struct ipcp_lattice) * param_count);
  new_info->param_decls = (tree *)
    duplicate_array (old_info->param_decls, sizeof (tree) * param_count);
  new_info->param_flags = (struct ipa_param_flags *)
    duplicate_array (old_info->param_flags,
		     sizeof (struct ipa_param_flags) * param_count);

  new_info->ipcp_orig_node = old_info->ipcp_orig_node;
  new_info->count_scale = old_info->count_scale;

  for (note = old_info->param_calls; note; note = note->next)
    {
      struct ipa_param_call_note *nn;

      nn = (struct ipa_param_call_note *)
	xcalloc (1, sizeof (struct ipa_param_call_note));
      memcpy (nn, note, sizeof (struct ipa_param_call_note));
      nn->next = new_info->param_calls;
      new_info->param_calls = nn;
    }

  data = data; 			/* Suppressing compiler warning.  */
}

/* Register our cgraph hooks if they are not already there.  */
void
ipa_register_cgraph_hooks (void)
{
  if (!edge_removal_hook_holder)
    edge_removal_hook_holder =
      cgraph_add_edge_removal_hook (&ipa_edge_removal_hook, NULL);
  if (!node_removal_hook_holder)
    node_removal_hook_holder =
      cgraph_add_node_removal_hook (&ipa_node_removal_hook, NULL);
  if (!edge_duplication_hook_holder)
    edge_duplication_hook_holder =
      cgraph_add_edge_duplication_hook (&ipa_edge_duplication_hook, NULL);
  if (!node_duplication_hook_holder)
    node_duplication_hook_holder =
      cgraph_add_node_duplication_hook (&ipa_node_duplication_hook, NULL);
}

/* Unregister our cgraph hooks if they are not already there.  */
static void
ipa_unregister_cgraph_hooks (void)
{
  cgraph_remove_edge_removal_hook (edge_removal_hook_holder);
  edge_removal_hook_holder = NULL;
  cgraph_remove_node_removal_hook (node_removal_hook_holder);
  node_removal_hook_holder = NULL;
  cgraph_remove_edge_duplication_hook (edge_duplication_hook_holder);
  edge_duplication_hook_holder = NULL;
  cgraph_remove_node_duplication_hook (node_duplication_hook_holder);
  node_duplication_hook_holder = NULL;
}

/* Free all ipa_node_params and all ipa_edge_args structures if they are no
   longer needed after ipa-cp.  */
void
free_all_ipa_structures_after_ipa_cp (void)
{
  if (!flag_indirect_inlining)
    {
      ipa_free_all_edge_args ();
      ipa_free_all_node_params ();
      ipa_unregister_cgraph_hooks ();
    }
}

/* Free all ipa_node_params and all ipa_edge_args structures if they are no
   longer needed after indirect inlining.  */
void
free_all_ipa_structures_after_iinln (void)
{
  ipa_free_all_edge_args ();
  ipa_free_all_node_params ();
  ipa_unregister_cgraph_hooks ();
}

/* Print ipa_tree_map data structures of all functions in the
   callgraph to F.  */
void
ipa_print_all_tree_maps (FILE * f)
{
  int i, count;
  tree temp;
  struct cgraph_node *node;

  fprintf (f, "\nPARAM TREE MAP PRINT\n");
  for (node = cgraph_nodes; node; node = node->next)
    {
      struct ipa_node_params *info;

      if (!node->analyzed)
	continue;
      info = IPA_NODE_REF (node);
      fprintf (f, "function  %s Trees :: \n", cgraph_node_name (node));
      count = ipa_get_param_count (info);
      for (i = 0; i < count; i++)
	{
	  temp = ipa_get_ith_param (info, i);
	  if (TREE_CODE (temp) == PARM_DECL)
	    fprintf (f, "  param [%d] : %s\n", i,
		     (*lang_hooks.decl_printable_name) (temp, 2));
	}

    }
}

/* Print param_flags data structures of the NODE to F.  */
void
ipa_print_node_param_flags (FILE * f, struct cgraph_node *node)
{
  int i, count;
  struct ipa_node_params *info;

  if (!node->analyzed)
    return;
  info = IPA_NODE_REF (node);
  fprintf (f, "PARAM FLAGS of function  %s: \n", cgraph_node_name (node));
  count = ipa_get_param_count (info);
  for (i = 0; i < count; i++)
    {
      fprintf (f, "   param %d flags:", i);
      if (ipa_is_ith_param_modified (info, i))
	fprintf (f, " modified");
      if (ipa_is_ith_param_called (info, i))
	fprintf (f, " called");
      fprintf (f, "\n");
    }
}

/* Print param_flags data structures of all functions in the
   callgraph to F.  */
void
ipa_print_all_param_flags (FILE * f)
{
  struct cgraph_node *node;

  fprintf (f, "\nIPA PARAM FLAGS DUMP\n");
  for (node = cgraph_nodes; node; node = node->next)
    ipa_print_node_param_flags (f, node);
}
