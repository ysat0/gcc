/* Pretty formatting of GENERIC trees in C syntax.
   Copyright (C) 2001, 2002, 2003 Free Software Foundation, Inc.
   Adapted from c-pretty-print.c by Diego Novillo <dnovillo@redhat.com>

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
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "errors.h"
#include "tree.h"
#include "diagnostic.h"
#include "real.h"
#include "hashtab.h"
#include "tree-flow.h"
#include "langhooks.h"
#include "tree-iterator.h"

/* Local functions, macros and variables.  */
static int op_prio (tree);
static const char *op_symbol (tree);
static void pretty_print_string (pretty_printer *, const char*);
static void print_call_name (pretty_printer *, tree);
static void newline_and_indent (pretty_printer *, int);
static void maybe_init_pretty_print (FILE *);
static void print_declaration (pretty_printer *, tree, int, int);
static void print_struct_decl (pretty_printer *, tree, int, int);
static void do_niy (pretty_printer *, tree);
static void dump_vops (pretty_printer *, tree, int, int);
static void dump_generic_bb_buff (pretty_printer *, basic_block, int, int);

#define INDENT(SPACE) do { \
  int i; for (i = 0; i<SPACE; i++) pp_space (buffer); } while (0)

#define NIY do_niy(buffer,node)

#define PRINT_FUNCTION_NAME(NODE)  pp_printf             \
  (buffer, "%s", TREE_CODE (NODE) == NOP_EXPR ?              \
   lang_hooks.decl_printable_name (TREE_OPERAND (NODE, 0), 1) : \
   lang_hooks.decl_printable_name (NODE, 1))

#define MASK_POINTER(P)	((unsigned)((unsigned long)(P) & 0xffff))

static pretty_printer buffer;
static int initialized = 0;
static bool dumping_stmts;

/* Try to print something for an unknown tree code.  */

static void
do_niy (pretty_printer *buffer, tree node)
{
  int i, len;

  pp_string (buffer, "<<< Unknown tree: ");
  pp_string (buffer, tree_code_name[(int) TREE_CODE (node)]);

  if (IS_EXPR_CODE_CLASS (TREE_CODE_CLASS (TREE_CODE (node))))
    {
      len = first_rtl_op (TREE_CODE (node));
      for (i = 0; i < len; ++i)
	{
	  newline_and_indent (buffer, 2);
	  dump_generic_node (buffer, TREE_OPERAND (node, i), 2, 0, false);
	}
    }

  pp_string (buffer, " >>>\n");
}

void
debug_generic_expr (tree t)
{
  print_generic_expr (stderr, t, TDF_VOPS|TDF_UID);
  fprintf (stderr, "\n");
}

void
debug_generic_stmt (tree t)
{
  print_generic_stmt (stderr, t, TDF_VOPS|TDF_UID);
  fprintf (stderr, "\n");
}

/* Prints declaration DECL to the FILE with details specified by FLAGS.  */
void
print_generic_decl (FILE *file, tree decl, int flags)
{
  maybe_init_pretty_print (file);
  dumping_stmts = true;
  print_declaration (&buffer, decl, 2, flags);
  pp_write_text_to_stream (&buffer);
}

/* Print tree T, and its successors, on file FILE.  FLAGS specifies details
   to show in the dump.  See TDF_* in tree.h.  */

void
print_generic_stmt (FILE *file, tree t, int flags)
{
  maybe_init_pretty_print (file);
  dumping_stmts = true;
  dump_generic_node (&buffer, t, 0, flags, true);
  pp_flush (&buffer);
}

/* Print tree T, and its successors, on file FILE.  FLAGS specifies details
   to show in the dump.  See TDF_* in tree.h.  The output is indented by
   INDENT spaces.  */

void
print_generic_stmt_indented (FILE *file, tree t, int flags, int indent)
{
  int i;

  maybe_init_pretty_print (file);
  dumping_stmts = true;

  for (i = 0; i < indent; i++)
    pp_space (&buffer);
  dump_generic_node (&buffer, t, indent, flags, true);
  pp_flush (&buffer);
}

/* Print a single expression T on file FILE.  FLAGS specifies details to show
   in the dump.  See TDF_* in tree.h.  */

void
print_generic_expr (FILE *file, tree t, int flags)
{
  maybe_init_pretty_print (file);
  dumping_stmts = false;
  dump_generic_node (&buffer, t, 0, flags, false);
}

/* Dump the name of a _DECL node and its DECL_UID if TDF_UID is set
   in FLAGS.  */

static void
dump_decl_name (pretty_printer *buffer, tree node, int flags)
{
  if (DECL_NAME (node))
    pp_tree_identifier (buffer, DECL_NAME (node));

  if ((flags & TDF_UID)
      || DECL_NAME (node) == NULL_TREE)
    {
      if (TREE_CODE (node) == LABEL_DECL
	  && LABEL_DECL_UID (node) != -1)
	pp_printf (buffer, "<L" HOST_WIDE_INT_PRINT_DEC ">",
		   LABEL_DECL_UID (node));
      else
	pp_printf (buffer, "<D%u>", DECL_UID (node));
    }
}

/* Dump the node NODE on the pretty_printer BUFFER, SPC spaces of indent.
   FLAGS specifies details to show in the dump (see TDF_* in tree.h).  If
   IS_STMT is true, the object printed is considered to be a statement
   and it is terminated by ';' if appropriate.  */

