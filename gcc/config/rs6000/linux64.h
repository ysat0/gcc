/* Definitions of target machine for GNU compiler,
   for 64 bit PowerPC linux.
   Copyright (C) 2000, 2001, 2002, 2003 Free Software Foundation, Inc.

   This file is part of GCC.

   GCC is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published
   by the Free Software Foundation; either version 2, or (at your
   option) any later version.

   GCC is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License
   along with GCC; see the file COPYING.  If not, write to the
   Free Software Foundation, 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA.  */

/* Yes!  We are AIX! Err. Wait. We're Linux!. No, wait, we're a
  combo of both!*/
#undef  DEFAULT_ABI
#define DEFAULT_ABI ABI_AIX

#undef  TARGET_AIX
#define TARGET_AIX 1

#undef  TARGET_DEFAULT
#define TARGET_DEFAULT \
  (MASK_POWERPC | MASK_POWERPC64 | MASK_64BIT | MASK_NEW_MNEMONICS)

#undef  PROCESSOR_DEFAULT
#define PROCESSOR_DEFAULT PROCESSOR_PPC630
#undef  PROCESSOR_DEFAULT64
#define PROCESSOR_DEFAULT64 PROCESSOR_PPC630

#undef  ASM_DEFAULT_SPEC
#define ASM_DEFAULT_SPEC "-mppc64"

#undef	ASM_SPEC
#define	ASM_SPEC "%{.s: %{mregnames} %{mno-regnames}} \
%{.S: %{mregnames} %{mno-regnames}} \
%{mlittle} %{mlittle-endian} %{mbig} %{mbig-endian} \
%{v:-V} %{Qy:} %{!Qn:-Qy} -a64 %(asm_cpu) %{Wa,*:%*}"

/* This is always a 64 bit compiler.  */
#undef	TARGET_64BIT
#define	TARGET_64BIT		1

/* 64-bit PowerPC Linux always has a TOC.  */
#undef  TARGET_NO_TOC
#define TARGET_NO_TOC		0
#undef  TARGET_TOC
#define	TARGET_TOC		1

/* Some things from sysv4.h we don't do.  */
#undef	TARGET_RELOCATABLE
#define	TARGET_RELOCATABLE	0
#undef	TARGET_EABI
#define	TARGET_EABI		0
#undef	TARGET_PROTOTYPE
#define	TARGET_PROTOTYPE	0

/* Reuse sysv4 mask bits we made available above.  */
#define	MASK_PROFILE_KERNEL	0x08000000

/* Non-standard profiling for kernels, which just saves LR then calls
   _mcount without worrying about arg saves.  The idea is to change
   the function prologue as little as possible as it isn't easy to
   account for arg save/restore code added just for _mcount.  */
#define TARGET_PROFILE_KERNEL	(target_flags & MASK_PROFILE_KERNEL)

/* Override sysv4.h.  */
#undef	SUBTARGET_SWITCHES
#define SUBTARGET_SWITCHES						\
  {"bit-align",	-MASK_NO_BITFIELD_TYPE,					\
    N_("Align to the base type of the bit-field") },			\
  {"no-bit-align",	 MASK_NO_BITFIELD_TYPE,				\
    N_("Don't align to the base type of the bit-field") },		\
  {"strict-align",	 MASK_STRICT_ALIGN,				\
    N_("Don't assume that unaligned accesses are handled by the system") }, \
  {"no-strict-align",	-MASK_STRICT_ALIGN,				\
    N_("Assume that unaligned accesses are handled by the system") },	\
  {"little-endian",	 MASK_LITTLE_ENDIAN,				\
    N_("Produce little endian code") },					\
  {"little",		 MASK_LITTLE_ENDIAN,				\
    N_("Produce little endian code") },					\
  {"big-endian",	-MASK_LITTLE_ENDIAN,				\
    N_("Produce big endian code") },					\
  {"big",		-MASK_LITTLE_ENDIAN,				\
    N_("Produce big endian code") },					\
  {"bit-word",		-MASK_NO_BITFIELD_WORD,				\
    N_("Allow bit-fields to cross word boundaries") },			\
  {"no-bit-word",	 MASK_NO_BITFIELD_WORD,				\
    N_("Do not allow bit-fields to cross word boundaries") },		\
  {"regnames",		 MASK_REGNAMES,					\
    N_("Use alternate register names") },				\
  {"no-regnames",	-MASK_REGNAMES,					\
    N_("Don't use alternate register names") },				\
  {"profile-kernel",	 MASK_PROFILE_KERNEL,				\
   N_("Call mcount for profiling before a function prologue") },	\
  {"no-profile-kernel",	-MASK_PROFILE_KERNEL,				\
   N_("Call mcount for profiling after a function prologue") },

