/* Dump a gcov file, for debugging use.
   Copyright (C) 2002 Free Software Foundation, Inc.
   Contributed by Nathan Sidwell <nathan@codesourcery.com>

Gcov is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

Gcov is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Gcov; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "version.h"
#include <getopt.h>
#define IN_GCOV (-1)
#include "gcov-io.h"
#include "gcov-io.c"

static void dump_file PARAMS ((const char *));
static void print_prefix PARAMS ((const char *, unsigned));
static void print_usage PARAMS ((void));
static void print_version PARAMS ((void));
static void tag_function PARAMS ((const char *, unsigned, unsigned));
static void tag_blocks PARAMS ((const char *, unsigned, unsigned));
static void tag_arcs PARAMS ((const char *, unsigned, unsigned));
static void tag_lines PARAMS ((const char *, unsigned, unsigned));
static void tag_counters PARAMS ((const char *, unsigned, unsigned));
static void tag_summary PARAMS ((const char *, unsigned, unsigned));
extern int main PARAMS ((int, char **));

typedef struct tag_format
{
  unsigned tag;
  char const *name;
  void (*proc) (const char *, unsigned, unsigned);
} tag_format_t;

static int flag_dump_contents = 0;

static const struct option options[] =
{
  { "help",                 no_argument,       NULL, 'h' },
  { "version",              no_argument,       NULL, 'v' },
  { "long",                 no_argument,       NULL, 'l' },
};

static const tag_format_t tag_table[] =
{
  {0, "NOP", NULL},
  {0, "UNKNOWN", NULL},
  {0, "COUNTERS", tag_counters},
  {GCOV_TAG_FUNCTION, "FUNCTION", tag_function},
  {GCOV_TAG_BLOCKS, "BLOCKS", tag_blocks},
  {GCOV_TAG_ARCS, "ARCS", tag_arcs},
  {GCOV_TAG_LINES, "LINES", tag_lines},
  {GCOV_TAG_OBJECT_SUMMARY, "OBJECT_SUMMARY", tag_summary},
  {GCOV_TAG_PROGRAM_SUMMARY, "PROGRAM_SUMMARY", tag_summary},
  {0, NULL, NULL}
};

int main (argc, argv)
     int argc ATTRIBUTE_UNUSED;
     char **argv;
{
  int opt;

  while ((opt = getopt_long (argc, argv, "hlv", options, NULL)) != -1)
    {
      switch (opt)
	{
	case 'h':
	  print_usage ();
	  break;
	case 'v':
	  print_version ();
	  break;
	case 'l':
	  flag_dump_contents = 1;
	  break;
	default:
	  fprintf (stderr, "unknown flag `%c'\n", opt);
	}
    }
  
  while (argv[optind])
    dump_file (argv[optind++]);
  return 0;
}

static void
print_usage ()
{
  printf ("Usage: gcov-dump [OPTION] ... gcovfiles\n");
  printf ("Print coverage file contents\n");
  printf ("  -h, --help           Print this help\n");
  printf ("  -v, --version        Print version number\n");
  printf ("  -l, --long           Dump record contents too\n");
}

static void
print_version ()
{
  char v[4];
  unsigned version = GCOV_VERSION;
  unsigned ix;

  for (ix = 4; ix--; version >>= 8)
    v[ix] = version;
  printf ("gcov %.4s (GCC %s)\n", v, version_string);
  printf ("Copyright (C) 2002 Free Software Foundation, Inc.\n");
  printf ("This is free software; see the source for copying conditions.  There is NO\n\
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\n");
}

static void
print_prefix (filename, depth)
     const char *filename;
     unsigned depth;
{
  static const char prefix[] = "    ";
  
  printf ("%s:%.*s", filename, (int) depth, prefix);
}

static void
dump_file (filename)
     const char *filename;
{
  unsigned tags[4];
  unsigned depth = 0;
  
  if (!gcov_open (filename, 1))
    {
      fprintf (stderr, "%s:cannot open\n", filename);
      return;
    }
  
  /* magic */
  {
    unsigned magic = gcov_read_unsigned ();
    unsigned version = gcov_read_unsigned ();
    const char *type = NULL;
    char e[4], v[4], m[4];
    unsigned expected = GCOV_VERSION;
    unsigned ix;
    int different = version != GCOV_VERSION;
    
    if (magic == GCOV_DATA_MAGIC)
      type = "data";
    else if (magic == GCOV_GRAPH_MAGIC)
      type = "graph";
    else
      {
	printf ("%s:not a gcov file\n", filename);
	gcov_close ();
	return;
      }
    for (ix = 4; ix--; expected >>= 8, version >>= 8, magic >>= 8)
      {
	e[ix] = expected;
	v[ix] = version;
	m[ix] = magic;
      }
    
    printf ("%s:%s:magic `%.4s':version `%.4s'\n", filename, type, m, v);
    if (different)
      printf ("%s:warning:current version is `%.4s'\n", filename, e);
  }

  while (!gcov_is_eof ())
    {
      unsigned tag = gcov_read_unsigned ();
      unsigned length = gcov_read_unsigned ();
      unsigned long base = gcov_position ();
      tag_format_t const *format;
      unsigned tag_depth;
      int error;
      
      if (!tag)
	tag_depth = depth;
      else
	{
	  unsigned mask = GCOV_TAG_MASK (tag) >> 1;
	  
	  for (tag_depth = 4; mask; mask >>= 8)
	    {
	      if ((mask & 0xff) != 0xff)
		{
		  printf ("%s:tag `%08x' is invalid\n", filename, tag);
		  break;
		}
	      tag_depth--;
	    }
	}
      for (format = tag_table; format->name; format++)
	if (format->tag == tag)
	  goto found;
      format = &tag_table[GCOV_TAG_IS_COUNTER (tag) ? 2 : 1];
    found:;
      if (tag)
	{
	  if (depth && depth < tag_depth)
	    {
	      if (!GCOV_TAG_IS_SUBTAG (tags[depth - 1], tag))
		printf ("%s:tag `%08x' is incorrectly nested\n",
			filename, tag);
	    }
	  depth = tag_depth;
	  tags[depth - 1] = tag;
	}
      
      print_prefix (filename, tag_depth);
      printf ("%08x:%4u:%s", tag, length, format->name);
      if (format->proc)
	(*format->proc) (filename, tag, length);
      
      printf ("\n");
      if (flag_dump_contents && format->proc)
	{
	  unsigned long actual_length = gcov_position () - base;
	  
	  if (actual_length > length)
	    printf ("%s:record size mismatch %lu bytes overread\n",
		    filename, actual_length - length);
	  else if (length > actual_length)
	    printf ("%s:record size mismatch %lu bytes unread\n",
		    filename, length - actual_length);
	}
      gcov_sync (base, length);
      if ((error = gcov_is_error ()))
	{
	  printf (error < 0 ? "%s:counter overflow at %lu\n" :
		  "%s:read error at %lu\n", filename,
		  (long unsigned) gcov_position ());
	  break;
	}
    }
  gcov_close ();
}