int
dump_generic_node (pretty_printer *buffer, tree node, int spc, int flags,
		   bool is_stmt)
{
  tree type;
  tree op0, op1;
  const char* str;
  bool is_expr;

  if (node == NULL_TREE)
    return spc;

  is_expr = EXPR_P (node);

  if (TREE_CODE (node) != ERROR_MARK
      && is_gimple_stmt (node)
      && (flags & TDF_VOPS)
      && stmt_ann (node))
    dump_vops (buffer, node, spc, flags);

  if (dumping_stmts
      && (flags & TDF_LINENO)
      && EXPR_HAS_LOCATION (node))
    {
      pp_character (buffer, '[');
      if (EXPR_FILENAME (node))
	{
	  pp_string (buffer, EXPR_FILENAME (node));
	  pp_string (buffer, " : ");
	}
      pp_decimal_int (buffer, EXPR_LINENO (node));
      pp_string (buffer, "] ");
    }

  switch (TREE_CODE (node))
    {
    case ERROR_MARK:
      pp_string (buffer, "<<< error >>>");
      break;

    case IDENTIFIER_NODE:
      pp_tree_identifier (buffer, node);
      break;

    case TREE_LIST:
      while (node && node != error_mark_node)
	{
	  if (TREE_PURPOSE (node))
	    {
	      dump_generic_node (buffer, TREE_PURPOSE (node), spc, flags, false);
	      pp_space (buffer);
	    }
	  dump_generic_node (buffer, TREE_VALUE (node), spc, flags, false);
	  node = TREE_CHAIN (node);
	  if (node && TREE_CODE (node) == TREE_LIST)
	    {
	      pp_character (buffer, ',');
	      pp_space (buffer);
	    }
	}
      break;

    case TREE_VEC:
      dump_generic_node (buffer, BINFO_TYPE (node), spc, flags, false);
      break;

    case BLOCK:
      NIY;
      break;

    case VOID_TYPE:
    case INTEGER_TYPE:
    case REAL_TYPE:
    case COMPLEX_TYPE:
    case VECTOR_TYPE:
    case ENUMERAL_TYPE:
    case BOOLEAN_TYPE:
    case CHAR_TYPE:
      {
	unsigned int quals = TYPE_QUALS (node);
	char class;

	if (quals & TYPE_QUAL_CONST)
	  pp_string (buffer, "const ");
	else if (quals & TYPE_QUAL_VOLATILE)
	  pp_string (buffer, "volatile ");
	else if (quals & TYPE_QUAL_RESTRICT)
	  pp_string (buffer, "restrict ");

	class = TREE_CODE_CLASS (TREE_CODE (node));

	if (class == 'd')
	  {
	    if (DECL_NAME (node))
	      dump_decl_name (buffer, node, flags);
	    else
              pp_string (buffer, "<unnamed type decl>");
	  }
	else if (class == 't')
	  {
	    if (TYPE_NAME (node))
	      {
		if (TREE_CODE (TYPE_NAME (node)) == IDENTIFIER_NODE)
		  pp_tree_identifier (buffer, TYPE_NAME (node));
		else if (TREE_CODE (TYPE_NAME (node)) == TYPE_DECL
			 && DECL_NAME (TYPE_NAME (node)))
		  dump_decl_name (buffer, TYPE_NAME (node), flags);
		else
                  pp_string (buffer, "<unnamed type>");
	      }
	    else
              pp_string (buffer, "<unnamed type>");
	  }
	break;
      }

    case POINTER_TYPE:
    case REFERENCE_TYPE:
      str = (TREE_CODE (node) == POINTER_TYPE ? "*" : "&");

      if (TREE_CODE (TREE_TYPE (node)) == FUNCTION_TYPE)
        {
	  tree fnode = TREE_TYPE (node);
	  dump_generic_node (buffer, TREE_TYPE (fnode), spc, flags, false);
	  pp_space (buffer);
	  pp_character (buffer, '(');
	  pp_string (buffer, str);
	  if (TYPE_NAME (node) && DECL_NAME (TYPE_NAME (node)))
	    dump_decl_name (buffer, TYPE_NAME (node), flags);
	  else
	    pp_printf (buffer, "<T%x>", TYPE_UID (node));

	  pp_character (buffer, ')');
          pp_space (buffer);
	  pp_character (buffer, '(');
	  /* Print the argument types.  The last element in the list is a
	     VOID_TYPE.  The following avoid to print the last element.  */
	  {
	    tree tmp = TYPE_ARG_TYPES (fnode);
	    while (tmp && TREE_CHAIN (tmp) && tmp != error_mark_node)
	      {
		dump_generic_node (buffer, TREE_VALUE (tmp), spc, flags, false);
		tmp = TREE_CHAIN (tmp);
		if (TREE_CHAIN (tmp) && TREE_CODE (TREE_CHAIN (tmp)) == TREE_LIST)
		  {
		    pp_character (buffer, ',');
		    pp_space (buffer);
		  }
	      }
	  }
	  pp_character (buffer, ')');
	}
      else
        {
	  unsigned int quals = TYPE_QUALS (node);

          dump_generic_node (buffer, TREE_TYPE (node), spc, flags, false);
	  pp_space (buffer);
	  pp_string (buffer, str);

	  if (quals & TYPE_QUAL_CONST)
	    pp_string (buffer, " const");
	  else if (quals & TYPE_QUAL_VOLATILE)
	    pp_string (buffer,  "volatile");
	  else if (quals & TYPE_QUAL_RESTRICT)
	    pp_string (buffer, " restrict");
	}
      break;

    case OFFSET_TYPE:
      NIY;
      break;

    case METHOD_TYPE:
      dump_decl_name (buffer, TYPE_NAME (TYPE_METHOD_BASETYPE (node)), flags);
      pp_string (buffer, "::");
      break;

    case FILE_TYPE:
      NIY;
      break;

    case ARRAY_TYPE:
      {
	tree tmp;

	/* Print the array type.  */
	dump_generic_node (buffer, TREE_TYPE (node), spc, flags, false);

	/* Print the dimensions.  */
	tmp = node;
	while (tmp && TREE_CODE (tmp) == ARRAY_TYPE)
	  {
	    pp_character (buffer, '[');
	    if (TYPE_SIZE (tmp))
	      {
		tree size = TYPE_SIZE (tmp);
		if (TREE_CODE (size) == INTEGER_CST)
		  pp_wide_integer (buffer,
				  TREE_INT_CST_LOW (TYPE_SIZE (tmp)) /
				  TREE_INT_CST_LOW (TYPE_SIZE (TREE_TYPE (tmp))));
		else if (TREE_CODE (size) == MULT_EXPR)
		  dump_generic_node (buffer, TREE_OPERAND (size, 0), spc, flags, false);
		/* else punt.  */
	      }
	    pp_character (buffer, ']');
	    tmp = TREE_TYPE (tmp);
	  }
	break;
      }

    case SET_TYPE:
      NIY;
      break;

    case RECORD_TYPE:
    case UNION_TYPE:
      /* Print the name of the structure.  */
      if (TREE_CODE (node) == RECORD_TYPE)
	pp_string (buffer, "struct ");
      else if (TREE_CODE (node) == UNION_TYPE)
	pp_string (buffer, "union ");

      if (TYPE_NAME (node))
	dump_generic_node (buffer, TYPE_NAME (node), spc, flags, false);
      else
	print_struct_decl (buffer, node, spc, flags);
      break;

    case QUAL_UNION_TYPE:
      NIY;
      break;


    case LANG_TYPE:
      NIY;
      break;

    case INTEGER_CST:
      if (TREE_CODE (TREE_TYPE (node)) == POINTER_TYPE)
	{
	  /* In the case of a pointer, one may want to divide by the
	     size of the pointed-to type.  Unfortunately, this not
	     straightforward.  The C front-end maps expressions

	     (int *) 5
	     int *p; (p + 5)

	     in such a way that the two INTEGER_CST nodes for "5" have
	     different values but identical types.  In the latter
	     case, the 5 is multiplied by sizeof (int) in c-common.c
	     (pointer_int_sum) to convert it to a byte address, and
	     yet the type of the node is left unchanged.  Argh.  What
	     is consistent though is that the number value corresponds
	     to bytes (UNITS) offset.

             NB: Neither of the following divisors can be trivially
             used to recover the original literal:

             TREE_INT_CST_LOW (TYPE_SIZE_UNIT (TREE_TYPE (node)))
	     TYPE_PRECISION (TREE_TYPE (TREE_TYPE (node)))  */
	  pp_wide_integer (buffer, TREE_INT_CST_LOW (node));
	  pp_string (buffer, "B"); /* pseudo-unit */
	}
      else if (! host_integerp (node, 0))
	{
	  tree val = node;

	  if (tree_int_cst_sgn (val) < 0)
	    {
	      pp_character (buffer, '-');
	      val = build_int_2 (-TREE_INT_CST_LOW (val),
				 ~TREE_INT_CST_HIGH (val)
				 + !TREE_INT_CST_LOW (val));
	    }
	  /* Would "%x%0*x" or "%x%*0x" get zero-padding on all
	     systems?  */
	  {
	    static char format[10]; /* "%x%09999x\0" */
	    if (!format[0])
	      sprintf (format, "%%x%%0%dx", HOST_BITS_PER_INT / 4);
	    sprintf (pp_buffer (buffer)->digit_buffer, format,
		     TREE_INT_CST_HIGH (val),
		     TREE_INT_CST_LOW (val));
	    pp_string (buffer, pp_buffer (buffer)->digit_buffer);
	  }
	}
      else
	pp_wide_integer (buffer, TREE_INT_CST_LOW (node));
      break;

    case REAL_CST:
      /* Code copied from print_node.  */
      {
	REAL_VALUE_TYPE d;
	if (TREE_OVERFLOW (node))
	  pp_string (buffer, " overflow");

#if !defined(REAL_IS_NOT_DOUBLE) || defined(REAL_ARITHMETIC)
	d = TREE_REAL_CST (node);
	if (REAL_VALUE_ISINF (d))
	  pp_string (buffer, " Inf");
	else if (REAL_VALUE_ISNAN (d))
	  pp_string (buffer, " Nan");
	else
	  {
	    char string[100];
	    real_to_decimal (string, &d, sizeof (string), 0, 1);
	    pp_string (buffer, string);
	  }
#else
	{
	  HOST_WIDE_INT i;
	  unsigned char *p = (unsigned char *) &TREE_REAL_CST (node);
	  pp_string (buffer, "0x");
	  for (i = 0; i < sizeof TREE_REAL_CST (node); i++)
	    output_formatted_integer (buffer, "%02x", *p++);
	}
#endif
	break;
      }

    case COMPLEX_CST:
      pp_string (buffer, "__complex__ (");
      dump_generic_node (buffer, TREE_REALPART (node), spc, flags, false);
      pp_string (buffer, ", ");
      dump_generic_node (buffer, TREE_IMAGPART (node), spc, flags, false);
      pp_string (buffer, ")");
      break;

    case STRING_CST:
      pp_string (buffer, "\"");
      pretty_print_string (buffer, TREE_STRING_POINTER (node));
      pp_string (buffer, "\"");
      break;

    case VECTOR_CST:
      {
	tree elt;
	pp_string (buffer, "{ ");
	for (elt = TREE_VECTOR_CST_ELTS (node); elt; elt = TREE_CHAIN (elt))
	  {
	    dump_generic_node (buffer, TREE_VALUE (elt), spc, flags, false);
	    if (TREE_CHAIN (elt))
	      pp_string (buffer, ", ");
	  }
	pp_string (buffer, " }");
      }
      break;

    case FUNCTION_TYPE:
      break;

    case FUNCTION_DECL:
    case CONST_DECL:
      dump_decl_name (buffer, node, flags);
      break;

    case LABEL_DECL:
      if (DECL_NAME (node))
	dump_decl_name (buffer, node, flags);
      else if (LABEL_DECL_UID (node) != -1)
        pp_printf (buffer, "<L" HOST_WIDE_INT_PRINT_DEC ">",
		   LABEL_DECL_UID (node));
      else
        pp_printf (buffer, "<D%u>", DECL_UID (node));
      break;

    case TYPE_DECL:
      if (strcmp (DECL_SOURCE_FILE (node), "<built-in>") == 0)
	{
	  /* Don't print the declaration of built-in types.  */
	  break;
	}
      if (DECL_NAME (node))
	{
	  dump_decl_name (buffer, node, flags);
	}
      else
	{
	  if (TYPE_METHODS (TREE_TYPE (node)))
	    {
	      /* The type is a c++ class: all structures have at least
		 4 methods. */
	      pp_string (buffer, "class ");
	      dump_generic_node (buffer, TREE_TYPE (node), spc, flags, false);
	    }
	  else
	    {
	      pp_string (buffer, "struct ");
	      dump_generic_node (buffer, TREE_TYPE (node), spc, flags, false);
	      pp_character (buffer, ';');
	      pp_newline (buffer);
	    }
	}
      break;

    case VAR_DECL:
    case PARM_DECL:
    case FIELD_DECL:
    case NAMESPACE_DECL:
      dump_decl_name (buffer, node, flags);
      break;

    case RESULT_DECL:
      pp_string (buffer, "<retval>");
      break;

    case COMPONENT_REF:
      op0 = TREE_OPERAND (node, 0);
      str = ".";
      if (TREE_CODE (op0) == INDIRECT_REF)
	{
	  op0 = TREE_OPERAND (op0, 0);
	  str = "->";
	}
      if (op_prio (op0) < op_prio (node))
	pp_character (buffer, '(');
      dump_generic_node (buffer, op0, spc, flags, false);
      if (op_prio (op0) < op_prio (node))
	pp_character (buffer, ')');
      pp_string (buffer, str);
      dump_generic_node (buffer, TREE_OPERAND (node, 1), spc, flags, false);
      break;

    case BIT_FIELD_REF:
      pp_string (buffer, "BIT_FIELD_REF <");
      dump_generic_node (buffer, TREE_OPERAND (node, 0), spc, flags, false);
      pp_string (buffer, ", ");
      dump_generic_node (buffer, TREE_OPERAND (node, 1), spc, flags, false);
      pp_string (buffer, ", ");
      dump_generic_node (buffer, TREE_OPERAND (node, 2), spc, flags, false);
      pp_string (buffer, ">");
      break;

    case BUFFER_REF:
      NIY;
      break;

    case ARRAY_REF:
      op0 = TREE_OPERAND (node, 0);
      if (op_prio (op0) < op_prio (node))
	pp_character (buffer, '(');
      dump_generic_node (buffer, op0, spc, flags, false);
      if (op_prio (op0) < op_prio (node))
	pp_character (buffer, ')');
      pp_character (buffer, '[');
      dump_generic_node (buffer, TREE_OPERAND (node, 1), spc, flags, false);
      pp_character (buffer, ']');
      break;

    case ARRAY_RANGE_REF:
      NIY;
      break;

    case CONSTRUCTOR:
      {
	tree lnode;
	bool is_struct_init = FALSE;
	pp_character (buffer, '{');
	lnode = CONSTRUCTOR_ELTS (node);
	if (TREE_CODE (TREE_TYPE (node)) == RECORD_TYPE
	    || TREE_CODE (TREE_TYPE (node)) == UNION_TYPE)
	  is_struct_init = TRUE;
	while (lnode && lnode != error_mark_node)
	  {
	    tree val;
	    if (TREE_PURPOSE (lnode) && is_struct_init)
	      {
		pp_character (buffer, '.');
		dump_generic_node (buffer, TREE_PURPOSE (lnode), spc, flags, false);
		pp_string (buffer, "=");
	      }
	    val = TREE_VALUE (lnode);
	    if (val && TREE_CODE (val) == ADDR_EXPR)
	      if (TREE_CODE (TREE_OPERAND (val, 0)) == FUNCTION_DECL)
		val = TREE_OPERAND (val, 0);
	    if (val && TREE_CODE (val) == FUNCTION_DECL)
	      {
		dump_decl_name (buffer, val, flags);
	      }
	    else
	      {
		dump_generic_node (buffer, TREE_VALUE (lnode), spc, flags, false);
	      }
	    lnode = TREE_CHAIN (lnode);
	    if (lnode && TREE_CODE (lnode) == TREE_LIST)
	      {
		pp_character (buffer, ',');
		pp_space (buffer);
	      }
	  }
	pp_character (buffer, '}');
      }
      break;

    case COMPOUND_EXPR:
      {
	tree *tp;
	if (flags & TDF_SLIM)
	  {
	    pp_string (buffer, "<COMPOUND_EXPR>");
	    break;
	  }

	dump_generic_node (buffer, TREE_OPERAND (node, 0),
			   spc, flags, dumping_stmts);
	if (dumping_stmts)
	  newline_and_indent (buffer, spc);
	else
	  {
	    pp_character (buffer, ',');
	    pp_space (buffer);
	  }

	for (tp = &TREE_OPERAND (node, 1);
	     TREE_CODE (*tp) == COMPOUND_EXPR;
	     tp = &TREE_OPERAND (*tp, 1))
	  {
	    dump_generic_node (buffer, TREE_OPERAND (*tp, 0),
			       spc, flags, dumping_stmts);
	    if (dumping_stmts)
	      newline_and_indent (buffer, spc);
	    else
	      {
	        pp_character (buffer, ',');
	        pp_space (buffer);
	      }
	  }

	dump_generic_node (buffer, *tp, spc, flags, dumping_stmts);
      }
      break;

    case STATEMENT_LIST:
      {
	tree_stmt_iterator si;
	bool first = true;
  
	if ((flags & TDF_SLIM) || !dumping_stmts)
	  {
	    pp_string (buffer, "<STATEMENT_LIST>");
	    break;
	  }

	for (si = tsi_start (node); !tsi_end_p (si); tsi_next (&si))
	  {
	    if (!first)
	      newline_and_indent (buffer, spc);
	    else
	      first = false;
	    dump_generic_node (buffer, tsi_stmt (si), spc, flags, true);
	  }
      }
      break;

    case MODIFY_EXPR:
    case INIT_EXPR:
      dump_generic_node (buffer, TREE_OPERAND (node, 0), spc, flags, false);
      pp_space (buffer);
      pp_character (buffer, '=');
      pp_space (buffer);
      dump_generic_node (buffer, TREE_OPERAND (node, 1), spc, flags, false);
      break;

    case TARGET_EXPR:
      dump_generic_node (buffer, TYPE_NAME (TREE_TYPE (node)), spc, flags, false);
      pp_character (buffer, '(');
      dump_generic_node (buffer, TARGET_EXPR_INITIAL (node), spc, flags, false);
      pp_character (buffer, ')');
      break;

    case COND_EXPR:
      if (TREE_TYPE (node) == void_type_node)
	{
	  pp_string (buffer, "if (");
	  dump_generic_node (buffer, COND_EXPR_COND (node), spc, flags, false);
	  pp_character (buffer, ')');
	  /* The lowered cond_exprs should always be printed in full.  */
	  if (COND_EXPR_THEN (node)
	      && TREE_CODE (COND_EXPR_THEN (node)) == GOTO_EXPR
	      && COND_EXPR_ELSE (node)
	      && TREE_CODE (COND_EXPR_ELSE (node)) == GOTO_EXPR)
	    {
	      pp_space (buffer);
	      dump_generic_node (buffer, COND_EXPR_THEN (node), 0, flags, true);
	      pp_string (buffer, " else ");
	      dump_generic_node (buffer, COND_EXPR_ELSE (node), 0, flags, true);
	    }
	  else if (!(flags & TDF_SLIM))
	    {
	      /* Output COND_EXPR_THEN.  */
	      if (COND_EXPR_THEN (node))
		{
		  newline_and_indent (buffer, spc+2);
		  pp_character (buffer, '{');
		  newline_and_indent (buffer, spc+4);
		  dump_generic_node (buffer, COND_EXPR_THEN (node), spc+4,
				     flags, true);
		  newline_and_indent (buffer, spc+2);
		  pp_character (buffer, '}');
		}

	      /* Output COND_EXPR_ELSE.  */
	      if (COND_EXPR_ELSE (node))
		{
		  newline_and_indent (buffer, spc);
		  pp_string (buffer, "else");
		  newline_and_indent (buffer, spc+2);
		  pp_character (buffer, '{');
		  newline_and_indent (buffer, spc+4);
		  dump_generic_node (buffer, COND_EXPR_ELSE (node), spc+4,
			             flags, true);
		  newline_and_indent (buffer, spc+2);
		  pp_character (buffer, '}');
		}
	    }
	  is_expr = false;
	}
      else
	{
	  dump_generic_node (buffer, TREE_OPERAND (node, 0), spc, flags, false);
	  pp_space (buffer);
	  pp_character (buffer, '?');
	  pp_space (buffer);
	  dump_generic_node (buffer, TREE_OPERAND (node, 1), spc, flags, false);
	  pp_space (buffer);
	  pp_character (buffer, ':');
	  pp_space (buffer);
	  dump_generic_node (buffer, TREE_OPERAND (node, 2), spc, flags, false);
	}
      break;

    case BIND_EXPR:
      pp_character (buffer, '{');
      if (!(flags & TDF_SLIM))
	{
	  if (BIND_EXPR_VARS (node))
	    {
	      pp_newline (buffer);

	      for (op0 = BIND_EXPR_VARS (node); op0; op0 = TREE_CHAIN (op0))
		{
		  print_declaration (buffer, op0, spc+2, flags);
		  pp_newline (buffer);
		}
	    }

	  newline_and_indent (buffer, spc+2);
	  dump_generic_node (buffer, BIND_EXPR_BODY (node), spc+2, flags, true);
	  newline_and_indent (buffer, spc);
	  pp_character (buffer, '}');
	}
      is_expr = false;
      break;

    case CALL_EXPR:
      print_call_name (buffer, node);

      /* Print parameters.  */
      pp_space (buffer);
      pp_character (buffer, '(');
      op1 = TREE_OPERAND (node, 1);
      if (op1)
	dump_generic_node (buffer, op1, spc, flags, false);
      pp_character (buffer, ')');

      op1 = TREE_OPERAND (node, 2);
      if (op1)
	{
	  pp_string (buffer, " [static-chain: ");
	  dump_generic_node (buffer, op1, spc, flags, false);
	  pp_character (buffer, ']');
	}

      if (CALL_EXPR_TAILCALL (node))
	pp_string (buffer, " [tail call]");
      break;

    case WITH_CLEANUP_EXPR:
      NIY;
      break;

    case CLEANUP_POINT_EXPR:
      pp_string (buffer, "<<cleanup_point ");
      dump_generic_node (buffer, TREE_OPERAND (node, 0), spc, flags, false);
      pp_string (buffer, ">>");
      break;

    case PLACEHOLDER_EXPR:
      NIY;
      break;

      /* Binary arithmetic and logic expressions.  */
    case MULT_EXPR:
    case PLUS_EXPR:
    case MINUS_EXPR:
    case TRUNC_DIV_EXPR:
    case CEIL_DIV_EXPR:
    case FLOOR_DIV_EXPR:
    case ROUND_DIV_EXPR:
    case TRUNC_MOD_EXPR:
    case CEIL_MOD_EXPR:
    case FLOOR_MOD_EXPR:
    case ROUND_MOD_EXPR:
    case RDIV_EXPR:
    case EXACT_DIV_EXPR:
    case LSHIFT_EXPR:
    case RSHIFT_EXPR:
    case LROTATE_EXPR:
    case RROTATE_EXPR:
    case BIT_IOR_EXPR:
    case BIT_XOR_EXPR:
    case BIT_AND_EXPR:
    case TRUTH_ANDIF_EXPR:
    case TRUTH_ORIF_EXPR:
    case TRUTH_AND_EXPR:
    case TRUTH_OR_EXPR:
    case TRUTH_XOR_EXPR:
    case LT_EXPR:
    case LE_EXPR:
    case GT_EXPR:
    case GE_EXPR:
    case EQ_EXPR:
    case NE_EXPR:
    case UNLT_EXPR:
    case UNLE_EXPR:
    case UNGT_EXPR:
    case UNGE_EXPR:
    case UNEQ_EXPR:
      {
	const char *op = op_symbol (node);
	op0 = TREE_OPERAND (node, 0);
	op1 = TREE_OPERAND (node, 1);

	/* When the operands are expressions with less priority,
	   keep semantics of the tree representation.  */
	if (op_prio (op0) < op_prio (node))
	  {
	    pp_character (buffer, '(');
	    dump_generic_node (buffer, op0, spc, flags, false);
	    pp_character (buffer, ')');
	  }
	else
	  dump_generic_node (buffer, op0, spc, flags, false);

	pp_space (buffer);
	pp_string (buffer, op);
	pp_space (buffer);

	/* When the operands are expressions with less priority,
	   keep semantics of the tree representation.  */
	if (op_prio (op1) < op_prio (node))
	  {
	    pp_character (buffer, '(');
	    dump_generic_node (buffer, op1, spc, flags, false);
	    pp_character (buffer, ')');
	  }
	else
	  dump_generic_node (buffer, op1, spc, flags, false);
      }
      break;

      /* Unary arithmetic and logic expressions.  */
    case NEGATE_EXPR:
    case BIT_NOT_EXPR:
    case TRUTH_NOT_EXPR:
    case ADDR_EXPR:
    case REFERENCE_EXPR:
    case PREDECREMENT_EXPR:
    case PREINCREMENT_EXPR:
    case INDIRECT_REF:
      if (TREE_CODE (node) == ADDR_EXPR
	  && (TREE_CODE (TREE_OPERAND (node, 0)) == STRING_CST
	      || TREE_CODE (TREE_OPERAND (node, 0)) == FUNCTION_DECL))
	;	/* Do not output '&' for strings and function pointers.  */
      else
	pp_string (buffer, op_symbol (node));

      if (op_prio (TREE_OPERAND (node, 0)) < op_prio (node))
	{
	  pp_character (buffer, '(');
	  dump_generic_node (buffer, TREE_OPERAND (node, 0), spc, flags, false);
	  pp_character (buffer, ')');
	}
      else
	dump_generic_node (buffer, TREE_OPERAND (node, 0), spc, flags, false);
      break;

    case POSTDECREMENT_EXPR:
    case POSTINCREMENT_EXPR:
      if (op_prio (TREE_OPERAND (node, 0)) < op_prio (node))
	{
	  pp_character (buffer, '(');
	  dump_generic_node (buffer, TREE_OPERAND (node, 0), spc, flags, false);
	  pp_character (buffer, ')');
	}
      else
	dump_generic_node (buffer, TREE_OPERAND (node, 0), spc, flags, false);
      pp_string (buffer, op_symbol (node));
      break;

    case MIN_EXPR:
      pp_string (buffer, "MIN_EXPR <");
      dump_generic_node (buffer, TREE_OPERAND (node, 0), spc, flags, false);
      pp_string (buffer, ", ");
      dump_generic_node (buffer, TREE_OPERAND (node, 1), spc, flags, false);
      pp_character (buffer, '>');
      break;

    case MAX_EXPR:
      pp_string (buffer, "MAX_EXPR <");
      dump_generic_node (buffer, TREE_OPERAND (node, 0), spc, flags, false);
      pp_string (buffer, ", ");
      dump_generic_node (buffer, TREE_OPERAND (node, 1), spc, flags, false);
      pp_character (buffer, '>');
      break;

    case ABS_EXPR:
      pp_string (buffer, "ABS_EXPR <");
      dump_generic_node (buffer, TREE_OPERAND (node, 0), spc, flags, false);
      pp_character (buffer, '>');
      break;

    case UNORDERED_EXPR:
      NIY;
      break;

    case ORDERED_EXPR:
      NIY;
      break;

    case IN_EXPR:
      NIY;
      break;

    case SET_LE_EXPR:
      NIY;
      break;

    case CARD_EXPR:
      NIY;
      break;

    case RANGE_EXPR:
      NIY;
      break;

    case FIX_TRUNC_EXPR:
    case FIX_CEIL_EXPR:
    case FIX_FLOOR_EXPR:
    case FIX_ROUND_EXPR:
    case FLOAT_EXPR:
    case CONVERT_EXPR:
    case NOP_EXPR:
      type = TREE_TYPE (node);
      op0 = TREE_OPERAND (node, 0);
      if (type != TREE_TYPE (op0))
	{
	  pp_character (buffer, '(');
	  dump_generic_node (buffer, type, spc, flags, false);
	  pp_string (buffer, ")");
	}
      if (op_prio (op0) < op_prio (node))
	pp_character (buffer, '(');
      dump_generic_node (buffer, op0, spc, flags, false);
      if (op_prio (op0) < op_prio (node))
	pp_character (buffer, ')');
      break;

    case VIEW_CONVERT_EXPR:
      pp_string (buffer, "VIEW_CONVERT_EXPR<");
      dump_generic_node (buffer, TREE_TYPE (node), spc, flags, false);
      pp_string (buffer, ">(");
      dump_generic_node (buffer, TREE_OPERAND (node, 0), spc, flags, false);
      pp_character (buffer, ')');
      break;

    case NON_LVALUE_EXPR:
      pp_string (buffer, "NON_LVALUE_EXPR <");
      dump_generic_node (buffer, TREE_OPERAND (node, 0), spc, flags, false);
      pp_character (buffer, '>');
      break;

    case SAVE_EXPR:
      pp_string (buffer, "SAVE_EXPR <");
      dump_generic_node (buffer, TREE_OPERAND (node, 0), spc, flags, false);
      pp_character (buffer, '>');
      break;

    case UNSAVE_EXPR:
      pp_string (buffer, "UNSAVE_EXPR <");
      dump_generic_node (buffer, TREE_OPERAND (node, 0), spc, flags, false);
      pp_character (buffer, '>');
      break;

    case RTL_EXPR:
      NIY;
      break;

    case ENTRY_VALUE_EXPR:
      NIY;
      break;

    case COMPLEX_EXPR:
      pp_string (buffer, "COMPLEX_EXPR <");
      dump_generic_node (buffer, TREE_OPERAND (node, 0), spc, flags, false);
      pp_string (buffer, ", ");
      dump_generic_node (buffer, TREE_OPERAND (node, 1), spc, flags, false);
      pp_string (buffer, ">");
      break;

    case CONJ_EXPR:
      pp_string (buffer, "CONJ_EXPR <");
      dump_generic_node (buffer, TREE_OPERAND (node, 0), spc, flags, false);
      pp_string (buffer, ">");
      break;

    case REALPART_EXPR:
      pp_string (buffer, "REALPART_EXPR <");
      dump_generic_node (buffer, TREE_OPERAND (node, 0), spc, flags, false);
      pp_string (buffer, ">");
      break;

    case IMAGPART_EXPR:
      pp_string (buffer, "IMAGPART_EXPR <");
      dump_generic_node (buffer, TREE_OPERAND (node, 0), spc, flags, false);
      pp_string (buffer, ">");
      break;

    case VA_ARG_EXPR:
      pp_string (buffer, "VA_ARG_EXPR <");
      dump_generic_node (buffer, TREE_OPERAND (node, 0), spc, flags, false);
      pp_string (buffer, ">");
      break;

    case TRY_FINALLY_EXPR:
    case TRY_CATCH_EXPR:
      pp_string (buffer, "try");
      newline_and_indent (buffer, spc+2);
      pp_string (buffer, "{");
      newline_and_indent (buffer, spc+4);
      dump_generic_node (buffer, TREE_OPERAND (node, 0), spc+4, flags, true);
      newline_and_indent (buffer, spc+2);
      pp_string (buffer, "}");
      newline_and_indent (buffer, spc);
      pp_string (buffer,
			 (TREE_CODE (node) == TRY_CATCH_EXPR) ? "catch" : "finally");
      newline_and_indent (buffer, spc+2);
      pp_string (buffer, "{");
      newline_and_indent (buffer, spc+4);
      dump_generic_node (buffer, TREE_OPERAND (node, 1), spc+4, flags, true);
      newline_and_indent (buffer, spc+2);
      pp_string (buffer, "}");
      is_expr = false;
      break;

    case CATCH_EXPR:
      pp_string (buffer, "catch (");
      dump_generic_node (buffer, CATCH_TYPES (node), spc+2, flags, false);
      pp_string (buffer, ")");
      newline_and_indent (buffer, spc+2);
      pp_string (buffer, "{");
      newline_and_indent (buffer, spc+4);
      dump_generic_node (buffer, CATCH_BODY (node), spc+4, flags, true);
      newline_and_indent (buffer, spc+2);
      pp_string (buffer, "}");
      is_expr = false;
      break;

    case EH_FILTER_EXPR:
      pp_string (buffer, "<<<eh_filter (");
      dump_generic_node (buffer, EH_FILTER_TYPES (node), spc+2, flags, false);
      pp_string (buffer, ")>>>");
      newline_and_indent (buffer, spc+2);
      pp_string (buffer, "{");
      newline_and_indent (buffer, spc+4);
      dump_generic_node (buffer, EH_FILTER_FAILURE (node), spc+4, flags, true);
      newline_and_indent (buffer, spc+2);
      pp_string (buffer, "}");
      is_expr = false;
      break;

    case GOTO_SUBROUTINE_EXPR:
      NIY;
      break;

    case LABEL_EXPR:
      op0 = TREE_OPERAND (node, 0);
      /* If this is for break or continue, don't bother printing it.  */
      if (DECL_NAME (op0))
	{
	  const char *name = IDENTIFIER_POINTER (DECL_NAME (op0));
	  if (strcmp (name, "break") == 0
	      || strcmp (name, "continue") == 0)
	    break;
	}
      dump_generic_node (buffer, op0, spc, flags, false);
      pp_character (buffer, ':');
      if (DECL_NONLOCAL (op0))
	pp_string (buffer, " [non-local]");
      break;

    case LABELED_BLOCK_EXPR:
      op0 = LABELED_BLOCK_LABEL (node);
      /* If this is for break or continue, don't bother printing it.  */
      if (DECL_NAME (op0))
	{
	  const char *name = IDENTIFIER_POINTER (DECL_NAME (op0));
	  if (strcmp (name, "break") == 0
	      || strcmp (name, "continue") == 0)
	    {
	      dump_generic_node (buffer, LABELED_BLOCK_BODY (node), spc, flags, false);
	      break;
	    }
	}
      dump_generic_node (buffer, LABELED_BLOCK_LABEL (node), spc, flags, false);
      pp_string (buffer, ": {");
      if (!(flags & TDF_SLIM))
	newline_and_indent (buffer, spc+2);
      dump_generic_node (buffer, LABELED_BLOCK_BODY (node), spc+2, flags, true);
      if (!flags)
	newline_and_indent (buffer, spc);
      pp_character (buffer, '}');
      is_expr = false;
      break;

    case EXIT_BLOCK_EXPR:
      op0 = LABELED_BLOCK_LABEL (EXIT_BLOCK_LABELED_BLOCK (node));
      /* If this is for a break or continue, print it accordingly.  */
      if (DECL_NAME (op0))
	{
	  const char *name = IDENTIFIER_POINTER (DECL_NAME (op0));
	  if (strcmp (name, "break") == 0
	      || strcmp (name, "continue") == 0)
	    {
	      pp_string (buffer, name);
	      break;
	    }
	}
      pp_string (buffer, "<<<exit block ");
      dump_generic_node (buffer, op0, spc, flags, false);
      pp_string (buffer, ">>>");
      break;

    case EXC_PTR_EXPR:
      pp_string (buffer, "<<<exception object>>>");
      break;

    case FILTER_EXPR:
      pp_string (buffer, "<<<filter object>>>");
      break;

    case LOOP_EXPR:
      pp_string (buffer, "while (1)");
      if (!(flags & TDF_SLIM))
	{
	  newline_and_indent (buffer, spc+2);
	  pp_character (buffer, '{');
	  newline_and_indent (buffer, spc+4);
	  dump_generic_node (buffer, LOOP_EXPR_BODY (node), spc+4, flags, true);
	  newline_and_indent (buffer, spc+2);
	  pp_character (buffer, '}');
	}
      is_expr = false;
      break;

    case RETURN_EXPR:
      pp_string (buffer, "return");
      op0 = TREE_OPERAND (node, 0);
      if (op0)
	{
	  pp_space (buffer);
	  if (TREE_CODE (op0) == MODIFY_EXPR)
	    dump_generic_node (buffer, TREE_OPERAND (op0, 1), spc, flags, false);
	  else
	    dump_generic_node (buffer, op0, spc, flags, false);
	}
      break;

    case EXIT_EXPR:
      pp_string (buffer, "if (");
      dump_generic_node (buffer, TREE_OPERAND (node, 0), spc, flags, false);
      pp_string (buffer, ") break");
      break;

    case SWITCH_EXPR:
      pp_string (buffer, "switch (");
      dump_generic_node (buffer, SWITCH_COND (node), spc, flags, false);
      pp_character (buffer, ')');
      if (!(flags & TDF_SLIM))
	{
	  newline_and_indent (buffer, spc+2);
	  pp_character (buffer, '{');
	  if (SWITCH_BODY (node))
	    {
	      newline_and_indent (buffer, spc+4);
	      dump_generic_node (buffer, SWITCH_BODY (node), spc+4, flags, true);
	    }
	  else
	    {
	      tree vec = SWITCH_LABELS (node);
	      size_t i, n = TREE_VEC_LENGTH (vec);
	      for (i = 0; i < n; ++i)
		{
		  tree elt = TREE_VEC_ELT (vec, i);
		  newline_and_indent (buffer, spc+4);
		  dump_generic_node (buffer, elt, spc+4, flags, false);
		  pp_string (buffer, " goto ");
		  dump_generic_node (buffer, CASE_LABEL (elt), spc+4, flags, true);
		  pp_semicolon (buffer);
		}
	    }
	  newline_and_indent (buffer, spc+2);
	  pp_character (buffer, '}');
	}
      is_expr = false;
      break;

    case GOTO_EXPR:
      op0 = GOTO_DESTINATION (node);
      if (TREE_CODE (op0) != SSA_NAME
	  && DECL_P (op0)
	  && DECL_NAME (op0))
	{
	  const char *name = IDENTIFIER_POINTER (DECL_NAME (op0));
	  if (strcmp (name, "break") == 0
	      || strcmp (name, "continue") == 0)
	    {
	      pp_string (buffer, name);
	      break;
	    }
	}
      pp_string (buffer, "goto ");
      dump_generic_node (buffer, op0, spc, flags, false);
      break;

    case RESX_EXPR:
      pp_string (buffer, "resx");
      /* ??? Any sensible way to present the eh region?  */
      break;

    case ASM_EXPR:
      pp_string (buffer, "__asm__");
      if (ASM_VOLATILE_P (node))
	pp_string (buffer, " __volatile__");
      pp_character (buffer, '(');
      dump_generic_node (buffer, ASM_STRING (node), spc, flags, false);
      pp_character (buffer, ':');
      dump_generic_node (buffer, ASM_OUTPUTS (node), spc, flags, false);
      pp_character (buffer, ':');
      dump_generic_node (buffer, ASM_INPUTS (node), spc, flags, false);
      if (ASM_CLOBBERS (node))
	{
	  pp_character (buffer, ':');
	  dump_generic_node (buffer, ASM_CLOBBERS (node), spc, flags, false);
	}
      pp_string (buffer, ")");
      break;

    case CASE_LABEL_EXPR:
      if (CASE_LOW (node) && CASE_HIGH (node))
	{
	  pp_string (buffer, "case ");
	  dump_generic_node (buffer, CASE_LOW (node), spc, flags, false);
	  pp_string (buffer, " ... ");
	  dump_generic_node (buffer, CASE_HIGH (node), spc, flags, false);
	}
      else if (CASE_LOW (node))
	{
	  pp_string (buffer, "case ");
	  dump_generic_node (buffer, CASE_LOW (node), spc, flags, false);
	}
      else
	pp_string (buffer, "default ");
      pp_character (buffer, ':');
      break;

    case VTABLE_REF:
      pp_string (buffer, "VTABLE_REF <(");
      dump_generic_node (buffer, TREE_OPERAND (node, 0), spc, flags, false);
      pp_string (buffer, "),");
      dump_generic_node (buffer, TREE_OPERAND (node, 1), spc, flags, false);
      pp_character (buffer, ',');
      dump_generic_node (buffer, TREE_OPERAND (node, 2), spc, flags, false);
      pp_character (buffer, '>');
      break;

    case EPHI_NODE:
      {
	int i;

	pp_string (buffer, " EPHI (");
	dump_generic_node (buffer, EREF_NAME (node), spc, flags, false);
	pp_string (buffer, ") ");
	pp_character (buffer, '[');
	pp_string (buffer, " class:");
	pp_decimal_int (buffer, EREF_CLASS (node));
	if (EPHI_DOWNSAFE (node))
	  pp_string (buffer, " downsafe");
	if (EPHI_CANT_BE_AVAIL (node))
	  pp_string (buffer, " cant_be_avail");
	if (EPHI_STOPS (node))
	  pp_string (buffer, " stops");
	pp_string (buffer, " bb:");
	pp_decimal_int (buffer, bb_for_stmt (node)->index);
	pp_character (buffer, ']');
	if (! (flags & TDF_SLIM))
	  {
	    pp_string (buffer, " <");
	    for (i = 0; i < EPHI_NUM_ARGS (node); i++)
	      {	    
		if (EPHI_ARG_DEF (node, i))
		  {
		    newline_and_indent (buffer, spc + 2);
		    pp_string (buffer, " edge ");
		    pp_decimal_int (buffer, EPHI_ARG_EDGE (node, i)->src->index);
		    pp_string (buffer, "->");
		    pp_decimal_int (buffer, EPHI_ARG_EDGE (node, i)->dest->index);
		    pp_string (buffer, " [ ");
		    if (EPHI_ARG_HAS_REAL_USE (node, i))
		      pp_string (buffer, " real use");
		    if (EPHI_ARG_INJURED (node, i))
		      pp_string (buffer, " injured");
		    if (EPHI_ARG_STOPS (node, i))
		      pp_string (buffer, " stops");
		    pp_string (buffer, " ] ");
		    pp_string (buffer, " defined by:");
		    dump_generic_node (buffer, EPHI_ARG_DEF (node, i),
				       spc + 4, flags | TDF_SLIM, false);
		  }
	      }
	  }
	pp_string (buffer, " >");
      }
      break;
    case EEXIT_NODE:
    case EKILL_NODE:
      if (TREE_CODE (node) == EEXIT_NODE)
	pp_string (buffer, "EEXIT (");
      else if (TREE_CODE (node) == EKILL_NODE)
	pp_string (buffer, "EKILL (");
      dump_generic_node (buffer, EREF_NAME (node), spc, flags, false);
      pp_string (buffer, ") ");
      pp_character (buffer, '[');
      pp_string (buffer, "class:");
      pp_decimal_int (buffer, EREF_CLASS (node));
      pp_string (buffer, " bb:");
      pp_decimal_int (buffer, bb_for_stmt (node)->index);
      pp_character (buffer, ']');
      break;
    case EUSE_NODE:
      pp_string (buffer, " EUSE (");
      dump_generic_node (buffer, EREF_NAME (node), spc, flags, false);

      pp_string (buffer, ") ");
      pp_character (buffer, '[');
      pp_string (buffer, "class:");
      pp_decimal_int (buffer, EREF_CLASS (node));
      pp_string (buffer, " phiop:");
      pp_decimal_int (buffer, EUSE_PHIOP (node));
      pp_string (buffer, " bb:");
      pp_decimal_int (buffer, bb_for_stmt (node)->index);
      if (EUSE_LVAL (node))
	pp_string (buffer, " left-occurrence");
      pp_string (buffer, " ]");
	
      break;
    case PHI_NODE:
      {
	int i;

	dump_generic_node (buffer, PHI_RESULT (node), spc, flags, false);
	pp_string (buffer, " = PHI <");
	for (i = 0; i < PHI_NUM_ARGS (node); i++)
	  {
	    dump_generic_node (buffer, PHI_ARG_DEF (node, i), spc, flags, false);
	    pp_string (buffer, "(");
	    pp_decimal_int (buffer, PHI_ARG_EDGE (node, i)->src->index);
	    pp_string (buffer, ")");
	    if (i < PHI_NUM_ARGS (node) - 1)
	      pp_string (buffer, ", ");
	  }
	pp_string (buffer, ">;");
      }
      break;

    case SSA_NAME:
      dump_generic_node (buffer, SSA_NAME_VAR (node), spc, flags, false);
      pp_string (buffer, "_");
      pp_decimal_int (buffer, SSA_NAME_VERSION (node));
      break;

    default:
      NIY;
    }

  if (is_stmt && is_expr)
    pp_semicolon (buffer);
  pp_write_text_to_stream (buffer);

  return spc;
}

