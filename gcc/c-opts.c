/* C/ObjC/C++ command line option handling.
   Copyright (C) 2002, 2003 Free Software Foundation, Inc.
   Contributed by Neil Booth.

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
#include "tree.h"
#include "c-common.h"
#include "c-pragma.h"
#include "flags.h"
#include "toplev.h"
#include "langhooks.h"
#include "tree-inline.h"
#include "diagnostic.h"
#include "intl.h"
#include "cppdefault.h"
#include "c-incpath.h"
#include "debug.h"		/* For debug_hooks.  */
#include "opts.h"
#include "options.h"

#ifndef DOLLARS_IN_IDENTIFIERS
# define DOLLARS_IN_IDENTIFIERS true
#endif

#ifndef TARGET_SYSTEM_ROOT
# define TARGET_SYSTEM_ROOT NULL
#endif

#ifndef TARGET_EBCDIC
# define TARGET_EBCDIC 0
#endif

static const int lang_flags[] = {CL_C, CL_ObjC, CL_CXX, CL_ObjCXX};

static int saved_lineno;

/* CPP's options.  */
static cpp_options *cpp_opts;

/* Input filename.  */
static const char *in_fname;

/* Filename and stream for preprocessed output.  */
static const char *out_fname;
static FILE *out_stream;

/* Append dependencies to deps_file.  */
static bool deps_append;

/* If dependency switches (-MF etc.) have been given.  */
static bool deps_seen;

/* If -v seen.  */
static bool verbose;

/* Dependency output file.  */
static const char *deps_file;

/* The prefix given by -iprefix, if any.  */
static const char *iprefix;

/* The system root, if any.  Overridden by -isysroot.  */
static const char *sysroot = TARGET_SYSTEM_ROOT;

/* Zero disables all standard directories for headers.  */
static bool std_inc = true;

/* Zero disables the C++-specific standard directories for headers.  */
static bool std_cxx_inc = true;

/* If the quote chain has been split by -I-.  */
static bool quote_chain_split;

/* If -Wunused-macros.  */
static bool warn_unused_macros;

/* Number of deferred options, deferred options array size.  */
static size_t deferred_count, deferred_size;

/* Number of deferred options scanned for -include.  */
static size_t include_cursor;

void missing_arg (enum opt_code);
static void set_Wimplicit (int);
static void print_help (void);
static void handle_OPT_d (const char *);
static void set_std_cxx98 (int);
static void set_std_c89 (int, int);
static void set_std_c99 (int);
static void check_deps_environment_vars (void);
static void handle_deferred_opts (void);
static void sanitize_cpp_opts (void);
static void add_prefixed_path (const char *, size_t);
static void push_command_line_include (void);
static void cb_file_change (cpp_reader *, const struct line_map *);
static void finish_options (void);

#ifndef STDC_0_IN_SYSTEM_HEADERS
#define STDC_0_IN_SYSTEM_HEADERS 0
#endif

/* Holds switches parsed by c_common_handle_option (), but whose
   handling is deferred to c_common_post_options ().  */
static void defer_opt (enum opt_code, const char *);
static struct deferred_opt
{
  enum opt_code code;
  const char *arg;
} *deferred_opts;

/* Complain that switch OPT_INDEX expects an argument but none was
   provided.  */
void
missing_arg (enum opt_code code)
{
  const char *opt_text = cl_options[code].opt_text;

  switch (code)
    {
    case OPT__output_pch_:
    case OPT_Wformat_:
    case OPT_d:
    case OPT_fabi_version_:
    case OPT_fbuiltin_:
    case OPT_fdump_:
    case OPT_fname_mangling_version_:
    case OPT_ftabstop_:
    case OPT_ftemplate_depth_:
    case OPT_iprefix:
    case OPT_iwithprefix:
    case OPT_iwithprefixbefore:
    default:
      error ("missing argument to \"-%s\"", opt_text);
      break;

    case OPT_fconstant_string_class_:
      error ("no class name specified with \"-%s\"", opt_text);
      break;

    case OPT_A:
      error ("assertion missing after \"-%s\"", opt_text);
      break;

    case OPT_D:
    case OPT_U:
      error ("macro name missing after \"-%s\"", opt_text);
      break;

    case OPT_I:
    case OPT_idirafter:
    case OPT_isysroot:
    case OPT_isystem:
      error ("missing path after \"-%s\"", opt_text);
      break;

    case OPT_MF:
    case OPT_MD:
    case OPT_MMD:
    case OPT_include:
    case OPT_imacros:
    case OPT_o:
      error ("missing filename after \"-%s\"", opt_text);
      break;

    case OPT_MQ:
    case OPT_MT:
      error ("missing target after \"-%s\"", opt_text);
      break;
    }
}

/* Defer option CODE with argument ARG.  */
static void
defer_opt (enum opt_code code, const char *arg)
{
  /* FIXME: this should be in c_common_init_options, which should take
     argc and argv.  */
  if (!deferred_opts)
    {
      extern int save_argc;
      deferred_size = save_argc;
      deferred_opts = (struct deferred_opt *)
	xmalloc (deferred_size * sizeof (struct deferred_opt));
    }

  if (deferred_count == deferred_size)
    abort ();

  deferred_opts[deferred_count].code = code;
  deferred_opts[deferred_count].arg = arg;
  deferred_count++;
}

