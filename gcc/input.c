/* Data and functions related to line maps and input files.
   Copyright (C) 2004, 2007, 2008, 2009, 2010, 2011
   Free Software Foundation, Inc.

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
#include "intl.h"
#include "input.h"

/* Current position in real source file.  */

location_t input_location;

struct line_maps *line_table;

/* Expand the source location LOC into a human readable location.  If
   LOC resolves to a builtin location, the file name of the readable
   location is set to the string "<built-in>". If EXPANSION_POINT_P is
   TRUE and LOC is virtual, then it is resolved to the expansion
   point of the involved macro.  Otherwise, it is resolved to the
   spelling location of the token.  */

static expanded_location
expand_location_1 (source_location loc,
		   bool expansion_point_p)
{
  expanded_location xloc;
  const struct line_map *map;

  loc = linemap_resolve_location (line_table, loc,
				  expansion_point_p
				  ? LRK_MACRO_EXPANSION_POINT
				  : LRK_SPELLING_LOCATION, &map);
  xloc = linemap_expand_location (line_table, map, loc);

  if (loc <= BUILTINS_LOCATION)
    xloc.file = loc == UNKNOWN_LOCATION ? NULL : _("<built-in>");

  return xloc;
}

/* Reads one line from file into a static buffer.  */
static const char *
read_line (FILE *file)
{
  static char *string;
  static size_t string_len;
  size_t pos = 0;
  char *ptr;

  if (!string_len)
    {
      string_len = 200;
      string = XNEWVEC (char, string_len);
    }

  while ((ptr = fgets (string + pos, string_len - pos, file)))
    {
      size_t len = strlen (string + pos);

      if (string[pos + len - 1] == '\n')
	{
	  string[pos + len - 1] = 0;
	  return string;
	}
      pos += len;
      ptr = XNEWVEC (char, string_len * 2);
      if (ptr)
	{
	  memcpy (ptr, string, pos);
	  string = ptr;
	  string_len += 2;
	}
      else
	pos = 0;
    }
      
  return pos ? string : NULL;
}

/* Return the physical source line that corresponds to xloc in a
   buffer that is statically allocated.  The newline is replaced by
   the null character.  */

const char *
location_get_source_line(expanded_location xloc)
{
  const char *buffer;
  int lines = 1;
  FILE *stream = xloc.file ? fopen (xloc.file, "r") : NULL;
  if (!stream)
    return NULL;

  while ((buffer = read_line (stream)) && lines < xloc.line)
    lines++;

  fclose (stream);
  return buffer;
}

/* Expand the source location LOC into a human readable location.  If
   LOC is virtual, it resolves to the expansion point of the involved
   macro.  If LOC resolves to a builtin location, the file name of the
   readable location is set to the string "<built-in>".  */

expanded_location
expand_location (source_location loc)
{
  return expand_location_1 (loc, /*expansion_point_p=*/true);
}

/* Expand the source location LOC into a human readable location.  If
   LOC is virtual, it resolves to the expansion location of the
   relevant macro.  If LOC resolves to a builtin location, the file
   name of the readable location is set to the string
   "<built-in>".  */

expanded_location
expand_location_to_spelling_point (source_location loc)
{
  return expand_location_1 (loc, /*expansion_piont_p=*/false);
}


#define ONE_K 1024
#define ONE_M (ONE_K * ONE_K)

/* Display a number as an integer multiple of either:
   - 1024, if said integer is >= to 10 K (in base 2)
   - 1024 * 1024, if said integer is >= 10 M in (base 2)
 */
#define SCALE(x) ((unsigned long) ((x) < 10 * ONE_K \
		  ? (x) \
		  : ((x) < 10 * ONE_M \
		     ? (x) / ONE_K \
		     : (x) / ONE_M)))

/* For a given integer, display either:
   - the character 'k', if the number is higher than 10 K (in base 2)
     but strictly lower than 10 M (in base 2)
   - the character 'M' if the number is higher than 10 M (in base2)
   - the charcter ' ' if the number is strictly lower  than 10 K  */
#define STAT_LABEL(x) ((x) < 10 * ONE_K ? ' ' : ((x) < 10 * ONE_M ? 'k' : 'M'))

/* Display an integer amount as multiple of 1K or 1M (in base 2).
   Display the correct unit (either k, M, or ' ') after the amout, as
   well.  */
#define FORMAT_AMOUNT(size) SCALE (size), STAT_LABEL (size)

/* Dump statistics to stderr about the memory usage of the line_table
   set of line maps.  This also displays some statistics about macro
   expansion.  */

void
dump_line_table_statistics (void)
{
  struct linemap_stats s;
  long total_used_map_size,
    macro_maps_size,
    total_allocated_map_size;

  memset (&s, 0, sizeof (s));

  linemap_get_statistics (line_table, &s);

  macro_maps_size = s.macro_maps_used_size
    + s.macro_maps_locations_size;

  total_allocated_map_size = s.ordinary_maps_allocated_size
    + s.macro_maps_allocated_size
    + s.macro_maps_locations_size;

  total_used_map_size = s.ordinary_maps_used_size
    + s.macro_maps_used_size
    + s.macro_maps_locations_size;

  fprintf (stderr, "Number of expanded macros:                     %5ld\n",
           s.num_expanded_macros);
  if (s.num_expanded_macros != 0)
    fprintf (stderr, "Average number of tokens per macro expansion:  %5ld\n",
             s.num_macro_tokens / s.num_expanded_macros);
  fprintf (stderr,
           "\nLine Table allocations during the "
           "compilation process\n");
  fprintf (stderr, "Number of ordinary maps used:        %5ld%c\n",
           SCALE (s.num_ordinary_maps_used),
           STAT_LABEL (s.num_ordinary_maps_used));
  fprintf (stderr, "Ordinary map used size:              %5ld%c\n",
           SCALE (s.ordinary_maps_used_size),
           STAT_LABEL (s.ordinary_maps_used_size));
  fprintf (stderr, "Number of ordinary maps allocated:   %5ld%c\n",
           SCALE (s.num_ordinary_maps_allocated),
           STAT_LABEL (s.num_ordinary_maps_allocated));
  fprintf (stderr, "Ordinary maps allocated size:        %5ld%c\n",
           SCALE (s.ordinary_maps_allocated_size),
           STAT_LABEL (s.ordinary_maps_allocated_size));
  fprintf (stderr, "Number of macro maps used:           %5ld%c\n",
           SCALE (s.num_macro_maps_used),
           STAT_LABEL (s.num_macro_maps_used));
  fprintf (stderr, "Macro maps used size:                %5ld%c\n",
           SCALE (s.macro_maps_used_size),
           STAT_LABEL (s.macro_maps_used_size));
  fprintf (stderr, "Macro maps locations size:           %5ld%c\n",
           SCALE (s.macro_maps_locations_size),
           STAT_LABEL (s.macro_maps_locations_size));
  fprintf (stderr, "Macro maps size:                     %5ld%c\n",
           SCALE (macro_maps_size),
           STAT_LABEL (macro_maps_size));
  fprintf (stderr, "Duplicated maps locations size:      %5ld%c\n",
           SCALE (s.duplicated_macro_maps_locations_size),
           STAT_LABEL (s.duplicated_macro_maps_locations_size));
  fprintf (stderr, "Total allocated maps size:           %5ld%c\n",
           SCALE (total_allocated_map_size),
           STAT_LABEL (total_allocated_map_size));
  fprintf (stderr, "Total used maps size:                %5ld%c\n",
           SCALE (total_used_map_size),
           STAT_LABEL (total_used_map_size));
  fprintf (stderr, "\n");
}