/* Print the declaration of a variable.  */

static void
print_declaration (pretty_printer *buffer, tree t, int spc, int flags)
{
  /* Don't print type declarations.  */
  if (TREE_CODE (t) == TYPE_DECL)
    return;

  INDENT (spc);

  if (DECL_REGISTER (t))
    pp_string (buffer, "register ");

  if (TREE_PUBLIC (t) && DECL_EXTERNAL (t))
    pp_string (buffer, "extern ");
  else if (TREE_STATIC (t))
    pp_string (buffer, "static ");

  /* Print the type and name.  */
  if (TREE_CODE (TREE_TYPE (t)) == ARRAY_TYPE)
    {
      tree tmp;

      /* Print array's type.  */
      tmp = TREE_TYPE (t);
      while (TREE_CODE (TREE_TYPE (tmp)) == ARRAY_TYPE)
	tmp = TREE_TYPE (tmp);
      dump_generic_node (buffer, TREE_TYPE (tmp), spc, flags, false);

      /* Print variable's name.  */
      pp_space (buffer);
      dump_generic_node (buffer, t, spc, flags, false);

      /* Print the dimensions.  */
      tmp = TREE_TYPE (t);
      while (TREE_CODE (tmp) == ARRAY_TYPE)
	{
	  pp_character (buffer, '[');
	  if (TYPE_DOMAIN (tmp))
	    {
	      if (TREE_CODE (TYPE_SIZE (tmp)) == INTEGER_CST)
		pp_wide_integer (buffer,
				TREE_INT_CST_LOW (TYPE_SIZE (tmp)) /
				TREE_INT_CST_LOW (TYPE_SIZE (TREE_TYPE (tmp))));
	      else
		dump_generic_node (buffer, TYPE_SIZE_UNIT (tmp), spc, flags,
				   false);
	    }
	  pp_character (buffer, ']');
	  tmp = TREE_TYPE (tmp);
	}
    }
  else
    {
      /* Print type declaration.  */
      dump_generic_node (buffer, TREE_TYPE (t), spc, flags, false);

      /* Print variable's name.  */
      pp_space (buffer);
      dump_generic_node (buffer, t, spc, flags, false);
    }

  /* The initial value of a function serves to determine wether the function
     is declared or defined.  So the following does not apply to function
     nodes.  */
  if (TREE_CODE (t) != FUNCTION_DECL)
    {
      /* Print the initial value.  */
      if (DECL_INITIAL (t))
	{
	  pp_space (buffer);
	  pp_character (buffer, '=');
	  pp_space (buffer);
	  dump_generic_node (buffer, DECL_INITIAL (t), spc, flags, false);
	}
    }

  pp_character (buffer, ';');
}