static void
tag_function (filename, tag, length)
     const char *filename ATTRIBUTE_UNUSED;
     unsigned tag ATTRIBUTE_UNUSED;
     unsigned length ATTRIBUTE_UNUSED;
{
  unsigned long pos = gcov_position ();
  
  printf (" ident=%u", gcov_read_unsigned ());
  printf (", checksum=0x%08x", gcov_read_unsigned ());

  if (gcov_position () - pos < length)
    {
      const char *name;
      
      name = gcov_read_string ();
      printf (", `%s'", name ? name : "NULL");
      name = gcov_read_string ();
      printf (" %s", name ? name : "NULL");
      printf (":%u", gcov_read_unsigned ());
    }
}

static void
tag_blocks (filename, tag, length)
     const char *filename ATTRIBUTE_UNUSED;
     unsigned tag ATTRIBUTE_UNUSED;
     unsigned length ATTRIBUTE_UNUSED;
{
  unsigned n_blocks = length / 4;
  
  printf (" %u blocks", n_blocks);

  if (flag_dump_contents)
    {
      unsigned ix;

      for (ix = 0; ix != n_blocks; ix++)
	{
	  if (!(ix & 7))
	    printf ("\n%s:\t\t%u", filename, ix);
	  printf (" %04x", gcov_read_unsigned ());
	}
    }
}

