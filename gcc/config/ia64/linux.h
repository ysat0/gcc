/* Definitions for ia64-linux target.  */

/* This macro is a C statement to print on `stderr' a string describing the
   particular machine description choice.  */

#define TARGET_VERSION fprintf (stderr, " (IA-64) Linux");

/* This is for -profile to use -lc_p instead of -lc. */
#undef CC1_SPEC
#define CC1_SPEC "%{profile:-p} %{G*}"

/* ??? Maybe this should be in sysv4.h?  */
#define CPP_PREDEFINES "\
-D__ia64 -D__ia64__ -D__linux -D__linux__ -D_LONGLONG -Dlinux -Dunix \
-D__LP64__ -D__ELF__ -Asystem=linux -Acpu=ia64 -Amachine=ia64"

/* ??? ia64 gas doesn't accept standard svr4 assembler options?  */
#undef ASM_SPEC
#define ASM_SPEC "-x %{mconstant-gp} %{mauto-pic}"

/* Need to override linux.h STARTFILE_SPEC, since it has crtbeginT.o in.  */
#undef STARTFILE_SPEC
#define STARTFILE_SPEC \
  "%{!shared: \
     %{pg:gcrt1.o%s} %{!pg:%{p:gcrt1.o%s} \
		       %{!p:%{profile:gcrt1.o%s} \
			 %{!profile:crt1.o%s}}}} \
   crti.o%s %{!shared:crtbegin.o%s} %{shared:crtbeginS.o%s}"

/* Similar to standard Linux, but adding -ffast-math support.  */
#undef  ENDFILE_SPEC
#define ENDFILE_SPEC \
  "%{ffast-math|funsafe-math-optimizations:crtfastmath.o%s} \
   %{!shared:crtend.o%s} %{shared:crtendS.o%s} crtn.o%s"

/* Define this for shared library support because it isn't in the main
   linux.h file.  */

#undef LINK_SPEC
#define LINK_SPEC "\
  %{shared:-shared} \
  %{!shared: \
    %{!static: \
      %{rdynamic:-export-dynamic} \
      %{!dynamic-linker:-dynamic-linker /lib/ld-linux-ia64.so.2}} \
      %{static:-static}}"


#define DONT_USE_BUILTIN_SETJMP
#define JMP_BUF_SIZE  76

/* Output any profiling code before the prologue.  */

#undef PROFILE_BEFORE_PROLOGUE
#define PROFILE_BEFORE_PROLOGUE 1

/* Override linux.h LINK_EH_SPEC definition.
   Signalize that because we have fde-glibc, we don't need all C shared libs
   linked against -lgcc_s.  */
#undef LINK_EH_SPEC
#define LINK_EH_SPEC ""

/* End of linux.h */