/* Prints a structure: name, fields, and methods.
   FIXME: Still incomplete.  */

static void
print_struct_decl (pretty_printer *buffer, tree node, int spc, int flags)
{
  /* Print the name of the structure.  */
  if (TYPE_NAME (node))
    {
      INDENT (spc);
      if (TREE_CODE (node) == RECORD_TYPE)
	pp_string (buffer, "struct ");
      else if (TREE_CODE (node) == UNION_TYPE)
	pp_string (buffer, "union ");
      else
	NIY;
      dump_generic_node (buffer, TYPE_NAME (node), spc, 0, false);
    }

  /* Print the contents of the structure.  */
  pp_newline (buffer);
  INDENT (spc);
  pp_character (buffer, '{');
  pp_newline (buffer);

  /* Print the fields of the structure.  */
  {
    tree tmp;
    tmp = TYPE_FIELDS (node);
    while (tmp)
      {
	/* Avoid to print recursively the structure.  */
	/* FIXME : Not implemented correctly...,
	   what about the case when we have a cycle in the contain graph? ...
	   Maybe this could be solved by looking at the scope in which the
	   structure was declared.  */
	if (TREE_TYPE (tmp) != node
	    || (TREE_CODE (TREE_TYPE (tmp)) == POINTER_TYPE &&
		TREE_TYPE (TREE_TYPE (tmp)) != node))
	  {
	    print_declaration (buffer, tmp, spc+2, flags);
	    pp_newline (buffer);
	  }
	else
	  {

	  }
	tmp = TREE_CHAIN (tmp);
      }
  }
  INDENT (spc);
  pp_character (buffer, '}');
}