#undef	SUBTARGET_OPTIONS
#define	SUBTARGET_OPTIONS

#undef	SUBTARGET_OVERRIDE_OPTIONS
#define	SUBTARGET_OVERRIDE_OPTIONS {}

/* We use glibc _mcount for profiling.  */
#define NO_PROFILE_COUNTERS 1
#define PROFILE_HOOK(LABEL) output_profile_hook (LABEL)

/* We don't need to generate entries in .fixup.  */
#undef RELOCATABLE_NEEDS_FIXUP

#define USER_LABEL_PREFIX  ""

/* This now supports a natural alignment mode. */
/* AIX word-aligns FP doubles but doubleword-aligns 64-bit ints.  */
#undef  ADJUST_FIELD_ALIGN
#define ADJUST_FIELD_ALIGN(FIELD, COMPUTED) \
  (TARGET_ALIGN_NATURAL ? (COMPUTED) : \
  (TYPE_MODE (TREE_CODE (TREE_TYPE (FIELD)) == ARRAY_TYPE \
	      ? get_inner_array_type (FIELD) \
	      : TREE_TYPE (FIELD)) == DFmode \
   ? MIN ((COMPUTED), 32) : (COMPUTED)))

/* AIX increases natural record alignment to doubleword if the first
   field is an FP double while the FP fields remain word aligned.  */
#undef  ROUND_TYPE_ALIGN
#define ROUND_TYPE_ALIGN(STRUCT, COMPUTED, SPECIFIED)	\
  ((TREE_CODE (STRUCT) == RECORD_TYPE			\
    || TREE_CODE (STRUCT) == UNION_TYPE			\
    || TREE_CODE (STRUCT) == QUAL_UNION_TYPE)		\
   && TYPE_FIELDS (STRUCT) != 0				\
   && TARGET_ALIGN_NATURAL == 0                         \
   && DECL_MODE (TYPE_FIELDS (STRUCT)) == DFmode	\
   ? MAX (MAX ((COMPUTED), (SPECIFIED)), 64)		\
   : MAX ((COMPUTED), (SPECIFIED)))

/* Indicate that jump tables go in the text section.  */
#undef  JUMP_TABLES_IN_TEXT_SECTION
#define JUMP_TABLES_IN_TEXT_SECTION 1

/* 64-bit PowerPC Linux always has GPR13 fixed.  */
#define FIXED_R13		1

/* __throw will restore its own return address to be the same as the
   return address of the function that the throw is being made to.
   This is unfortunate, because we want to check the original
   return address to see if we need to restore the TOC.
   So we have to squirrel it away with this.  */
#define SETUP_FRAME_ADDRESSES() rs6000_aix_emit_builtin_unwind_init ()

/* Override svr4.h  */
#undef MD_EXEC_PREFIX
#undef MD_STARTFILE_PREFIX

/* Override sysv4.h  */
#undef	CPP_SYSV_SPEC
#define	CPP_SYSV_SPEC ""