/* Common initialization before parsing options.  */
int
c_common_init_options (enum c_language_kind lang)
{
  c_language = lang;
  parse_in = cpp_create_reader (lang == clk_c ? CLK_GNUC89 : CLK_GNUCXX,
				ident_hash);
  cpp_opts = cpp_get_options (parse_in);
  cpp_opts->dollars_in_ident = DOLLARS_IN_IDENTIFIERS;

  /* Reset to avoid warnings on internal definitions.  We set it just
     before passing on command-line options to cpplib.  */
  cpp_opts->warn_dollars = 0;

  if (flag_objc)
    cpp_opts->objc = 1;

  flag_const_strings = (lang == clk_cplusplus);
  warn_pointer_arith = (lang == clk_cplusplus);

  return lang_flags[(c_language << 1) + flag_objc];
}

/* Handle switch SCODE with argument ARG.  ON is true, unless no-
   form of an -f or -W option was given.  Returns 0 if the switch was
   invalid, a negative number to prevent language-independent
   processing in toplev.c (a hack necessary for the short-term).  */
int
c_common_handle_option (size_t scode, const char *arg, int value)
{
  const struct cl_option *option = &cl_options[scode];
  enum opt_code code = (enum opt_code) scode;
  int result = 1;

  if (code == N_OPTS)
    {
      if (!in_fname)
	in_fname = arg;
      else if (!out_fname)
	out_fname = arg;
      else
	  error ("too many filenames given.  Type %s --help for usage",
		 progname);
      return 1;
    }

  switch (code)
    {
    default:
      return 0;

    case OPT__help:
      print_help ();
      break;

    case OPT__output_pch_:
      pch_file = arg;
      break;

    case OPT_A:
      defer_opt (code, arg);
      break;

    case OPT_C:
      cpp_opts->discard_comments = 0;
      break;

    case OPT_CC:
      cpp_opts->discard_comments = 0;
      cpp_opts->discard_comments_in_macro_exp = 0;
      break;

    case OPT_D:
      defer_opt (code, arg);
      break;

    case OPT_E:
      flag_preprocess_only = 1;
      break;

    case OPT_H:
      cpp_opts->print_include_names = 1;
      break;

    case OPT_I:
      if (strcmp (arg, "-"))
	add_path (xstrdup (arg), BRACKET, 0);
      else
	{
	  if (quote_chain_split)
	    error ("-I- specified twice");
	  quote_chain_split = true;
	  split_quote_chain ();
	}
      break;

    case OPT_M:
    case OPT_MM:
      /* When doing dependencies with -M or -MM, suppress normal
	 preprocessed output, but still do -dM etc. as software
	 depends on this.  Preprocessed output does occur if -MD, -MMD
	 or environment var dependency generation is used.  */
      cpp_opts->deps.style = (code == OPT_M ? DEPS_SYSTEM: DEPS_USER);
      flag_no_output = 1;
      cpp_opts->inhibit_warnings = 1;
      break;

    case OPT_MD:
    case OPT_MMD:
      cpp_opts->deps.style = (code == OPT_MD ? DEPS_SYSTEM: DEPS_USER);
      deps_file = arg;
      break;

    case OPT_MF:
      deps_seen = true;
      deps_file = arg;
      break;

    case OPT_MG:
      deps_seen = true;
      cpp_opts->deps.missing_files = true;
      break;

    case OPT_MP:
      deps_seen = true;
      cpp_opts->deps.phony_targets = true;
      break;

    case OPT_MQ:
    case OPT_MT:
      deps_seen = true;
      defer_opt (code, arg);
      break;

    case OPT_P:
      flag_no_line_commands = 1;
      break;

    case OPT_U:
      defer_opt (code, arg);
      break;

    case OPT_Wabi:
      warn_abi = value;
      break;

    case OPT_Wall:
      set_Wunused (value);
      set_Wformat (value);
      set_Wimplicit (value);
      warn_char_subscripts = value;
      warn_missing_braces = value;
      warn_parentheses = value;
      warn_return_type = value;
      warn_sequence_point = value;	/* Was C only.  */
      if (c_language == clk_cplusplus)
	warn_sign_compare = value;
      warn_switch = value;
      warn_strict_aliasing = value;

      /* Only warn about unknown pragmas that are not in system
	 headers.  */
      warn_unknown_pragmas = value;

      /* We save the value of warn_uninitialized, since if they put
	 -Wuninitialized on the command line, we need to generate a
	 warning about not using it without also specifying -O.  */
      if (warn_uninitialized != 1)
	warn_uninitialized = (value ? 2 : 0);

      if (c_language == clk_c)
	/* We set this to 2 here, but 1 in -Wmain, so -ffreestanding
	   can turn it off only if it's not explicit.  */
	warn_main = value * 2;
      else
	{
	  /* C++-specific warnings.  */
	  warn_nonvdtor = value;
	  warn_reorder = value;
	  warn_nontemplate_friend = value;
	}

      cpp_opts->warn_trigraphs = value;
      cpp_opts->warn_comments = value;
      cpp_opts->warn_num_sign_change = value;
      cpp_opts->warn_multichar = value;	/* Was C++ only.  */
      break;

    case OPT_Wbad_function_cast:
      warn_bad_function_cast = value;
      break;

    case OPT_Wcast_qual:
      warn_cast_qual = value;
      break;

    case OPT_Wchar_subscripts:
      warn_char_subscripts = value;
      break;

    case OPT_Wcomment:
    case OPT_Wcomments:
      cpp_opts->warn_comments = value;
      break;

    case OPT_Wconversion:
      warn_conversion = value;
      break;

    case OPT_Wctor_dtor_privacy:
      warn_ctor_dtor_privacy = value;
      break;

    case OPT_Wdeprecated:
      warn_deprecated = value;
      cpp_opts->warn_deprecated = value;
      break;

    case OPT_Wdiv_by_zero:
      warn_div_by_zero = value;
      break;

    case OPT_Weffc__:
      warn_ecpp = value;
      break;

    case OPT_Wendif_labels:
      cpp_opts->warn_endif_labels = value;
      break;

    case OPT_Werror:
      cpp_opts->warnings_are_errors = value;
      break;

    case OPT_Werror_implicit_function_declaration:
      mesg_implicit_function_declaration = 2;
      break;

    case OPT_Wfloat_equal:
      warn_float_equal = value;
      break;

    case OPT_Wformat:
      set_Wformat (value);
      break;

    case OPT_Wformat_:
      set_Wformat (atoi (arg));
      break;

    case OPT_Wformat_extra_args:
      warn_format_extra_args = value;
      break;

    case OPT_Wformat_nonliteral:
      warn_format_nonliteral = value;
      break;

    case OPT_Wformat_security:
      warn_format_security = value;
      break;

    case OPT_Wformat_y2k:
      warn_format_y2k = value;
      break;

    case OPT_Wformat_zero_length:
      warn_format_zero_length = value;
      break;

    case OPT_Wimplicit:
      set_Wimplicit (value);
      break;

    case OPT_Wimplicit_function_declaration:
      mesg_implicit_function_declaration = value;
      break;

    case OPT_Wimplicit_int:
      warn_implicit_int = value;
      break;

    case OPT_Wimport:
      cpp_opts->warn_import = value;
      break;

    case OPT_Winvalid_offsetof:
      warn_invalid_offsetof = value;
      break;

    case OPT_Winvalid_pch:
      cpp_opts->warn_invalid_pch = value;
      break;

    case OPT_Wlong_long:
      warn_long_long = value;
      break;

    case OPT_Wmain:
      if (value)
	warn_main = 1;
      else
	warn_main = -1;
      break;

    case OPT_Wmissing_braces:
      warn_missing_braces = value;
      break;

    case OPT_Wmissing_declarations:
      warn_missing_declarations = value;
      break;

    case OPT_Wmissing_format_attribute:
      warn_missing_format_attribute = value;
      break;

    case OPT_Wmissing_prototypes:
      warn_missing_prototypes = value;
      break;

    case OPT_Wmultichar:
      cpp_opts->warn_multichar = value;
      break;

    case OPT_Wnested_externs:
      warn_nested_externs = value;
      break;

    case OPT_Wnon_template_friend:
      warn_nontemplate_friend = value;
      break;

    case OPT_Wnon_virtual_dtor:
      warn_nonvdtor = value;
      break;

    case OPT_Wnonnull:
      warn_nonnull = value;
      break;

    case OPT_Wold_style_cast:
      warn_old_style_cast = value;
      break;

    case OPT_Woverloaded_virtual:
      warn_overloaded_virtual = value;
      break;

    case OPT_Wparentheses:
      warn_parentheses = value;
      break;

    case OPT_Wpmf_conversions:
      warn_pmf2ptr = value;
      break;

    case OPT_Wpointer_arith:
      warn_pointer_arith = value;
      break;

    case OPT_Wprotocol:
      warn_protocol = value;
      break;

    case OPT_Wselector:
      warn_selector = value;
      break;

    case OPT_Wredundant_decls:
      warn_redundant_decls = value;
      break;

    case OPT_Wreorder:
      warn_reorder = value;
      break;

    case OPT_Wreturn_type:
      warn_return_type = value;
      break;

    case OPT_Wsequence_point:
      warn_sequence_point = value;
      break;

    case OPT_Wsign_compare:
      warn_sign_compare = value;
      break;

    case OPT_Wsign_promo:
      warn_sign_promo = value;
      break;

    case OPT_Wstrict_prototypes:
      warn_strict_prototypes = value;
      break;

    case OPT_Wsynth:
      warn_synth = value;
      break;

    case OPT_Wsystem_headers:
      cpp_opts->warn_system_headers = value;
      break;

    case OPT_Wtraditional:
      warn_traditional = value;
      cpp_opts->warn_traditional = value;
      break;

    case OPT_Wtrigraphs:
      cpp_opts->warn_trigraphs = value;
      break;

    case OPT_Wundeclared_selector:
      warn_undeclared_selector = value;
      break;

    case OPT_Wundef:
      cpp_opts->warn_undef = value;
      break;

    case OPT_Wunknown_pragmas:
      /* Set to greater than 1, so that even unknown pragmas in
	 system headers will be warned about.  */
      warn_unknown_pragmas = value * 2;
      break;

    case OPT_Wunused_macros:
      warn_unused_macros = value;
      break;

    case OPT_Wwrite_strings:
      if (c_language == clk_c)
	flag_const_strings = value;
      else
	warn_write_strings = value;
      break;

    case OPT_ansi:
      if (c_language == clk_c)
	set_std_c89 (false, true);
      else
	set_std_cxx98 (true);
      break;

    case OPT_d:
      handle_OPT_d (arg);
      break;

    case OPT_fcond_mismatch:
      if (c_language == clk_c)
	{
	  flag_cond_mismatch = value;
	  break;
	}
      /* Fall through.  */

    case OPT_fall_virtual:
    case OPT_fenum_int_equiv:
    case OPT_fguiding_decls:
    case OPT_fhonor_std:
    case OPT_fhuge_objects:
    case OPT_flabels_ok:
    case OPT_fname_mangling_version_:
    case OPT_fnew_abi:
    case OPT_fnonnull_objects:
    case OPT_fsquangle:
    case OPT_fstrict_prototype:
    case OPT_fthis_is_variable:
    case OPT_fvtable_thunks:
    case OPT_fxref:
      warning ("switch \"%s\" is no longer supported", option->opt_text);
      break;

    case OPT_fabi_version_:
      flag_abi_version = value;
      break;

    case OPT_faccess_control:
      flag_access_control = value;
      break;

    case OPT_falt_external_templates:
      flag_alt_external_templates = value;
      if (value)
	flag_external_templates = true;
    cp_deprecated:
      warning ("switch \"%s\" is deprecated, please see documentation for details", option->opt_text);
      break;

    case OPT_fasm:
      flag_no_asm = !value;
      break;

    case OPT_fbuiltin:
      flag_no_builtin = !value;
      break;

    case OPT_fbuiltin_:
      if (value)
	result = 0;
      else
	disable_builtin_function (arg);
      break;

    case OPT_fdollars_in_identifiers:
      cpp_opts->dollars_in_ident = value;
      break;

    case OPT_fdump_:
      if (!dump_switch_p (arg))
	result = 0;
      break;

    case OPT_ffreestanding:
      value = !value;
      /* Fall through...  */
    case OPT_fhosted:
      flag_hosted = value;
      flag_no_builtin = !value;
      /* warn_main will be 2 if set by -Wall, 1 if set by -Wmain */
      if (!value && warn_main == 2)
	warn_main = 0;
      break;

    case OPT_fshort_double:
      flag_short_double = value;
      break;

    case OPT_fshort_enums:
      flag_short_enums = value;
      break;

    case OPT_fshort_wchar:
      flag_short_wchar = value;
      break;

    case OPT_fsigned_bitfields:
      flag_signed_bitfields = value;
      explicit_flag_signed_bitfields = 1;
      break;

    case OPT_fsigned_char:
      flag_signed_char = value;
      break;

    case OPT_funsigned_bitfields:
      flag_signed_bitfields = !value;
      explicit_flag_signed_bitfields = 1;
      break;

    case OPT_funsigned_char:
      flag_signed_char = !value;
      break;

    case OPT_fcheck_new:
      flag_check_new = value;
      break;

    case OPT_fconserve_space:
      flag_conserve_space = value;
      break;

    case OPT_fconst_strings:
      flag_const_strings = value;
      break;

    case OPT_fconstant_string_class_:
      constant_string_class_name = arg;
      break;

    case OPT_fdefault_inline:
      flag_default_inline = value;
      break;

    case OPT_felide_constructors:
      flag_elide_constructors = value;
      break;

    case OPT_fenforce_eh_specs:
      flag_enforce_eh_specs = value;
      break;

    case OPT_fexternal_templates:
      flag_external_templates = value;
      goto cp_deprecated;

    case OPT_ffixed_form:
    case OPT_ffixed_line_length_:
      /* Fortran front end options ignored when preprocessing only.  */
      if (!flag_preprocess_only)
        result = 0;
      break;

    case OPT_ffor_scope:
      flag_new_for_scope = value;
      break;

    case OPT_fgnu_keywords:
      flag_no_gnu_keywords = !value;
      break;

    case OPT_fgnu_runtime:
      flag_next_runtime = !value;
      break;

    case OPT_fhandle_exceptions:
      warning ("-fhandle-exceptions has been renamed -fexceptions (and is now on by default)");
      flag_exceptions = value;
      break;

    case OPT_fimplement_inlines:
      flag_implement_inlines = value;
      break;

    case OPT_fimplicit_inline_templates:
      flag_implicit_inline_templates = value;
      break;

    case OPT_fimplicit_templates:
      flag_implicit_templates = value;
      break;

    case OPT_fms_extensions:
      flag_ms_extensions = value;
      break;

    case OPT_fnext_runtime:
      flag_next_runtime = value;
      break;

    case OPT_fnonansi_builtins:
      flag_no_nonansi_builtin = !value;
      break;

    case OPT_foperator_names:
      cpp_opts->operator_names = value;
      break;

    case OPT_foptional_diags:
      flag_optional_diags = value;
      break;

    case OPT_fpch_deps:
      cpp_opts->restore_pch_deps = value;
      break;

    case OPT_fpermissive:
      flag_permissive = value;
      break;

    case OPT_fpreprocessed:
      cpp_opts->preprocessed = value;
      break;

    case OPT_frepo:
      flag_use_repository = value;
      if (value)
	flag_implicit_templates = 0;
      break;

    case OPT_frtti:
      flag_rtti = value;
      break;

    case OPT_fshow_column:
      cpp_opts->show_column = value;
      break;

    case OPT_fstats:
      flag_detailed_statistics = value;
      break;

    case OPT_ftabstop_:
      /* It is documented that we silently ignore silly values.  */
      if (value >= 1 && value <= 100)
	cpp_opts->tabstop = value;
      break;

    case OPT_ftemplate_depth_:
      max_tinst_depth = value;
      break;

    case OPT_fvtable_gc:
      flag_vtable_gc = value;
      break;

    case OPT_fuse_cxa_atexit:
      flag_use_cxa_atexit = value;
      break;

    case OPT_fweak:
      flag_weak = value;
      break;

    case OPT_gen_decls:
      flag_gen_declaration = 1;
      break;

    case OPT_idirafter:
      add_path (xstrdup (arg), AFTER, 0);
      break;

    case OPT_imacros:
    case OPT_include:
      defer_opt (code, arg);
      break;

    case OPT_iprefix:
      iprefix = arg;
      break;

    case OPT_isysroot:
      sysroot = arg;
      break;

    case OPT_isystem:
      add_path (xstrdup (arg), SYSTEM, 0);
      break;

    case OPT_iwithprefix:
      add_prefixed_path (arg, SYSTEM);
      break;

    case OPT_iwithprefixbefore:
      add_prefixed_path (arg, BRACKET);
      break;

    case OPT_lang_asm:
      cpp_set_lang (parse_in, CLK_ASM);
      cpp_opts->dollars_in_ident = false;
      break;

    case OPT_lang_objc:
      cpp_opts->objc = 1;
      break;

    case OPT_nostdinc:
      std_inc = false;
      break;

    case OPT_nostdinc__:
      std_cxx_inc = false;
      break;

    case OPT_o:
      if (!out_fname)
	out_fname = arg;
      else
	error ("output filename specified twice");
      break;

      /* We need to handle the -pedantic switches here, rather than in
	 c_common_post_options, so that a subsequent -Wno-endif-labels
	 is not overridden.  */
    case OPT_pedantic_errors:
      cpp_opts->pedantic_errors = 1;
      /* fall through */
    case OPT_pedantic:
      cpp_opts->pedantic = 1;
      cpp_opts->warn_endif_labels = 1;
      break;

    case OPT_print_objc_runtime_info:
      print_struct_values = 1;
      break;

    case OPT_remap:
      cpp_opts->remap = 1;
      break;

    case OPT_std_c__98:
    case OPT_std_gnu__98:
      set_std_cxx98 (code == OPT_std_c__98 /* ISO */);
      break;

    case OPT_std_c89:
    case OPT_std_iso9899_1990:
    case OPT_std_iso9899_199409:
      set_std_c89 (code == OPT_std_iso9899_199409 /* c94 */, true /* ISO */);
      break;

    case OPT_std_gnu89:
      set_std_c89 (false /* c94 */, false /* ISO */);
      break;

    case OPT_std_c99:
    case OPT_std_c9x:
    case OPT_std_iso9899_1999:
    case OPT_std_iso9899_199x:
      set_std_c99 (true /* ISO */);
      break;

    case OPT_std_gnu99:
    case OPT_std_gnu9x:
      set_std_c99 (false /* ISO */);
      break;

    case OPT_trigraphs:
      cpp_opts->trigraphs = 1;
      break;

    case OPT_traditional_cpp:
      cpp_opts->traditional = 1;
      break;

    case OPT_undef:
      flag_undef = 1;
      break;

    case OPT_w:
      cpp_opts->inhibit_warnings = 1;
      break;

    case OPT_v:
      verbose = true;
      break;
    }

  return result;
}