/* Return the priority of the operator OP.

   From lowest to highest precedence with either left-to-right (L-R)
   or right-to-left (R-L) associativity]:

     1	[L-R] ,
     2	[R-L] = += -= *= /= %= &= ^= |= <<= >>=
     3	[R-L] ?:
     4	[L-R] ||
     5	[L-R] &&
     6	[L-R] |
     7	[L-R] ^
     8	[L-R] &
     9	[L-R] == !=
    10	[L-R] < <= > >=
    11	[L-R] << >>
    12	[L-R] + -
    13	[L-R] * / %
    14	[R-L] ! ~ ++ -- + - * & (type) sizeof
    15	[L-R] fn() [] -> .

   unary +, - and * have higher precedence than the corresponding binary
   operators.  */

static int
op_prio (tree op)
{
  if (op == NULL)
    return 9999;

  switch (TREE_CODE (op))
    {
    case TREE_LIST:
    case COMPOUND_EXPR:
    case BIND_EXPR:
      return 1;

    case MODIFY_EXPR:
    case INIT_EXPR:
      return 2;

    case COND_EXPR:
      return 3;

    case TRUTH_OR_EXPR:
    case TRUTH_ORIF_EXPR:
      return 4;

    case TRUTH_AND_EXPR:
    case TRUTH_ANDIF_EXPR:
      return 5;

    case BIT_IOR_EXPR:
      return 6;

    case BIT_XOR_EXPR:
    case TRUTH_XOR_EXPR:
      return 7;

    case BIT_AND_EXPR:
      return 8;

    case EQ_EXPR:
    case NE_EXPR:
      return 9;

    case LT_EXPR:
    case LE_EXPR:
    case GT_EXPR:
    case GE_EXPR:
      return 10;

    case LSHIFT_EXPR:
    case RSHIFT_EXPR:
    case LROTATE_EXPR:
    case RROTATE_EXPR:
      return 11;

    case PLUS_EXPR:
    case MINUS_EXPR:
      return 12;

    case MULT_EXPR:
    case TRUNC_DIV_EXPR:
    case CEIL_DIV_EXPR:
    case FLOOR_DIV_EXPR:
    case ROUND_DIV_EXPR:
    case RDIV_EXPR:
    case EXACT_DIV_EXPR:
    case TRUNC_MOD_EXPR:
    case CEIL_MOD_EXPR:
    case FLOOR_MOD_EXPR:
    case ROUND_MOD_EXPR:
      return 13;

    case TRUTH_NOT_EXPR:
    case BIT_NOT_EXPR:
    case POSTINCREMENT_EXPR:
    case POSTDECREMENT_EXPR:
    case PREINCREMENT_EXPR:
    case PREDECREMENT_EXPR:
    case NEGATE_EXPR:
    case INDIRECT_REF:
    case ADDR_EXPR:
    case FLOAT_EXPR:
    case NOP_EXPR:
    case CONVERT_EXPR:
    case FIX_TRUNC_EXPR:
    case FIX_CEIL_EXPR:
    case FIX_FLOOR_EXPR:
    case FIX_ROUND_EXPR:
    case TARGET_EXPR:
      return 14;

    case CALL_EXPR:
    case ARRAY_REF:
    case COMPONENT_REF:
      return 15;

      /* Special expressions.  */
    case MIN_EXPR:
    case MAX_EXPR:
    case ABS_EXPR:
    case REALPART_EXPR:
    case IMAGPART_EXPR:
      return 16;

    case SAVE_EXPR:
    case NON_LVALUE_EXPR:
      return op_prio (TREE_OPERAND (op, 0));

    default:
      /* Return an arbitrarily high precedence to avoid surrounding single
	 VAR_DECLs in ()s.  */
      return 9999;
    }
}


