/* Configuration for GNU C-compiler for openVMS/Alpha.
   Copyright (C) 1996, 1997, 2001 Free Software Foundation, Inc.
   Contributed by Klaus Kaempf (kkaempf@progis.de).

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

/* If compiling with DECC, need to fix problem with <stdio.h>
   which defines a macro called FILE_TYPE that breaks "tree.h".
   Fortunately it uses #ifndef to suppress multiple inclusions.
   Three possible cases:
        1) <stdio.h> has already been included -- ours will be no-op;
        2) <stdio.h> will be included after us -- "theirs" will be no-op;
        3) <stdio.h> isn't needed -- including it here shouldn't hurt.
   In all three cases, the problem macro will be removed here.  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __DECC
#undef FILE_TYPE
#endif

#define HOST_WIDE_INT long long
#define HOST_BITS_PER_WIDE_INT 64

/* Override values in stdlib.h since gcc uses __posix_exit */
#undef SUCCESS_EXIT_CODE
#define SUCCESS_EXIT_CODE 0
#undef FATAL_EXIT_CODE
#define FATAL_EXIT_CODE 1
#undef exit
#define exit __posix_exit
void __posix_exit (int);

/* A couple of conditionals for execution machine are controlled here.  */
#ifndef VMS
#define VMS
#endif

#define GCC_INCLUDE_DIR ""
/* Specify the list of include file directories.  */
#define INCLUDE_DEFAULTS		\
{					\
  { "GNU_GXX_INCLUDE:", "G++", 1, 1 },	\
  { "GNU_CC_INCLUDE:", "GCC", 0, 0 },	\
  { ".", 0, 0, 1 },			\
  { 0, 0, 0, 0 }			\
}

/* Define a local equivalent (sort of) for unlink */
#define unlink remove

#define STDC_HEADERS 1

#define HOST_EXECUTABLE_SUFFIX ".exe"
#define HOST_OBJECT_SUFFIX ".obj"

#define DUMPFILE_FORMAT "_%02d_"
