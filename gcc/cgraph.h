/* Callgraph handling code.
   Copyright (C) 2003 Free Software Foundation, Inc.
   Contributed by Jan Hubicka

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

#ifndef GCC_CGRAPH_H
#define GCC_CGRAPH_H

/* Information about the function collected locally.
   Available after function is lowered  */

struct cgraph_local_info GTY(())
{
  /* Set when function function is visible in current compilation unit only
     and it's address is never taken.  */
  bool local;
  /* Set when function is small enough to be inlinable many times.  */
  bool inline_many;
  /* Set when function can be inlined once (false only for functions calling
     alloca, using varargs and so on).  */
  bool can_inline_once;
  /* Set once it has been finalized so we consider it to be output.  */
  bool finalized;
};

/* Information about the function that needs to be computed globally
   once compilation is finished.  Available only with -funit-at-time.  */

struct cgraph_global_info GTY(())
{
  /* Set when the function will be inlined exactly once.  */
  bool inline_once;
};

/* Information about the function that is propagated by the RTL backend.
   Available only for functions that has been already assembled.  */

struct cgraph_rtl_info GTY(())
{
   bool const_function;
   bool pure_function;
   int preferred_incoming_stack_boundary;
};


/* The cgraph data strutcture.
   Each function decl has assigned cgraph_node listing callees and callers.  */

struct cgraph_node GTY(())
{
  tree decl;
  struct cgraph_edge *callees;
  struct cgraph_edge *callers;
  struct cgraph_node *next;
  struct cgraph_node *previous;
  /* For nested functions points to function the node is nested in.  */
  struct cgraph_node *origin;
  /* Points to first nested function, if any.  */
  struct cgraph_node *nested;
  /* Pointer to the next function with same origin, if any.  */
  struct cgraph_node *next_nested;
  /* Pointer to the next function in cgraph_nodes_queue.  */
  struct cgraph_node *next_needed;
  PTR GTY ((skip (""))) aux;

  /* Set when function must be output - it is externally visible
     or it's address is taken.  */
  bool needed;
  /* Set when function is reachable by call from other function
     that is either reachable or needed.  */
  bool reachable;
  /* Set when the frontend has been asked to lower representation of this
     function into trees.  Callees lists are not available when lowered
     is not set.  */
  bool lowered;
  /* Set when function is scheduled to be assembled.  */
  bool output;
  struct cgraph_local_info local;
  struct cgraph_global_info global;
  struct cgraph_rtl_info rtl;
};

struct cgraph_edge GTY(())
{
  struct cgraph_node *caller;
  struct cgraph_node *callee;
  struct cgraph_edge *next_caller;
  struct cgraph_edge *next_callee;
};

/* The cgraph_varpool data strutcture.
   Each static variable decl has assigned cgraph_varpool_node.  */

struct cgraph_varpool_node GTY(())
{
  tree decl;
  /* Pointer to the next function in cgraph_varpool_nodes_queue.  */
  struct cgraph_varpool_node *next_needed;

  /* Set when function must be output - it is externally visible
     or it's address is taken.  */
  bool needed;
  /* Set once it has been finalized so we consider it to be output.  */
  bool finalized;
  /* Set when function is scheduled to be assembled.  */
  bool output;
};

extern GTY(()) struct cgraph_node *cgraph_nodes;
extern GTY(()) int cgraph_n_nodes;
extern bool cgraph_global_info_ready;
extern GTY(()) struct cgraph_node *cgraph_nodes_queue;
extern FILE *cgraph_dump_file;

extern GTY(()) int cgraph_varpool_n_nodes;
extern GTY(()) struct cgraph_varpool_node *cgraph_varpool_nodes_queue;


/* In cgraph.c  */
void dump_cgraph			PARAMS ((FILE *));
void cgraph_remove_call			PARAMS ((tree, tree));
void cgraph_remove_node			PARAMS ((struct cgraph_node *));
struct cgraph_edge *cgraph_record_call	PARAMS ((tree, tree));
struct cgraph_node *cgraph_node		PARAMS ((tree decl));
struct cgraph_node *cgraph_node_for_identifier	PARAMS ((tree id));
bool cgraph_calls_p			PARAMS ((tree, tree));
struct cgraph_local_info *cgraph_local_info PARAMS ((tree));
struct cgraph_global_info *cgraph_global_info PARAMS ((tree));
struct cgraph_rtl_info *cgraph_rtl_info PARAMS ((tree));
const char * cgraph_node_name PARAMS ((struct cgraph_node *));

struct cgraph_varpool_node *cgraph_varpool_node (tree decl);
struct cgraph_varpool_node *cgraph_varpool_node_for_identifier (tree id);
void cgraph_varpool_mark_needed_node (struct cgraph_varpool_node *);
void cgraph_varpool_finalize_decl (tree);
bool cgraph_varpool_assemble_pending_decls (void);

/* In cgraphunit.c  */
void cgraph_finalize_function		PARAMS ((tree, tree));
void cgraph_finalize_compilation_unit	PARAMS ((void));
void cgraph_create_edges		PARAMS ((tree, tree));
void cgraph_optimize			PARAMS ((void));
void cgraph_mark_needed_node		PARAMS ((struct cgraph_node *, int));

#endif  /* GCC_CGRAPH_H  */