/* Return the symbol associated with operator OP.  */

static const char *
op_symbol (tree op)
{
  if (op == NULL)
    abort ();

  switch (TREE_CODE (op))
    {
    case MODIFY_EXPR:
      return "=";

    case TRUTH_OR_EXPR:
    case TRUTH_ORIF_EXPR:
      return "||";

    case TRUTH_AND_EXPR:
    case TRUTH_ANDIF_EXPR:
      return "&&";

    case BIT_IOR_EXPR:
      return "|";

    case TRUTH_XOR_EXPR:
    case BIT_XOR_EXPR:
      return "^";

    case ADDR_EXPR:
    case BIT_AND_EXPR:
      return "&";

    case EQ_EXPR:
    case UNEQ_EXPR:
      return "==";

    case NE_EXPR:
      return "!=";

    case LT_EXPR:
    case UNLT_EXPR:
      return "<";

    case LE_EXPR:
    case UNLE_EXPR:
      return "<=";

    case GT_EXPR:
    case UNGT_EXPR:
      return ">";

    case GE_EXPR:
    case UNGE_EXPR:
      return ">=";

    case LSHIFT_EXPR:
      return "<<";

    case RSHIFT_EXPR:
      return ">>";

    case PLUS_EXPR:
      return "+";

    case NEGATE_EXPR:
    case MINUS_EXPR:
      return "-";

    case BIT_NOT_EXPR:
      return "~";

    case TRUTH_NOT_EXPR:
      return "!";

    case MULT_EXPR:
    case INDIRECT_REF:
      return "*";

    case TRUNC_DIV_EXPR:
    case CEIL_DIV_EXPR:
    case FLOOR_DIV_EXPR:
    case ROUND_DIV_EXPR:
    case RDIV_EXPR:
    case EXACT_DIV_EXPR:
      return "/";

    case TRUNC_MOD_EXPR:
    case CEIL_MOD_EXPR:
    case FLOOR_MOD_EXPR:
    case ROUND_MOD_EXPR:
      return "%";

    case PREDECREMENT_EXPR:
      return " --";

    case PREINCREMENT_EXPR:
      return " ++";

    case POSTDECREMENT_EXPR:
      return "-- ";

    case POSTINCREMENT_EXPR:
      return "++ ";

    case REFERENCE_EXPR:
      return "";

    default:
      return "<<< ??? >>>";
    }
}