/* Post-switch processing.  */
bool
c_common_post_options (const char **pfilename)
{
  /* Canonicalize the input and output filenames.  */
  if (in_fname == NULL || !strcmp (in_fname, "-"))
    in_fname = "";

  if (out_fname == NULL || !strcmp (out_fname, "-"))
    out_fname = "";

  if (cpp_opts->deps.style == DEPS_NONE)
    check_deps_environment_vars ();

  handle_deferred_opts ();

  sanitize_cpp_opts ();

  register_include_chains (parse_in, sysroot, iprefix,
			   std_inc, std_cxx_inc && c_language == clk_cplusplus,
			   verbose);

  flag_inline_trees = 1;

  /* Use tree inlining if possible.  Function instrumentation is only
     done in the RTL level, so we disable tree inlining.  */
  if (! flag_instrument_function_entry_exit)
    {
      if (!flag_no_inline)
	flag_no_inline = 1;
      if (flag_inline_functions)
	{
	  flag_inline_trees = 2;
	  flag_inline_functions = 0;
	}
    }

  /* -Wextra implies -Wsign-compare, but not if explicitly
      overridden.  */
  if (warn_sign_compare == -1)
    warn_sign_compare = extra_warnings;

  /* Special format checking options don't work without -Wformat; warn if
     they are used.  */
  if (warn_format_y2k && !warn_format)
    warning ("-Wformat-y2k ignored without -Wformat");
  if (warn_format_extra_args && !warn_format)
    warning ("-Wformat-extra-args ignored without -Wformat");
  if (warn_format_zero_length && !warn_format)
    warning ("-Wformat-zero-length ignored without -Wformat");
  if (warn_format_nonliteral && !warn_format)
    warning ("-Wformat-nonliteral ignored without -Wformat");
  if (warn_format_security && !warn_format)
    warning ("-Wformat-security ignored without -Wformat");
  if (warn_missing_format_attribute && !warn_format)
    warning ("-Wmissing-format-attribute ignored without -Wformat");

  if (flag_preprocess_only)
    {
      /* Open the output now.  We must do so even if flag_no_output is
	 on, because there may be other output than from the actual
	 preprocessing (e.g. from -dM).  */
      if (out_fname[0] == '\0')
	out_stream = stdout;
      else
	out_stream = fopen (out_fname, "w");

      if (out_stream == NULL)
	{
	  fatal_error ("opening output file %s: %m", out_fname);
	  return false;
	}

      init_pp_output (out_stream);
    }
  else
    {
      init_c_lex ();

      /* Yuk.  WTF is this?  I do know ObjC relies on it somewhere.  */
      input_line = 0;
    }

  cpp_get_callbacks (parse_in)->file_change = cb_file_change;

  /* NOTE: we use in_fname here, not the one supplied.  */
  *pfilename = cpp_read_main_file (parse_in, in_fname);

  saved_lineno = input_line;
  input_line = 0;

  /* If an error has occurred in cpplib, note it so we fail
     immediately.  */
  errorcount += cpp_errors (parse_in);

  return flag_preprocess_only;
}