#undef  TARGET_OS_CPP_BUILTINS
#define TARGET_OS_CPP_BUILTINS()            \
  do                                        \
    {                                       \
      builtin_define ("__PPC__");           \
      builtin_define ("__PPC64__");         \
      builtin_define ("__powerpc__");       \
      builtin_define ("__powerpc64__");     \
      builtin_define ("__PIC__");           \
      builtin_assert ("cpu=powerpc64");     \
      builtin_assert ("machine=powerpc64"); \
    }                                       \
  while (0)

#undef  CPP_OS_DEFAULT_SPEC
#define CPP_OS_DEFAULT_SPEC "%(cpp_os_linux)"

/* The GNU C++ standard library currently requires _GNU_SOURCE being
   defined on glibc-based systems. This temporary hack accomplishes this,
   it should go away as soon as libstdc++-v3 has a real fix.  */
#undef  CPLUSPLUS_CPP_SPEC
#define CPLUSPLUS_CPP_SPEC "-D_GNU_SOURCE %(cpp)"

#undef  LINK_SHLIB_SPEC
#define LINK_SHLIB_SPEC "%{shared:-shared} %{!shared: %{static:-static}}"

#undef  LIB_DEFAULT_SPEC
#define LIB_DEFAULT_SPEC "%(lib_linux)"

#undef  STARTFILE_DEFAULT_SPEC
#define STARTFILE_DEFAULT_SPEC "%(startfile_linux)"

#undef	ENDFILE_DEFAULT_SPEC
#define ENDFILE_DEFAULT_SPEC "%(endfile_linux)"

#undef	LINK_START_DEFAULT_SPEC
#define LINK_START_DEFAULT_SPEC "%(link_start_linux)"

#undef	LINK_OS_DEFAULT_SPEC
#define LINK_OS_DEFAULT_SPEC "%(link_os_linux)"

#undef  LINK_OS_LINUX_SPEC
#define LINK_OS_LINUX_SPEC "-m elf64ppc %{!shared: %{!static: \
  %{rdynamic:-export-dynamic} \
  %{!dynamic-linker:-dynamic-linker /lib64/ld64.so.1}}}"

#ifdef NATIVE_CROSS
#define STARTFILE_PREFIX_SPEC "/usr/local/lib64/ /lib64/ /usr/lib64/"
#endif

#undef  STARTFILE_LINUX_SPEC
#define STARTFILE_LINUX_SPEC "\
%{!shared: %{pg:gcrt1.o%s} %{!pg:%{p:gcrt1.o%s} %{!p:crt1.o%s}}} crti.o%s \
%{static:crtbeginT.o%s} \
%{!static:%{!shared:crtbegin.o%s} %{shared:crtbeginS.o%s}}"

#undef  ENDFILE_LINUX_SPEC
#define ENDFILE_LINUX_SPEC "\
%{!shared:crtend.o%s} %{shared:crtendS.o%s} crtn.o%s"

#undef  TOC_SECTION_ASM_OP
#define TOC_SECTION_ASM_OP "\t.section\t\".toc\",\"aw\""

#undef  MINIMAL_TOC_SECTION_ASM_OP
#define MINIMAL_TOC_SECTION_ASM_OP "\t.section\t\".toc1\",\"aw\""

#undef  TARGET_VERSION
#define TARGET_VERSION fprintf (stderr, " (PowerPC64 GNU/Linux)");

/* Must be at least as big as our pointer type.  */
#undef  SIZE_TYPE
#define SIZE_TYPE "long unsigned int"

#undef  PTRDIFF_TYPE
#define PTRDIFF_TYPE "long int"

#undef  WCHAR_TYPE
#define WCHAR_TYPE "int"
#undef  WCHAR_TYPE_SIZE
#define WCHAR_TYPE_SIZE 32

/* Override rs6000.h definition.  */
#undef  ASM_APP_ON
#define ASM_APP_ON "#APP\n"