/* Prints the name of a CALL_EXPR.  */

static void
print_call_name (pretty_printer *buffer, tree node)
{
  tree op0;

  if (TREE_CODE (node) != CALL_EXPR)
    abort ();

  op0 = TREE_OPERAND (node, 0);

  if (TREE_CODE (op0) == NON_LVALUE_EXPR)
    op0 = TREE_OPERAND (op0, 0);

  switch (TREE_CODE (op0))
    {
    case VAR_DECL:
    case PARM_DECL:
      PRINT_FUNCTION_NAME (op0);
      break;

    case ADDR_EXPR:
    case INDIRECT_REF:
    case NOP_EXPR:
      dump_generic_node (buffer, TREE_OPERAND (op0, 0), 0, 0, false);
      break;

    case COND_EXPR:
      pp_string (buffer, "(");
      dump_generic_node (buffer, TREE_OPERAND (op0, 0), 0, 0, false);
      pp_string (buffer, ") ? ");
      dump_generic_node (buffer, TREE_OPERAND (op0, 1), 0, 0, false);
      pp_string (buffer, " : ");
      dump_generic_node (buffer, TREE_OPERAND (op0, 2), 0, 0, false);
      break;

    case COMPONENT_REF:
      /* The function is a pointer contained in a structure.  */
      if (TREE_CODE (TREE_OPERAND (op0, 0)) == INDIRECT_REF ||
	  TREE_CODE (TREE_OPERAND (op0, 0)) == VAR_DECL)
	PRINT_FUNCTION_NAME (TREE_OPERAND (op0, 1));
      else
	dump_generic_node (buffer, TREE_OPERAND (op0, 0), 0, 0, false);
      /* else
	 We can have several levels of structures and a function
	 pointer inside.  This is not implemented yet...  */
      /*		  NIY;*/
      break;

    case ARRAY_REF:
      if (TREE_CODE (TREE_OPERAND (op0, 0)) == VAR_DECL)
	PRINT_FUNCTION_NAME (TREE_OPERAND (op0, 0));
      else
	dump_generic_node (buffer, op0, 0, 0, false);
      break;

    case SSA_NAME:
      dump_generic_node (buffer, op0, 0, 0, false);
      break;

    default:
      NIY;
    }
}