static void
tag_arcs (filename, tag, length)
     const char *filename ATTRIBUTE_UNUSED;
     unsigned tag ATTRIBUTE_UNUSED;
     unsigned length ATTRIBUTE_UNUSED;
{
  unsigned n_arcs = (length - 4) / 8;

  printf (" %u arcs", n_arcs);
  if (flag_dump_contents)
    {
      unsigned ix;
      unsigned blockno = gcov_read_unsigned ();

      for (ix = 0; ix != n_arcs; ix++)
	{
	  unsigned dst = gcov_read_unsigned ();
	  unsigned flags = gcov_read_unsigned ();
	  
	  if (!(ix & 3))
	    printf ("\n%s:\tblock %u:", filename, blockno);
	  printf (" %u:%04x", dst, flags);
	}
    }
}

static void
tag_lines (filename, tag, length)
     const char *filename ATTRIBUTE_UNUSED;
     unsigned tag ATTRIBUTE_UNUSED;
     unsigned length ATTRIBUTE_UNUSED;
{
  if (flag_dump_contents)
    {
      unsigned blockno = gcov_read_unsigned ();
      char const *sep = NULL;

      while (1)
	{
	  const char *source = NULL;
	  unsigned lineno = gcov_read_unsigned ();
	  
	  if (!lineno)
	    {
	      source = gcov_read_string ();
	      if (!source)
		break;
	      sep = NULL;
	    }
	  
	  if (!sep)
	    {
	      printf ("\n%s:\tblock %u:", filename, blockno);
	      sep = "";
	    }
	  if (lineno)
	    {
	      printf ("%s%u", sep, lineno);
	      sep = ", ";
	    }
	  else
	    {
	      printf ("%s`%s'", sep, source);
	      sep = ":";
	    }
	}
    }
}

static void
tag_counters (filename, tag, length)
     const char *filename ATTRIBUTE_UNUSED;
     unsigned tag ATTRIBUTE_UNUSED;
     unsigned length ATTRIBUTE_UNUSED;
{
  static const char *const counter_names[] = GCOV_COUNTER_NAMES;
  unsigned n_counts = length / 8;
  
  printf (" %s %u counts",
	  counter_names[GCOV_COUNTER_FOR_TAG (tag)], n_counts);
  if (flag_dump_contents)
    {
      unsigned ix;

      for (ix = 0; ix != n_counts; ix++)
	{
	  gcov_type count = gcov_read_counter ();
	  
	  if (!(ix & 7))
	    printf ("\n%s:\t\t%u", filename, ix);
	  printf (" ");
	  printf (HOST_WIDEST_INT_PRINT_DEC, count);
	}
    }
}

static void
tag_summary (filename, tag, length)
     const char *filename ATTRIBUTE_UNUSED;
     unsigned tag ATTRIBUTE_UNUSED;
     unsigned length ATTRIBUTE_UNUSED;
{
  struct gcov_summary summary;
  unsigned ix;
  
  gcov_read_summary (&summary);
  printf (" checksum=0x%08x", summary.checksum);
  
  for (ix = 0; ix != GCOV_COUNTERS; ix++)
    {
      printf ("\n%sL\t\tcounts=%u, runs=%u", filename,
	      summary.ctrs[ix].num, summary.ctrs[ix].runs);
      
      printf (", sum_all=" HOST_WIDEST_INT_PRINT_DEC,
	      (HOST_WIDEST_INT)summary.ctrs[ix].sum_all);
      printf (", run_max=" HOST_WIDEST_INT_PRINT_DEC,
	      (HOST_WIDEST_INT)summary.ctrs[ix].run_max);
      printf (", sum_max=" HOST_WIDEST_INT_PRINT_DEC,
	      (HOST_WIDEST_INT)summary.ctrs[ix].sum_max);
    }
}