/* Front end initialization common to C, ObjC and C++.  */
bool
c_common_init (void)
{
  input_line = saved_lineno;

  /* Set up preprocessor arithmetic.  Must be done after call to
     c_common_nodes_and_builtins for type nodes to be good.  */
  cpp_opts->precision = TYPE_PRECISION (intmax_type_node);
  cpp_opts->char_precision = TYPE_PRECISION (char_type_node);
  cpp_opts->int_precision = TYPE_PRECISION (integer_type_node);
  cpp_opts->wchar_precision = TYPE_PRECISION (wchar_type_node);
  cpp_opts->unsigned_wchar = TREE_UNSIGNED (wchar_type_node);
  cpp_opts->EBCDIC = TARGET_EBCDIC;

  if (flag_preprocess_only)
    {
      finish_options ();
      preprocess_file (parse_in);
      return false;
    }

  /* Has to wait until now so that cpplib has its hash table.  */
  init_pragma ();

  return true;
}

/* A thin wrapper around the real parser that initializes the
   integrated preprocessor after debug output has been initialized.
   Also, make sure the start_source_file debug hook gets called for
   the primary source file.  */
void
c_common_parse_file (int set_yydebug ATTRIBUTE_UNUSED)
{
#if YYDEBUG != 0
  yydebug = set_yydebug;
#else
  warning ("YYDEBUG not defined");
#endif

  (*debug_hooks->start_source_file) (input_line, input_filename);
  finish_options();
  pch_init();
  yyparse ();
  free_parser_stacks ();
}