/* Override rs6000.h definition.  */
#undef  ASM_APP_OFF
#define ASM_APP_OFF "#NO_APP\n"

/* PowerPC no-op instruction.  */
#undef  RS6000_CALL_GLUE
#define RS6000_CALL_GLUE "nop"

#undef  RS6000_MCOUNT
#define RS6000_MCOUNT "_mcount"

/* FP save and restore routines.  */
#undef  SAVE_FP_PREFIX
#define SAVE_FP_PREFIX "._savef"
#undef  SAVE_FP_SUFFIX
#define SAVE_FP_SUFFIX ""
#undef  RESTORE_FP_PREFIX
#define RESTORE_FP_PREFIX "._restf"
#undef  RESTORE_FP_SUFFIX
#define RESTORE_FP_SUFFIX ""

/* Dwarf2 debugging.  */
#undef  PREFERRED_DEBUGGING_TYPE
#define PREFERRED_DEBUGGING_TYPE DWARF2_DEBUG

#undef  ASM_DECLARE_FUNCTION_NAME
#define ASM_DECLARE_FUNCTION_NAME(FILE, NAME, DECL)			\
  do									\
    {									\
      fputs ("\t.section\t\".opd\",\"aw\"\n\t.align 3\n", (FILE));	\
      ASM_OUTPUT_LABEL ((FILE), (NAME));				\
      fputs (DOUBLE_INT_ASM_OP, (FILE));				\
      putc ('.', (FILE));						\
      assemble_name ((FILE), (NAME));					\
      fputs (",.TOC.@tocbase,0\n\t.previous\n\t.size\t", (FILE));	\
      assemble_name ((FILE), (NAME));					\
      fputs (",24\n\t.type\t.", (FILE));				\
      assemble_name ((FILE), (NAME));					\
      fputs (",@function\n", (FILE));					\
      if (TREE_PUBLIC (DECL) && ! DECL_WEAK (DECL))			\
        {								\
	  fputs ("\t.globl\t.", (FILE));				\
	  assemble_name ((FILE), (NAME));				\
	  putc ('\n', (FILE));						\
        }								\
      ASM_DECLARE_RESULT ((FILE), DECL_RESULT (DECL));			\
      putc ('.', (FILE));						\
      ASM_OUTPUT_LABEL ((FILE), (NAME));				\
    }									\
  while (0)

/* This is how to declare the size of a function.  */
#undef	ASM_DECLARE_FUNCTION_SIZE
#define	ASM_DECLARE_FUNCTION_SIZE(FILE, FNAME, DECL)			\
  do									\
    {									\
      if (!flag_inhibit_size_directive)					\
	{								\
	  fputs ("\t.size\t.", (FILE));					\
	  assemble_name ((FILE), (FNAME));				\
	  fputs (",.-.", (FILE));					\
	  assemble_name ((FILE), (FNAME));				\
	  putc ('\n', (FILE));						\
	}								\
    }									\
  while (0)

/* Return nonzero if this entry is to be written into the constant
   pool in a special way.  We do so if this is a SYMBOL_REF, LABEL_REF
   or a CONST containing one of them.  If -mfp-in-toc (the default),
   we also do this for floating-point constants.  We actually can only
   do this if the FP formats of the target and host machines are the
   same, but we can't check that since not every file that uses
   GO_IF_LEGITIMATE_ADDRESS_P includes real.h.  We also do this when
   we can write the entry into the TOC and the entry is not larger
   than a TOC entry.  */