/* Parses the string STR and replaces new-lines by '\n', tabs by '\t', ...  */

static void
pretty_print_string (pretty_printer *buffer, const char *str)
{
  if (str == NULL)
    return;

  while (*str)
    {
      switch (str[0])
	{
	case '\b':
	  pp_string (buffer, "\\b");
	  break;

	case '\f':
	  pp_string (buffer, "\\f");
	  break;

	case '\n':
	  pp_string (buffer, "\\n");
	  break;

	case '\r':
	  pp_string (buffer, "\\r");
	  break;

	case '\t':
	  pp_string (buffer, "\\t");
	  break;

	case '\v':
	  pp_string (buffer, "\\v");
	  break;

	case '\\':
	  pp_string (buffer, "\\\\");
	  break;

	case '\"':
	  pp_string (buffer, "\\\"");
	  break;

	case '\'':
	  pp_string (buffer, "\\'");
	  break;

	case '\0':
	  pp_string (buffer, "\\0");
	  break;

	case '\1':
	  pp_string (buffer, "\\1");
	  break;

	case '\2':
	  pp_string (buffer, "\\2");
	  break;

	case '\3':
	  pp_string (buffer, "\\3");
	  break;

	case '\4':
	  pp_string (buffer, "\\4");
	  break;

	case '\5':
	  pp_string (buffer, "\\5");
	  break;

	case '\6':
	  pp_string (buffer, "\\6");
	  break;

	case '\7':
	  pp_string (buffer, "\\7");
	  break;

	default:
	  pp_character (buffer, str[0]);
	  break;
	}
      str++;
    }
}

static void
maybe_init_pretty_print (FILE *file)
{
  if (!initialized)
    {
      pp_construct (&buffer, /* prefix */NULL, /* line-width */0);
      pp_needs_newline (&buffer) = true;
      initialized = 1;
    }

  buffer.buffer->stream = file;
}

static void
newline_and_indent (pretty_printer *buffer, int spc)
{
  pp_newline (buffer);
  INDENT (spc);
}

static void
dump_vops (pretty_printer *buffer, tree stmt, int spc, int flags)
{
  size_t i;
  stmt_ann_t ann = stmt_ann (stmt);
  vdef_optype vdefs = VDEF_OPS (ann);
  vuse_optype vuses = VUSE_OPS (ann);

  for (i = 0; i < NUM_VDEFS (vdefs); i++)
    {
      pp_string (buffer, "#   ");
      dump_generic_node (buffer, VDEF_RESULT (vdefs, i), spc + 2, flags, false);
      pp_string (buffer, " = VDEF <");
      dump_generic_node (buffer, VDEF_OP (vdefs, i), spc + 2, flags, false);
      pp_string (buffer, ">;");
      newline_and_indent (buffer, spc);
    }

  for (i = 0; i < NUM_VUSES (vuses); i++)
    {
      tree vuse = VUSE_OP (vuses, i);
      pp_string (buffer, "#   VUSE <");
      dump_generic_node (buffer, vuse, spc + 2, flags, false);
      pp_string (buffer, ">;");
      newline_and_indent (buffer, spc);
    }
}

/* Dumps basic block BB to FILE with details described by FLAGS and
   indented by INDENT spaces.  */

void
dump_generic_bb (FILE *file, basic_block bb, int indent, int flags)
{
  maybe_init_pretty_print (file);
  dumping_stmts = true;
  dump_generic_bb_buff (&buffer, bb, indent, flags);
  pp_flush (&buffer);
}

/* Dumps header of basic block BB to buffer BUFFER indented by INDENT
   spaces and details described by flags.  */

static void
dump_bb_header (pretty_printer *buffer, basic_block bb, int indent, int flags)
{
  edge e;
  tree stmt;

  if (flags & TDF_BLOCKS)
    {
      INDENT (indent);
      pp_string (buffer, "# BLOCK ");
      pp_decimal_int (buffer, bb->index);

      if (flags & TDF_LINENO)
	{
	  block_stmt_iterator bsi;

	  for (bsi = bsi_start (bb); !bsi_end_p (bsi); bsi_next (&bsi))
	    if (get_lineno (bsi_stmt (bsi)) != -1)
	      {
		pp_string (buffer, ", starting at line ");
		pp_decimal_int (buffer, get_lineno (bsi_stmt (bsi)));
		break;
	      }
	}
      newline_and_indent (buffer, indent);

      pp_string (buffer, "# PRED:");
      pp_write_text_to_stream (buffer);
      for (e = bb->pred; e; e = e->pred_next)
        if (flags & TDF_SLIM)
	  {
	    pp_string (buffer, " ");
	    if (e->src == ENTRY_BLOCK_PTR)
	      pp_string (buffer, "ENTRY");
	    else
	      pp_decimal_int (buffer, e->src->index);
	  }
	else
	  dump_edge_info (buffer->buffer->stream, e, 0);
      pp_newline (buffer);
    }
  else
    {
      stmt = first_stmt (bb);
      if (!stmt || TREE_CODE (stmt) != LABEL_EXPR)
	{
	  INDENT (indent - 2);
	  pp_string (buffer, "<bb ");
	  pp_decimal_int (buffer, bb->index);
	  pp_string (buffer, ">:");
	  pp_newline (buffer);
	}
    }
}

/* Dumps end of basic block BB to buffer BUFFER indented by INDENT
   spaces.  */

static void
dump_bb_end (pretty_printer *buffer, basic_block bb, int indent, int flags)
{
  edge e;

  INDENT (indent);
  pp_string (buffer, "# SUCC:");
  pp_write_text_to_stream (buffer);
  for (e = bb->succ; e; e = e->succ_next)
    if (flags & TDF_SLIM)
      {
	pp_string (buffer, " ");
	if (e->dest == EXIT_BLOCK_PTR)
	  pp_string (buffer, "EXIT");
	else
	  pp_decimal_int (buffer, e->dest->index);
      }
    else
      dump_edge_info (buffer->buffer->stream, e, 1);
  pp_newline (buffer);
}

/* Dumps phi nodes of basic block BB to buffer BUFFER with details described by
   FLAGS indented by INDENT spaces.  */

static void
dump_phi_nodes (pretty_printer *buffer, basic_block bb, int indent, int flags)
{
  tree phi = phi_nodes (bb);
  if (!phi)
    return;

  for (; phi; phi = TREE_CHAIN (phi))
    {
      if (is_gimple_reg (PHI_RESULT (phi)) || (flags & TDF_VOPS))
        {
          INDENT (indent);
          pp_string (buffer, "# ");
          dump_generic_node (buffer, phi, indent, flags, false);
          pp_newline (buffer);
        }
    }
}

/* Dump jump to basic block BB that is represented implicitly in the cfg
   to BUFFER.  */

static void
pp_cfg_jump (pretty_printer *buffer, basic_block bb)
{
  tree stmt;

  stmt = first_stmt (bb);

  pp_string (buffer, "goto <bb ");
  pp_decimal_int (buffer, bb->index);
  pp_string (buffer, ">");
  if (stmt && TREE_CODE (stmt) == LABEL_EXPR)
    {
      pp_string (buffer, " (");
      dump_generic_node (buffer, LABEL_EXPR_LABEL (stmt), 0, 0, false);
      pp_string (buffer, ")");
    }
  pp_semicolon (buffer);
}

/* Dump edges represented implicitly in basic block BB to BUFFER, indented
   by INDENT spaces, with details given by FLAGS.  */

static void
dump_implicit_edges (pretty_printer *buffer, basic_block bb, int indent)
{
  edge e;

  /* If there is a fallthru edge, we may need to add an artificial goto to the
     dump.  */
  for (e = bb->succ; e; e = e->succ_next)
    if (e->flags & EDGE_FALLTHRU)
      break;
  if (e && e->dest != bb->next_bb)
    {
      INDENT (indent);
      pp_cfg_jump (buffer, e->dest);
      pp_newline (buffer);
    }
}

/* Dumps basic block BB to buffer BUFFER with details described by FLAGS and
   indented by INDENT spaces.  */

static void
dump_generic_bb_buff (pretty_printer *buffer, basic_block bb,
		      int indent, int flags)
{
  block_stmt_iterator bsi;
  tree stmt;
  int label_indent = indent - 2;

  if (label_indent < 0)
    label_indent = 0;

  dump_bb_header (buffer, bb, indent, flags);

  if (bb_ann (bb))
    dump_phi_nodes (buffer, bb, indent, flags);
  
  for (bsi = bsi_start (bb); !bsi_end_p (bsi); bsi_next (&bsi))
    {
      int curr_indent;

      stmt = bsi_stmt (bsi);

      curr_indent = TREE_CODE (stmt) == LABEL_EXPR ? label_indent : indent;

      INDENT (curr_indent);
      dump_generic_node (buffer, stmt, curr_indent, flags, true);
      pp_newline (buffer);
    }

  dump_implicit_edges (buffer, bb, indent);

  if (flags & TDF_BLOCKS)
    dump_bb_end (buffer, bb, indent, flags);
}