/* Common finish hook for the C, ObjC and C++ front ends.  */
void
c_common_finish (void)
{
  FILE *deps_stream = NULL;

  if (cpp_opts->deps.style != DEPS_NONE)
    {
      /* If -M or -MM was seen without -MF, default output to the
	 output stream.  */
      if (!deps_file)
	deps_stream = out_stream;
      else
	{
	  deps_stream = fopen (deps_file, deps_append ? "a": "w");
	  if (!deps_stream)
	    fatal_error ("opening dependency file %s: %m", deps_file);
	}
    }

  /* For performance, avoid tearing down cpplib's internal structures
     with cpp_destroy ().  */
  errorcount += cpp_finish (parse_in, deps_stream);

  if (deps_stream && deps_stream != out_stream
      && (ferror (deps_stream) || fclose (deps_stream)))
    fatal_error ("closing dependency file %s: %m", deps_file);

  if (out_stream && (ferror (out_stream) || fclose (out_stream)))
    fatal_error ("when writing output to %s: %m", out_fname);
}

/* Either of two environment variables can specify output of
   dependencies.  Their value is either "OUTPUT_FILE" or "OUTPUT_FILE
   DEPS_TARGET", where OUTPUT_FILE is the file to write deps info to
   and DEPS_TARGET is the target to mention in the deps.  They also
   result in dependency information being appended to the output file
   rather than overwriting it, and like Sun's compiler
   SUNPRO_DEPENDENCIES suppresses the dependency on the main file.  */