#undef  ASM_OUTPUT_SPECIAL_POOL_ENTRY_P
#define ASM_OUTPUT_SPECIAL_POOL_ENTRY_P(X, MODE)			\
  (TARGET_TOC								\
   && (GET_CODE (X) == SYMBOL_REF					\
       || (GET_CODE (X) == CONST && GET_CODE (XEXP (X, 0)) == PLUS	\
	   && GET_CODE (XEXP (XEXP (X, 0), 0)) == SYMBOL_REF)		\
       || GET_CODE (X) == LABEL_REF					\
       || (GET_CODE (X) == CONST_INT 					\
	   && GET_MODE_BITSIZE (MODE) <= GET_MODE_BITSIZE (Pmode))	\
       || (GET_CODE (X) == CONST_DOUBLE					\
	   && (TARGET_POWERPC64						\
	       || TARGET_MINIMAL_TOC					\
	       || (GET_MODE_CLASS (GET_MODE (X)) == MODE_FLOAT		\
		   && ! TARGET_NO_FP_IN_TOC)))))

/* This is the same as the dbxelf.h version, except that we need to
   use the function code label, not the function descriptor.  */
#undef	ASM_OUTPUT_SOURCE_LINE
#define	ASM_OUTPUT_SOURCE_LINE(FILE, LINE)				\
do									\
  {									\
    static int sym_lineno = 1;						\
    char temp[256];							\
    ASM_GENERATE_INTERNAL_LABEL (temp, "LM", sym_lineno);		\
    fprintf (FILE, "\t.stabn 68,0,%d,", LINE);				\
    assemble_name (FILE, temp);						\
    fputs ("-.", FILE);							\
    assemble_name (FILE,						\
		   XSTR (XEXP (DECL_RTL (current_function_decl), 0), 0));\
    putc ('\n', FILE);							\
    (*targetm.asm_out.internal_label) (FILE, "LM", sym_lineno);		\
    sym_lineno += 1;							\
  }									\
while (0)

/* Similarly, we want the function code label here.  */
#define DBX_OUTPUT_BRAC(FILE, NAME, BRAC) \
  do									\
    {									\
      const char *flab;							\
      fprintf (FILE, "%s%d,0,0,", ASM_STABN_OP, BRAC);			\
      assemble_name (FILE, NAME);					\
      putc ('-', FILE);							\
      if (current_function_func_begin_label != NULL_TREE)		\
	flab = IDENTIFIER_POINTER (current_function_func_begin_label);	\
      else								\
	{								\
	  putc ('.', FILE);						\
	  flab = XSTR (XEXP (DECL_RTL (current_function_decl), 0), 0);	\
	}								\
      assemble_name (FILE, flab);					\
      putc ('\n', FILE);						\
    }									\
  while (0)

#define DBX_OUTPUT_LBRAC(FILE, NAME) DBX_OUTPUT_BRAC (FILE, NAME, N_LBRAC)
#define DBX_OUTPUT_RBRAC(FILE, NAME) DBX_OUTPUT_BRAC (FILE, NAME, N_RBRAC)

/* Another case where we want the dot name.  */
#define	DBX_OUTPUT_NFUN(FILE, LSCOPE, DECL)				\
  do									\
    {									\
      fprintf (FILE, "%s\"\",%d,0,0,", ASM_STABS_OP, N_FUN);		\
      assemble_name (FILE, LSCOPE);					\
      fputs ("-.", FILE);						\
      assemble_name (FILE, XSTR (XEXP (DECL_RTL (DECL), 0), 0));	\
      putc ('\n', FILE);						\
    }									\
  while (0)

/* Override sysv4.h as these are ABI_V4 only.  */
#undef	ASM_OUTPUT_REG_PUSH
#undef	ASM_OUTPUT_REG_POP

/* Select a format to encode pointers in exception handling data.  CODE
   is 0 for data, 1 for code labels, 2 for function pointers.  GLOBAL is
   true if the symbol may be affected by dynamic relocations.  */
#undef	ASM_PREFERRED_EH_DATA_FORMAT
#define	ASM_PREFERRED_EH_DATA_FORMAT(CODE, GLOBAL) \
  (((GLOBAL) ? DW_EH_PE_indirect : 0) | DW_EH_PE_pcrel | DW_EH_PE_udata8)
