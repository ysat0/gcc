// posix.cc -- Helper functions for POSIX-flavored OSs.

/* Copyright (C) 2000, 2001, 2002  Free Software Foundation

   This file is part of libgcj.

This software is copyrighted work licensed under the terms of the
Libgcj License.  Please consult the file "LIBGCJ_LICENSE" for
details.  */

#include <config.h>

#include "posix.h"

#include <errno.h>
#include <signal.h>

#include <jvm.h>
#include <java/lang/Thread.h>
#include <java/io/InterruptedIOException.h>

#if defined (ECOS)
extern "C" unsigned long long _clock (void);
#endif

// gettimeofday implementation.
jlong
_Jv_platform_gettimeofday ()
{
#if defined (HAVE_GETTIMEOFDAY)
  timeval tv;
  gettimeofday (&tv, NULL);
  return tv.tv_sec * 1000 + tv.tv_usec / 1000;
#elif defined (HAVE_TIME)
  return time (NULL) * 1000;
#elif defined (HAVE_FTIME)
  struct timeb t;
  ftime (&t);
  return t.time * 1000 + t.millitm;
#elif defined (ECOS)
  // FIXME.
  return _clock();
#else
  // In the absence of any function, time remains forever fixed.
  return 23000;
#endif
}

// Platform-specific VM initialization.
void
_Jv_platform_initialize (void)
{
#if defined (HAVE_SIGACTION)
  // We only want this on POSIX systems.
  struct sigaction act;
  act.sa_handler = SIG_IGN;
  sigemptyset (&act.sa_mask);
  act.sa_flags = 0;
  sigaction (SIGPIPE, &act, NULL);
#else
  signal (SIGPIPE, SIG_IGN);
#endif
}

static inline void
internal_gettimeofday (struct timeval *result)
{
#if defined (HAVE_GETTIMEOFDAY)
  gettimeofday (result, NULL);
#else
  jlong val = _Jv_platform_gettimeofday ();
  result->tv_sec = val / 1000;
  result->tv_usec = (val % 1000) * 1000;
#endif /* HAVE_GETTIMEOFDAY */
}

// A wrapper for select() which ignores EINTR.
int
_Jv_select (int n, fd_set *readfds, fd_set  *writefds,
	    fd_set *exceptfds, struct timeval *timeout)
{
#ifdef HAVE_SELECT
  // If we have a timeout, compute the absolute ending time.
  struct timeval end, delay;
  if (timeout)
    {
      internal_gettimeofday (&end);
      end.tv_usec += timeout->tv_usec;
      if (end.tv_usec >= 1000000)
	{
	  ++end.tv_sec;
	  end.tv_usec -= 1000000;
	}
      end.tv_sec += timeout->tv_sec;
      delay = *timeout;
    }
  else
    {
      // Placate compiler.
      delay.tv_sec = delay.tv_usec = 0;
    }

  while (1)
    {
      int r = select (n, readfds, writefds, exceptfds,
		      timeout ? &delay : NULL);
      if (r != -1 || errno != EINTR)
	return r;

      // Here we know we got EINTR.
      if (java::lang::Thread::interrupted ())
	throw new java::io::InterruptedIOException (JvNewStringLatin1 ("select interrupted"));

      struct timeval after;
      if (timeout)
	{
	  internal_gettimeofday (&after);
	  // Now compute new timeout argument.
	  delay.tv_usec = end.tv_usec - after.tv_usec;
	  delay.tv_sec = end.tv_sec - after.tv_sec;
	  if (delay.tv_usec < 0)
	    {
	      --delay.tv_sec;
	      delay.tv_usec += 1000000;
	    }
	  if (delay.tv_sec < 0)
	    {
	      // We assume that the user wants a valid select() call
	      // more than precise timing.  So if we get a series of
	      // EINTR we just keep trying with delay 0 until we get a
	      // valid result.
	      delay.tv_sec = 0;
	    }
	}
    }
#else /* HAVE_SELECT */
  return 0;
#endif
}