static void
check_deps_environment_vars (void)
{
  char *spec;

  GET_ENVIRONMENT (spec, "DEPENDENCIES_OUTPUT");
  if (spec)
    cpp_opts->deps.style = DEPS_USER;
  else
    {
      GET_ENVIRONMENT (spec, "SUNPRO_DEPENDENCIES");
      if (spec)
	{
	  cpp_opts->deps.style = DEPS_SYSTEM;
	  cpp_opts->deps.ignore_main_file = true;
	}
    }

  if (spec)
    {
      /* Find the space before the DEPS_TARGET, if there is one.  */
      char *s = strchr (spec, ' ');
      if (s)
	{
	  /* Let the caller perform MAKE quoting.  */
	  defer_opt (OPT_MT, s + 1);
	  *s = '\0';
	}

      /* Command line -MF overrides environment variables and default.  */
      if (!deps_file)
	deps_file = spec;

      deps_append = 1;
    }
}

/* Handle deferred command line switches.  */
static void
handle_deferred_opts (void)
{
  size_t i;

  for (i = 0; i < deferred_count; i++)
    {
      struct deferred_opt *opt = &deferred_opts[i];

      if (opt->code == OPT_MT || opt->code == OPT_MQ)
	cpp_add_dependency_target (parse_in, opt->arg, opt->code == OPT_MQ);
    }
}

/* These settings are appropriate for GCC, but not necessarily so for
   cpplib as a library.  */
