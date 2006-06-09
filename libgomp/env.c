/* Copyright (C) 2005 Free Software Foundation, Inc.
   Contributed by Richard Henderson <rth@redhat.com>.

   This file is part of the GNU OpenMP Library (libgomp).

   Libgomp is free software; you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 2.1 of the License, or
   (at your option) any later version.

   Libgomp is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
   FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
   more details.

   You should have received a copy of the GNU Lesser General Public License 
   along with libgomp; see the file COPYING.LIB.  If not, write to the
   Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
   MA 02110-1301, USA.  */

/* As a special exception, if you link this library with other files, some
   of which are compiled with GCC, to produce an executable, this library
   does not by itself cause the resulting executable to be covered by the
   GNU General Public License.  This exception does not however invalidate
   any other reasons why the executable file might be covered by the GNU
   General Public License.  */

/* This file defines the OpenMP internal control variables, and arranges
   for them to be initialized from environment variables at startup.  */

#include "libgomp.h"
#include "libgomp_f.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>


unsigned long gomp_nthreads_var = 1;
bool gomp_dyn_var = false;
bool gomp_nest_var = false;
enum gomp_schedule_type gomp_run_sched_var = GFS_DYNAMIC;
unsigned long gomp_run_sched_chunk = 1;

/* Parse the OMP_SCHEDULE environment variable.  */

static void
parse_schedule (void)
{
  char *env, *end;

  env = getenv ("OMP_SCHEDULE");
  if (env == NULL)
    return;

  if (strncmp (env, "static", 6) == 0)
    {
      gomp_run_sched_var = GFS_STATIC;
      env += 6;
    }
  else if (strncmp (env, "dynamic", 7) == 0)
    {
      gomp_run_sched_var = GFS_DYNAMIC;
      env += 7;
    }
  else if (strncmp (env, "guided", 6) == 0)
    {
      gomp_run_sched_var = GFS_GUIDED;
      env += 6;
    }
  else
    goto unknown;

  if (*env == '\0')
    return;
  if (*env != ' ' && *env != ',')
    goto unknown;
  while (*env == ' ')
    env++;
  if (*env == '\0')
    return;
  if (*env != ',')
    goto unknown;
  if (*++env == '\0')
    goto invalid;

  gomp_run_sched_chunk = strtoul (env, &end, 10);
  if (*end != '\0')
    goto invalid;
  return;

 unknown:
  gomp_error ("Unknown value for environment variable OMP_SCHEDULE");
  return;

 invalid:
  gomp_error ("Invalid value for chunk size in "
	      "environment variable OMP_SCHEDULE");
  gomp_run_sched_chunk = 1;
  return;
}

/* Parse an unsigned long environment varible.  Return true if one was
   present and it was successfully parsed.  */

static bool
parse_unsigned_long (const char *name, unsigned long *pvalue)
{
  char *env, *end;
  unsigned long value;

  env = getenv (name);
  if (env == NULL)
    return false;

  if (*env == '\0')
    goto invalid;

  value = strtoul (env, &end, 10);
  if (*end != '\0')
    goto invalid;

  *pvalue = value;
  return true;

 invalid:
  gomp_error ("Invalid value for environment variable %s", name);
  return false;
}

/* Parse a boolean value for environment variable NAME and store the 
   result in VALUE.  */

static void
parse_boolean (const char *name, bool *value)
{
  const char *env;

  env = getenv (name);
  if (env == NULL)
    return;

  if (strcmp (env, "true") == 0)
    *value = true;
  else if (strcmp (env, "false") == 0)
    *value = false;
  else
    gomp_error ("Invalid value for environment variable %s", name);
}

static void __attribute__((constructor))
initialize_env (void)
{
  unsigned long stacksize;

  /* Do a compile time check that mkomp_h.pl did good job.  */
  omp_check_defines ();

  parse_schedule ();
  parse_boolean ("OMP_DYNAMIC", &gomp_dyn_var);
  parse_boolean ("OMP_NESTED", &gomp_nest_var);
  if (!parse_unsigned_long ("OMP_NUM_THREADS", &gomp_nthreads_var))
    gomp_init_num_threads ();

  /* Not strictly environment related, but ordering constructors is tricky.  */
  pthread_attr_init (&gomp_thread_attr);
  pthread_attr_setdetachstate (&gomp_thread_attr, PTHREAD_CREATE_DETACHED);

  if (parse_unsigned_long ("OMP_STACKSIZE", &stacksize))
    {
      stacksize *= 1024;
      if (stacksize < PTHREAD_STACK_MIN)
	gomp_error ("Stack size less than minimum of %luk",
		    PTHREAD_STACK_MIN / 1024ul
		    + (PTHREAD_STACK_MIN % 1024 != 0));
      else
	{
	  int err = pthread_attr_setstacksize (&gomp_thread_attr, stacksize);
	  if (err == EINVAL)
	    gomp_error ("Stack size larger than system limit");
	  else if (err != 0)
	    gomp_error ("Stack size change failed: %s", strerror (err));
	}
    }
}


/* The public OpenMP API routines that access these variables.  */

void
omp_set_num_threads (int n)
{
  gomp_nthreads_var = n;
}

void
omp_set_dynamic (int val)
{
  gomp_dyn_var = val;
}

int
omp_get_dynamic (void)
{
  return gomp_dyn_var;
}

void
omp_set_nested (int val)
{
  gomp_nest_var = val;
}

int
omp_get_nested (void)
{
  return gomp_nest_var;
}

ialias (omp_set_dynamic)
ialias (omp_set_nested)
ialias (omp_set_num_threads)
ialias (omp_get_dynamic)
ialias (omp_get_nested)