static void
sanitize_cpp_opts (void)
{
  /* If we don't know what style of dependencies to output, complain
     if any other dependency switches have been given.  */
  if (deps_seen && cpp_opts->deps.style == DEPS_NONE)
    error ("to generate dependencies you must specify either -M or -MM");

  /* -dM and dependencies suppress normal output; do it here so that
     the last -d[MDN] switch overrides earlier ones.  */
  if (flag_dump_macros == 'M')
    flag_no_output = 1;

  /* Disable -dD, -dN and -dI if normal output is suppressed.  Allow
     -dM since at least glibc relies on -M -dM to work.  */
  if (flag_no_output)
    {
      if (flag_dump_macros != 'M')
	flag_dump_macros = 0;
      flag_dump_includes = 0;
    }

  cpp_opts->unsigned_char = !flag_signed_char;
  cpp_opts->stdc_0_in_system_headers = STDC_0_IN_SYSTEM_HEADERS;

  /* We want -Wno-long-long to override -pedantic -std=non-c99
     and/or -Wtraditional, whatever the ordering.  */
  cpp_opts->warn_long_long
    = warn_long_long && ((!flag_isoc99 && pedantic) || warn_traditional);
}

/* Add include path with a prefix at the front of its name.  */
static void
add_prefixed_path (const char *suffix, size_t chain)
{
  char *path;
  const char *prefix;
  size_t prefix_len, suffix_len;

  suffix_len = strlen (suffix);
  prefix     = iprefix ? iprefix : cpp_GCC_INCLUDE_DIR;
  prefix_len = iprefix ? strlen (iprefix) : cpp_GCC_INCLUDE_DIR_len;

  path = xmalloc (prefix_len + suffix_len + 1);
  memcpy (path, prefix, prefix_len);
  memcpy (path + prefix_len, suffix, suffix_len);
  path[prefix_len + suffix_len] = '\0';

  add_path (path, chain, 0);
}

/* Handle -D, -U, -A, -imacros, and the first -include.  */
static void
finish_options (void)
{
  if (!cpp_opts->preprocessed)
    {
      size_t i;

      cpp_change_file (parse_in, LC_RENAME, _("<built-in>"));
      cpp_init_builtins (parse_in, flag_hosted);
      c_cpp_builtins (parse_in);

      /* We're about to send user input to cpplib, so make it warn for
	 things that we previously (when we sent it internal definitions)
	 told it to not warn.

	 C99 permits implementation-defined characters in identifiers.
	 The documented meaning of -std= is to turn off extensions that
	 conflict with the specified standard, and since a strictly
	 conforming program cannot contain a '$', we do not condition
	 their acceptance on the -std= setting.  */
      cpp_opts->warn_dollars = (cpp_opts->pedantic && !cpp_opts->c99);

      cpp_change_file (parse_in, LC_RENAME, _("<command line>"));
      for (i = 0; i < deferred_count; i++)
	{
	  struct deferred_opt *opt = &deferred_opts[i];

	  if (opt->code == OPT_D)
	    cpp_define (parse_in, opt->arg);
	  else if (opt->code == OPT_U)
	    cpp_undef (parse_in, opt->arg);
	  else if (opt->code == OPT_A)
	    {
	      if (opt->arg[0] == '-')
		cpp_unassert (parse_in, opt->arg + 1);
	      else
		cpp_assert (parse_in, opt->arg);
	    }
	}

      /* Handle -imacros after -D and -U.  */
      for (i = 0; i < deferred_count; i++)
	{
	  struct deferred_opt *opt = &deferred_opts[i];

	  if (opt->code == OPT_imacros
	      && cpp_push_include (parse_in, opt->arg))
	    cpp_scan_nooutput (parse_in);
	}
    }

  push_command_line_include ();
}

/* Give CPP the next file given by -include, if any.  */
static void
push_command_line_include (void)
{
  if (cpp_opts->preprocessed)
    return;

  while (include_cursor < deferred_count)
    {
      struct deferred_opt *opt = &deferred_opts[include_cursor++];

      if (opt->code == OPT_include && cpp_push_include (parse_in, opt->arg))
	return;
    }

  if (include_cursor == deferred_count)
    {
      /* Restore the line map from <command line>.  */
      cpp_change_file (parse_in, LC_RENAME, main_input_filename);
      /* -Wunused-macros should only warn about macros defined hereafter.  */
      cpp_opts->warn_unused_macros = warn_unused_macros;
      include_cursor++;
    }
}

/* File change callback.  Has to handle -include files.  */
static void
cb_file_change (cpp_reader *pfile ATTRIBUTE_UNUSED,
		const struct line_map *new_map)
{
  if (flag_preprocess_only)
    pp_file_change (new_map);
  else
    fe_file_change (new_map);

  if (new_map->reason == LC_LEAVE && MAIN_FILE_P (new_map))
    push_command_line_include ();
}

/* Set the C 89 standard (with 1994 amendments if C94, without GNU
   extensions if ISO).  There is no concept of gnu94.  */
static void
set_std_c89 (int c94, int iso)
{
  cpp_set_lang (parse_in, c94 ? CLK_STDC94: iso ? CLK_STDC89: CLK_GNUC89);
  flag_iso = iso;
  flag_no_asm = iso;
  flag_no_gnu_keywords = iso;
  flag_no_nonansi_builtin = iso;
  flag_noniso_default_format_attributes = !iso;
  flag_isoc94 = c94;
  flag_isoc99 = 0;
  flag_writable_strings = 0;
}

/* Set the C 99 standard (without GNU extensions if ISO).  */
static void
set_std_c99 (int iso)
{
  cpp_set_lang (parse_in, iso ? CLK_STDC99: CLK_GNUC99);
  flag_no_asm = iso;
  flag_no_nonansi_builtin = iso;
  flag_noniso_default_format_attributes = !iso;
  flag_iso = iso;
  flag_isoc99 = 1;
  flag_isoc94 = 1;
  flag_writable_strings = 0;
}

/* Set the C++ 98 standard (without GNU extensions if ISO).  */
static void
set_std_cxx98 (int iso)
{
  cpp_set_lang (parse_in, iso ? CLK_CXX98: CLK_GNUCXX);
  flag_no_gnu_keywords = iso;
  flag_no_nonansi_builtin = iso;
  flag_noniso_default_format_attributes = !iso;
  flag_iso = iso;
}

/* Handle setting implicit to ON.  */
static void
set_Wimplicit (int on)
{
  warn_implicit = on;
  warn_implicit_int = on;
  if (on)
    {
      if (mesg_implicit_function_declaration != 2)
	mesg_implicit_function_declaration = 1;
    }
  else
    mesg_implicit_function_declaration = 0;
}

/* Args to -d specify what to dump.  Silently ignore
   unrecognized options; they may be aimed at toplev.c.  */
static void
handle_OPT_d (const char *arg)
{
  char c;

  while ((c = *arg++) != '\0')
    switch (c)
      {
      case 'M':			/* Dump macros only.  */
      case 'N':			/* Dump names.  */
      case 'D':			/* Dump definitions.  */
	flag_dump_macros = c;
	break;

      case 'I':
	flag_dump_includes = 1;
	break;
      }
}

/* Handle --help output.  */
static void
print_help (void)
{
  /* To keep the lines from getting too long for some compilers, limit
     to about 500 characters (6 lines) per chunk.  */
  fputs (_("\
Switches:\n\
  -include <file>           Include the contents of <file> before other files\n\
  -imacros <file>           Accept definition of macros in <file>\n\
  -iprefix <path>           Specify <path> as a prefix for next two options\n\
  -iwithprefix <dir>        Add <dir> to the end of the system include path\n\
  -iwithprefixbefore <dir>  Add <dir> to the end of the main include path\n\
  -isystem <dir>            Add <dir> to the start of the system include path\n\
"), stdout);
  fputs (_("\
  -idirafter <dir>          Add <dir> to the end of the system include path\n\
  -I <dir>                  Add <dir> to the end of the main include path\n\
  -I-                       Fine-grained include path control; see info docs\n\
  -nostdinc                 Do not search system include directories\n\
                             (dirs specified with -isystem will still be used)\n\
  -nostdinc++               Do not search system include directories for C++\n\
  -o <file>                 Put output into <file>\n\
"), stdout);
  fputs (_("\
  -trigraphs                Support ISO C trigraphs\n\
  -std=<std name>           Specify the conformance standard; one of:\n\
                            gnu89, gnu99, c89, c99, iso9899:1990,\n\
                            iso9899:199409, iso9899:1999, c++98\n\
  -w                        Inhibit warning messages\n\
  -W[no-]trigraphs          Warn if trigraphs are encountered\n\
  -W[no-]comment{s}         Warn if one comment starts inside another\n\
"), stdout);
  fputs (_("\
  -W[no-]traditional        Warn about features not present in traditional C\n\
  -W[no-]undef              Warn if an undefined macro is used by #if\n\
  -W[no-]import             Warn about the use of the #import directive\n\
"), stdout);
  fputs (_("\
  -W[no-]error              Treat all warnings as errors\n\
  -W[no-]system-headers     Do not suppress warnings from system headers\n\
  -W[no-]all                Enable most preprocessor warnings\n\
"), stdout);
  fputs (_("\
  -M                        Generate make dependencies\n\
  -MM                       As -M, but ignore system header files\n\
  -MD                       Generate make dependencies and compile\n\
  -MMD                      As -MD, but ignore system header files\n\
  -MF <file>                Write dependency output to the given file\n\
  -MG                       Treat missing header file as generated files\n\
"), stdout);
  fputs (_("\
  -MP			    Generate phony targets for all headers\n\
  -MQ <target>              Add a MAKE-quoted target\n\
  -MT <target>              Add an unquoted target\n\
"), stdout);
  fputs (_("\
  -D<macro>                 Define a <macro> with string '1' as its value\n\
  -D<macro>=<val>           Define a <macro> with <val> as its value\n\
  -A<question>=<answer>     Assert the <answer> to <question>\n\
  -A-<question>=<answer>    Disable the <answer> to <question>\n\
  -U<macro>                 Undefine <macro> \n\
  -v                        Display the version number\n\
"), stdout);
  fputs (_("\
  -H                        Print the name of header files as they are used\n\
  -C                        Do not discard comments\n\
  -dM                       Display a list of macro definitions active at end\n\
  -dD                       Preserve macro definitions in output\n\
  -dN                       As -dD except that only the names are preserved\n\
  -dI                       Include #include directives in the output\n\
"), stdout);
  fputs (_("\
  -f[no-]preprocessed       Treat the input file as already preprocessed\n\
  -ftabstop=<number>        Distance between tab stops for column reporting\n\
  -isysroot <dir>           Set <dir> to be the system root directory\n\
  -P                        Do not generate #line directives\n\
  -remap                    Remap file names when including files\n\
  --help                    Display this information\n\
"), stdout);
}
