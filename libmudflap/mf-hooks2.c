/* Mudflap: narrow-pointer bounds-checking by tree rewriting.
   Copyright (C) 2002, 2003 Free Software Foundation, Inc.
   Contributed by Frank Ch. Eigler <fche@redhat.com>
   and Graydon Hoare <graydon@redhat.com>

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

In addition to the permissions in the GNU General Public License, the
Free Software Foundation gives you unlimited permission to link the
compiled version of this file into combinations with other programs,
and to distribute those combinations without any restriction coming
from the use of this file.  (The General Public License restrictions
do apply in other respects; for example, they cover modification of
the file, and distribution when not linked into a combine
executable.)

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */


#include "config.h"

#ifndef HAVE_SOCKLEN_T
#define socklen_t int
#endif

/* These attempt to coax various unix flavours to declare all our
   needed tidbits in the system headers.  */
#if !defined(__FreeBSD__) && !defined(__APPLE__)
#define _POSIX_SOURCE
#endif /* Some BSDs break <sys/socket.h> if this is defined. */
#define _GNU_SOURCE 
#define _XOPEN_SOURCE
#define _BSD_TYPES
#define __EXTENSIONS__
#define _ALL_SOURCE
#define _LARGE_FILE_API
#define _XOPEN_SOURCE_EXTENDED 1

#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <time.h>

#include "mf-runtime.h"
#include "mf-impl.h"

#ifdef _MUDFLAP
#error "Do not compile this file with -fmudflap!"
#endif


/* A bunch of independent stdlib/unistd hook functions, all
   intercepted by mf-runtime.h macros.  */

#ifdef __FreeBSD__
#undef WRAP_memrchr
#undef WRAP_memmem
#include <dlfcn.h>
static inline size_t (strnlen) (const char* str, size_t n)
{
  const char *s;

  for (s = str; n && *s; ++s, --n)
    ;
  return (s - str);
}
#endif

/* str*,mem*,b* */

#ifdef WRAP_memcpy
WRAPPER2(void *, memcpy, void *dest, const void *src, size_t n)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT(src, n, __MF_CHECK_READ, "memcpy source");
  MF_VALIDATE_EXTENT(dest, n, __MF_CHECK_WRITE, "memcpy dest");
  return memcpy (dest, src, n);
}
#endif


#ifdef WRAP_memmove
WRAPPER2(void *, memmove, void *dest, const void *src, size_t n)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT(src, n, __MF_CHECK_READ, "memmove src");
  MF_VALIDATE_EXTENT(dest, n, __MF_CHECK_WRITE, "memmove dest");
  return memmove (dest, src, n);
}
#endif

#ifdef WRAP_memset
WRAPPER2(void *, memset, void *s, int c, size_t n)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT(s, n, __MF_CHECK_WRITE, "memset dest");
  return memset (s, c, n);
}
#endif

#ifdef WRAP_memcmp
WRAPPER2(int, memcmp, const void *s1, const void *s2, size_t n)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT(s1, n, __MF_CHECK_READ, "memcmp 1st arg");
  MF_VALIDATE_EXTENT(s2, n, __MF_CHECK_READ, "memcmp 2nd arg");
  return memcmp (s1, s2, n);
}
#endif

#ifdef WRAP_memchr
WRAPPER2(void *, memchr, const void *s, int c, size_t n)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT(s, n, __MF_CHECK_READ, "memchr region");
  return memchr (s, c, n);
}
#endif

#ifdef WRAP_memrchr
WRAPPER2(void *, memrchr, const void *s, int c, size_t n)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT(s, n, __MF_CHECK_READ, "memrchr region");
  return memrchr (s, c, n);
}
#endif

#ifdef WRAP_strcpy
WRAPPER2(char *, strcpy, char *dest, const char *src)
{
  /* nb: just because strlen(src) == n doesn't mean (src + n) or (src + n +
     1) are valid pointers. the allocated object might have size < n.
     check anyways. */

  size_t n = strlen (src);
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT(src, CLAMPADD(n, 1), __MF_CHECK_READ, "strcpy src"); 
  MF_VALIDATE_EXTENT(dest, CLAMPADD(n, 1), __MF_CHECK_WRITE, "strcpy dest");
  return strcpy (dest, src);
}
#endif

#ifdef WRAP_strncpy
WRAPPER2(char *, strncpy, char *dest, const char *src, size_t n)
{
  size_t len = strnlen (src, n);
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT(src, len, __MF_CHECK_READ, "strncpy src");
  MF_VALIDATE_EXTENT(dest, len, __MF_CHECK_WRITE, "strncpy dest"); /* nb: strNcpy */
  return strncpy (dest, src, n);
}
#endif

#ifdef WRAP_strcat
WRAPPER2(char *, strcat, char *dest, const char *src)
{
  size_t dest_sz;
  size_t src_sz;
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  dest_sz = strlen (dest);
  src_sz = strlen (src);  
  MF_VALIDATE_EXTENT(src, CLAMPADD(src_sz, 1), __MF_CHECK_READ, "strcat src");
  MF_VALIDATE_EXTENT(dest, CLAMPADD(dest_sz, CLAMPADD(src_sz, 1)),
		     __MF_CHECK_WRITE, "strcat dest");
  return strcat (dest, src);
}
#endif

#ifdef WRAP_strncat
WRAPPER2(char *, strncat, char *dest, const char *src, size_t n)
{

  /* nb: validating the extents (s,n) might be a mistake for two reasons.
     
  (1) the string s might be shorter than n chars, and n is just a 
  poor choice by the programmer. this is not a "true" error in the
  sense that the call to strncat would still be ok.
  
  (2) we could try to compensate for case (1) by calling strlen(s) and
  using that as a bound for the extent to verify, but strlen might fall off
  the end of a non-terminated string, leading to a false positive.
  
  so we will call strnlen(s,n) and use that as a bound.

  if strnlen returns a length beyond the end of the registered extent
  associated with s, there is an error: the programmer's estimate for n is
  too large _AND_ the string s is unterminated, in which case they'd be
  about to touch memory they don't own while calling strncat.

  this same logic applies to further uses of strnlen later down in this
  file. */

  size_t src_sz;
  size_t dest_sz;
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  src_sz = strnlen (src, n);
  dest_sz = strnlen (dest, n);
  MF_VALIDATE_EXTENT(src, src_sz, __MF_CHECK_READ, "strncat src");
  MF_VALIDATE_EXTENT(dest, (CLAMPADD(dest_sz, CLAMPADD(src_sz, 1))),
		     __MF_CHECK_WRITE, "strncat dest");
  return strncat (dest, src, n);
}
#endif

#ifdef WRAP_strcmp
WRAPPER2(int, strcmp, const char *s1, const char *s2)
{
  size_t s1_sz;
  size_t s2_sz;
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  s1_sz = strlen (s1);
  s2_sz = strlen (s2);  
  MF_VALIDATE_EXTENT(s1, CLAMPADD(s1_sz, 1), __MF_CHECK_READ, "strcmp 1st arg");
  MF_VALIDATE_EXTENT(s2, CLAMPADD(s2_sz, 1), __MF_CHECK_WRITE, "strcmp 2nd arg");
  return strcmp (s1, s2);
}
#endif

#ifdef WRAP_strcasecmp
WRAPPER2(int, strcasecmp, const char *s1, const char *s2)
{
  size_t s1_sz;
  size_t s2_sz;
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  s1_sz = strlen (s1);
  s2_sz = strlen (s2);  
  MF_VALIDATE_EXTENT(s1, CLAMPADD(s1_sz, 1), __MF_CHECK_READ, "strcasecmp 1st arg");
  MF_VALIDATE_EXTENT(s2, CLAMPADD(s2_sz, 1), __MF_CHECK_READ, "strcasecmp 2nd arg");
  return strcasecmp (s1, s2);
}
#endif

#ifdef WRAP_strncmp
WRAPPER2(int, strncmp, const char *s1, const char *s2, size_t n)
{
  size_t s1_sz;
  size_t s2_sz;
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  s1_sz = strnlen (s1, n);
  s2_sz = strnlen (s2, n);
  MF_VALIDATE_EXTENT(s1, s1_sz, __MF_CHECK_READ, "strncmp 1st arg");
  MF_VALIDATE_EXTENT(s2, s2_sz, __MF_CHECK_READ, "strncmp 2nd arg");
  return strncmp (s1, s2, n);
}
#endif

#ifdef WRAP_strncasecmp
WRAPPER2(int, strncasecmp, const char *s1, const char *s2, size_t n)
{
  size_t s1_sz;
  size_t s2_sz;
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  s1_sz = strnlen (s1, n);
  s2_sz = strnlen (s2, n);
  MF_VALIDATE_EXTENT(s1, s1_sz, __MF_CHECK_READ, "strncasecmp 1st arg");
  MF_VALIDATE_EXTENT(s2, s2_sz, __MF_CHECK_READ, "strncasecmp 2nd arg");
  return strncasecmp (s1, s2, n);
}
#endif

#ifdef WRAP_strdup
WRAPPER2(char *, strdup, const char *s)
{
  DECLARE(void *, malloc, size_t sz);
  char *result;
  size_t n = strlen (s);
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT(s, CLAMPADD(n,1), __MF_CHECK_READ, "strdup region");
  result = (char *)CALL_REAL(malloc, 
			     CLAMPADD(CLAMPADD(n,1),
				      CLAMPADD(__mf_opts.crumple_zone,
					       __mf_opts.crumple_zone)));

  if (UNLIKELY(! result)) return result;

  result += __mf_opts.crumple_zone;
  memcpy (result, s, n);
  result[n] = '\0';

  __mf_register (result, CLAMPADD(n,1), __MF_TYPE_HEAP_I, "strdup region");
  return result;
}
#endif

#ifdef WRAP_strndup
WRAPPER2(char *, strndup, const char *s, size_t n)
{
  DECLARE(void *, malloc, size_t sz);
  char *result;
  size_t sz = strnlen (s, n);
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT(s, sz, __MF_CHECK_READ, "strndup region"); /* nb: strNdup */

  /* note: strndup still adds a \0, even with the N limit! */
  result = (char *)CALL_REAL(malloc, 
			     CLAMPADD(CLAMPADD(n,1),
				      CLAMPADD(__mf_opts.crumple_zone,
					       __mf_opts.crumple_zone)));
  
  if (UNLIKELY(! result)) return result;

  result += __mf_opts.crumple_zone;
  memcpy (result, s, n);
  result[n] = '\0';

  __mf_register (result, CLAMPADD(n,1), __MF_TYPE_HEAP_I, "strndup region");
  return result;
}
#endif

#ifdef WRAP_strchr
WRAPPER2(char *, strchr, const char *s, int c)
{
  size_t n;
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  n = strlen (s);
  MF_VALIDATE_EXTENT(s, CLAMPADD(n,1), __MF_CHECK_READ, "strchr region");
  return strchr (s, c);
}
#endif

#ifdef WRAP_strrchr
WRAPPER2(char *, strrchr, const char *s, int c)
{
  size_t n;
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  n = strlen (s);
  MF_VALIDATE_EXTENT(s, CLAMPADD(n,1), __MF_CHECK_READ, "strrchr region");
  return strrchr (s, c);
}
#endif

#ifdef WRAP_strstr
WRAPPER2(char *, strstr, const char *haystack, const char *needle)
{
  size_t haystack_sz;
  size_t needle_sz;
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  haystack_sz = strlen (haystack);
  needle_sz = strlen (needle);
  MF_VALIDATE_EXTENT(haystack, CLAMPADD(haystack_sz, 1), __MF_CHECK_READ, "strstr haystack");
  MF_VALIDATE_EXTENT(needle, CLAMPADD(needle_sz, 1), __MF_CHECK_READ, "strstr needle");
  return strstr (haystack, needle);
}
#endif

#ifdef WRAP_memmem
WRAPPER2(void *, memmem, 
	const void *haystack, size_t haystacklen,
	const void *needle, size_t needlelen)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT(haystack, haystacklen, __MF_CHECK_READ, "memmem haystack");
  MF_VALIDATE_EXTENT(needle, needlelen, __MF_CHECK_READ, "memmem needle");
  return memmem (haystack, haystacklen, needle, needlelen);
}
#endif

#ifdef WRAP_strlen
WRAPPER2(size_t, strlen, const char *s)
{
  size_t result = strlen (s);
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT(s, CLAMPADD(result, 1), __MF_CHECK_READ, "strlen region");
  return result;
}
#endif

#ifdef WRAP_strnlen
WRAPPER2(size_t, strnlen, const char *s, size_t n)
{
  size_t result = strnlen (s, n);
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT(s, result, __MF_CHECK_READ, "strnlen region");
  return result;
}
#endif

#ifdef WRAP_bzero
WRAPPER2(void, bzero, void *s, size_t n)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT(s, n, __MF_CHECK_WRITE, "bzero region");
  bzero (s, n);
}
#endif

#ifdef WRAP_bcopy
#undef bcopy
WRAPPER2(void, bcopy, const void *src, void *dest, size_t n)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT(src, n, __MF_CHECK_READ, "bcopy src");
  MF_VALIDATE_EXTENT(dest, n, __MF_CHECK_WRITE, "bcopy dest");
  bcopy (src, dest, n);
}
#endif

#ifdef WRAP_bcmp
#undef bcmp
WRAPPER2(int, bcmp, const void *s1, const void *s2, size_t n)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT(s1, n, __MF_CHECK_READ, "bcmp 1st arg");
  MF_VALIDATE_EXTENT(s2, n, __MF_CHECK_READ, "bcmp 2nd arg");
  return bcmp (s1, s2, n);
}
#endif

#ifdef WRAP_index
WRAPPER2(char *, index, const char *s, int c)
{
  size_t n = strlen (s);
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT(s, CLAMPADD(n, 1), __MF_CHECK_READ, "index region");
  return index (s, c);
}
#endif

#ifdef WRAP_rindex
WRAPPER2(char *, rindex, const char *s, int c)
{
  size_t n = strlen (s);
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT(s, CLAMPADD(n, 1), __MF_CHECK_READ, "rindex region");
  return rindex (s, c);
}
#endif

/* XXX:  stpcpy, memccpy */


/* XXX: *printf,*scanf */


/* XXX: setjmp, longjmp */

#ifdef WRAP_asctime
WRAPPER2(char *, asctime, struct tm *tm)
{
  static char *reg_result = NULL;
  char *result;
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT(tm, sizeof (struct tm), __MF_CHECK_READ, "asctime tm");
  result = asctime (tm);
  if (reg_result == NULL)
    {
      __mf_register (result, strlen (result)+1, __MF_TYPE_STATIC, "asctime string");
      reg_result = result;
    }
  return result;
}
#endif

#ifdef WRAP_ctime
WRAPPER2(char *, ctime, const time_t *timep)
{
  static char *reg_result = NULL;
  char *result;
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT(timep, sizeof (time_t), __MF_CHECK_READ, "ctime time");
  result = ctime (timep);
  if (reg_result == NULL)
    {
      /* XXX: what if asctime and ctime return the same static ptr? */
      __mf_register (result, strlen (result)+1, __MF_TYPE_STATIC, "ctime string");
      reg_result = result;
    }
  return result;
}
#endif


#ifdef WRAP_localtime
WRAPPER2(struct tm*, localtime, const time_t *timep)
{
  static struct tm *reg_result = NULL;
  struct tm *result;
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT(timep, sizeof (time_t), __MF_CHECK_READ, "localtime time");
  result = localtime (timep);
  if (reg_result == NULL)
    {
      __mf_register (result, sizeof (struct tm), __MF_TYPE_STATIC, "localtime tm");
      reg_result = result;
    }
  return result;
}
#endif

#ifdef WRAP_gmtime
WRAPPER2(struct tm*, gmtime, const time_t *timep)
{
  static struct tm *reg_result = NULL;
  struct tm *result;
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT(timep, sizeof (time_t), __MF_CHECK_READ, "gmtime time");
  result = gmtime (timep);
  if (reg_result == NULL)
    {
      __mf_register (result, sizeof (struct tm), __MF_TYPE_STATIC, "gmtime tm");
      reg_result = result;
    }
  return result;
}
#endif



/* EL start */

/* The following indicate if the result of the corresponding function
 * should be explicitly un/registered by the wrapper
*/
#define MF_REGISTER_strerror		__MF_TYPE_STATIC
#undef  MF_REGISTER_fopen
#define MF_RESULT_SIZE_fopen		(sizeof (FILE))
#undef  MF_REGISTER_opendir
#define MF_RESULT_SIZE_opendir		0	/* (sizeof (DIR)) */
#undef  MF_REGISTER_readdir
#define MF_REGISTER_gethostbyname	__MF_TYPE_STATIC
#undef  MF_REGISTER_gethostbyname_items
#undef  MF_REGISTER_dlopen
#undef  MF_REGISTER_dlerror
#undef  MF_REGISTER_dlsym
#define MF_REGISTER_shmat		__MF_TYPE_GUESS


#ifdef WRAP_time
#include <time.h>
WRAPPER2(time_t, time, time_t *timep)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  if (NULL != timep)
    MF_VALIDATE_EXTENT (timep, sizeof (*timep), __MF_CHECK_WRITE,
      "time timep");
  return time (timep);
}
#endif

#ifdef WRAP_strerror
WRAPPER2(char *, strerror, int errnum)
{
  char *p;
  size_t n;
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  p = strerror (errnum);
  if (NULL != p) {
    n = strlen (p);
    n = CLAMPADD(n, 1);
#ifdef MF_REGISTER_strerror
    __mf_register (p, n, MF_REGISTER_strerror, "strerror result");
#endif
    MF_VALIDATE_EXTENT (p, n, __MF_CHECK_WRITE, "strerror result");
  }
  return p;
}
#endif

#ifdef WRAP_fopen
WRAPPER2(FILE *, fopen, const char *path, const char *mode)
{
  size_t n;
  FILE *p;
  TRACE ("%s\n", __PRETTY_FUNCTION__);

  n = strlen (path);
  MF_VALIDATE_EXTENT (path, CLAMPADD(n, 1), __MF_CHECK_READ, "fopen path");

  n = strlen (mode);
  MF_VALIDATE_EXTENT (mode, CLAMPADD(n, 1), __MF_CHECK_READ, "fopen mode");

  p = fopen (path, mode);
  if (NULL != p) {
#ifdef MF_REGISTER_fopen
    __mf_register (p, sizeof (*p), MF_REGISTER_fopen, "fopen result");
#endif
    MF_VALIDATE_EXTENT (p, sizeof (*p), __MF_CHECK_WRITE, "fopen result");
  }

  return p;
}
#endif

#ifdef HAVE_FOPEN64
#ifdef WRAP_fopen64
WRAPPER2(FILE *, fopen64, const char *path, const char *mode)
{
  size_t n;
  FILE *p;
  TRACE ("%s\n", __PRETTY_FUNCTION__);

  n = strlen (path);
  MF_VALIDATE_EXTENT (path, CLAMPADD(n, 1), __MF_CHECK_READ, "fopen64 path");

  n = strlen (mode);
  MF_VALIDATE_EXTENT (mode, CLAMPADD(n, 1), __MF_CHECK_READ, "fopen64 mode");

  p = fopen64 (path, mode);
  if (NULL != p) {
#ifdef MF_REGISTER_fopen
    __mf_register (p, sizeof (*p), MF_REGISTER_fopen, "fopen64 result");
#endif
    MF_VALIDATE_EXTENT (p, sizeof (*p), __MF_CHECK_WRITE, "fopen64 result");
  }

  return p;
}
#endif
#endif

#ifdef WRAP_fclose
WRAPPER2(int, fclose, FILE *stream)
{
  int resp;
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (stream, sizeof (*stream), __MF_CHECK_WRITE,
    "fclose stream");
  resp = fclose (stream);
#ifdef MF_REGISTER_fopen
  __mf_unregister (stream, sizeof (*stream));
#endif

  return resp;
}
#endif

#ifdef WRAP_fread
WRAPPER2(size_t, fread, void *ptr, size_t size, size_t nmemb, FILE *stream)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (stream, sizeof (*stream), __MF_CHECK_WRITE,
    "fread stream");
  MF_VALIDATE_EXTENT (ptr, size * nmemb, __MF_CHECK_WRITE, "fread buffer");
  return fread (ptr, size, nmemb, stream);
}
#endif

#ifdef WRAP_fwrite
WRAPPER2(size_t, fwrite, const void *ptr, size_t size, size_t nmemb,
	FILE *stream)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (stream, sizeof (*stream), __MF_CHECK_WRITE,
    "fwrite stream");
  MF_VALIDATE_EXTENT (ptr, size * nmemb, __MF_CHECK_READ, "fwrite buffer");
  return fwrite (ptr, size, nmemb, stream);
}
#endif

#ifdef WRAP_fgetc
WRAPPER2(int, fgetc, FILE *stream)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (stream, sizeof (*stream), __MF_CHECK_WRITE,
    "fgetc stream");
  return fgetc (stream);
}
#endif

#ifdef WRAP_fgets
WRAPPER2(char *, fgets, char *s, int size, FILE *stream)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (stream, sizeof (*stream), __MF_CHECK_WRITE,
    "fgets stream");
  MF_VALIDATE_EXTENT (s, size, __MF_CHECK_WRITE, "fgets buffer");
  return fgets (s, size, stream);
}
#endif

#ifdef WRAP_getc
WRAPPER2(int, getc, FILE *stream)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (stream, sizeof (*stream), __MF_CHECK_WRITE,
    "getc stream");
  return getc (stream);
}
#endif

#ifdef WRAP_gets
WRAPPER2(char *, gets, char *s)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (s, 1, __MF_CHECK_WRITE, "gets buffer");
  /* Avoid link-time warning... */
  s = fgets (s, INT_MAX, stdin);
  if (NULL != s) {	/* better late than never */
    size_t n = strlen (s);
    MF_VALIDATE_EXTENT (s, CLAMPADD(n, 1), __MF_CHECK_WRITE, "gets buffer");
  }
  return s;
}
#endif

#ifdef WRAP_ungetc
WRAPPER2(int, ungetc, int c, FILE *stream)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (stream, sizeof (*stream), __MF_CHECK_WRITE,
     "ungetc stream");
  return ungetc (c, stream);
}
#endif

#ifdef WRAP_fputc
WRAPPER2(int, fputc, int c, FILE *stream)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (stream, sizeof (*stream), __MF_CHECK_WRITE,
    "fputc stream");
  return fputc (c, stream);
}
#endif

#ifdef WRAP_fputs
WRAPPER2(int, fputs, const char *s, FILE *stream)
{
  size_t n;
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  n = strlen (s);
  MF_VALIDATE_EXTENT (s, CLAMPADD(n, 1), __MF_CHECK_READ, "fputs buffer");
  MF_VALIDATE_EXTENT (stream, sizeof (*stream), __MF_CHECK_WRITE,
    "fputs stream");
  return fputs (s, stream);
}
#endif

#ifdef WRAP_putc
WRAPPER2(int, putc, int c, FILE *stream)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (stream, sizeof (*stream), __MF_CHECK_WRITE,
    "putc stream");
  return putc (c, stream);
}
#endif

#ifdef WRAP_puts
WRAPPER2(int, puts, const char *s)
{
  size_t n;
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  n = strlen (s);
  MF_VALIDATE_EXTENT (s, CLAMPADD(n, 1), __MF_CHECK_READ, "puts buffer");
  return puts (s);
}
#endif

#ifdef WRAP_clearerr
WRAPPER2(void, clearerr, FILE *stream)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (stream, sizeof (*stream), __MF_CHECK_WRITE,
    "clearerr stream");
  clearerr (stream);
}
#endif

#ifdef WRAP_feof
WRAPPER2(int, feof, FILE *stream)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (stream, sizeof (*stream), __MF_CHECK_WRITE,
    "feof stream");
  return feof (stream);
}
#endif

#ifdef WRAP_ferror
WRAPPER2(int, ferror, FILE *stream)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (stream, sizeof (*stream), __MF_CHECK_WRITE,
    "ferror stream");
  return ferror (stream);
}
#endif

#ifdef WRAP_fileno
#include <stdio.h>
WRAPPER2(int, fileno, FILE *stream)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (stream, sizeof (*stream), __MF_CHECK_WRITE,
    "fileno stream");
  return fileno (stream);
}
#endif

#ifdef WRAP_printf
#include <stdio.h>
#include <stdarg.h>
WRAPPER2(int, printf, const char *format, ...)
{
  size_t n;
  va_list ap;
  int result;
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  n = strlen (format);
  MF_VALIDATE_EXTENT (format, CLAMPADD(n, 1), __MF_CHECK_READ,
    "printf format");
  va_start (ap, format);
  result = vprintf (format, ap);
  va_end (ap);
  return result;
}
#endif

#ifdef WRAP_fprintf
#include <stdio.h>
#include <stdarg.h>
WRAPPER2(int, fprintf, FILE *stream, const char *format, ...)
{
  size_t n;
  va_list ap;
  int result;
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (stream, sizeof (*stream), __MF_CHECK_WRITE,
    "fprintf stream");
  n = strlen (format);
  MF_VALIDATE_EXTENT (format, CLAMPADD(n, 1), __MF_CHECK_READ,
    "fprintf format");
  va_start (ap, format);
  result = vfprintf (stream, format, ap);
  va_end (ap);
  return result;
}
#endif

#ifdef WRAP_sprintf
#include <stdio.h>
#include <stdarg.h>
WRAPPER2(int, sprintf, char *str, const char *format, ...)
{
  size_t n;
  va_list ap;
  int result;
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (str, 1, __MF_CHECK_WRITE, "sprintf str");
  n = strlen (format);
  MF_VALIDATE_EXTENT (format, CLAMPADD(n, 1), __MF_CHECK_READ,
    "sprintf format");
  va_start (ap, format);
  result = vsprintf (str, format, ap);
  va_end (ap);
  n = strlen (str);
  MF_VALIDATE_EXTENT (str, CLAMPADD(n, 1), __MF_CHECK_WRITE, "sprintf str");
  return result;
}
#endif

#ifdef WRAP_snprintf
#include <stdio.h>
#include <stdarg.h>
WRAPPER2(int, snprintf, char *str, size_t size, const char *format, ...)
{
  size_t n;
  va_list ap;
  int result;
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (str, size, __MF_CHECK_WRITE, "snprintf str");
  n = strlen (format);
  MF_VALIDATE_EXTENT (format, CLAMPADD(n, 1), __MF_CHECK_READ,
    "snprintf format");
  va_start (ap, format);
  result = vsnprintf (str, size, format, ap);
  va_end (ap);
  return result;
}
#endif

#ifdef WRAP_vprintf
#include <stdio.h>
#include <stdarg.h>
WRAPPER2(int, vprintf,  const char *format, va_list ap)
{
  size_t n;
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  n = strlen (format);
  MF_VALIDATE_EXTENT (format, CLAMPADD(n, 1), __MF_CHECK_READ,
    "vprintf format");
  return vprintf (format, ap);
}
#endif

#ifdef WRAP_vfprintf
#include <stdio.h>
#include <stdarg.h>
WRAPPER2(int, vfprintf, FILE *stream, const char *format, va_list ap)
{
  size_t n;
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (stream, sizeof (*stream), __MF_CHECK_WRITE,
    "vfprintf stream");
  n = strlen (format);
  MF_VALIDATE_EXTENT (format, CLAMPADD(n, 1), __MF_CHECK_READ,
    "vfprintf format");
  return vfprintf (stream, format, ap);
}
#endif

#ifdef WRAP_vsprintf
#include <stdio.h>
#include <stdarg.h>
WRAPPER2(int, vsprintf, char *str, const char *format, va_list ap)
{
  size_t n;
  int result;
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (str, 1, __MF_CHECK_WRITE, "vsprintf str");
  n = strlen (format);
  MF_VALIDATE_EXTENT (format, CLAMPADD(n, 1), __MF_CHECK_READ,
    "vsprintf format");
  result = vsprintf (str, format, ap);
  n = strlen (str);
  MF_VALIDATE_EXTENT (str, CLAMPADD(n, 1), __MF_CHECK_WRITE, "vsprintf str");
  return result;
}
#endif

#ifdef WRAP_vsnprintf
#include <stdio.h>
#include <stdarg.h>
WRAPPER2(int, vsnprintf, char *str, size_t size, const char *format,
	va_list ap)
{
  size_t n;
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (str, size, __MF_CHECK_WRITE, "vsnprintf str");
  n = strlen (format);
  MF_VALIDATE_EXTENT (format, CLAMPADD(n, 1), __MF_CHECK_READ,
    "vsnprintf format");
  return vsnprintf (str, size, format, ap);
}
#endif

#ifdef WRAP_access
WRAPPER2(int , access, const char *path, int mode)
{
  size_t n;
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  n = strlen (path);
  MF_VALIDATE_EXTENT (path, CLAMPADD(n, 1), __MF_CHECK_READ, "access path");
  return access (path, mode);
}
#endif

#ifdef WRAP_remove
WRAPPER2(int , remove, const char *path)
{
  size_t n;
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  n = strlen (path);
  MF_VALIDATE_EXTENT (path, CLAMPADD(n, 1), __MF_CHECK_READ, "remove path");
  return remove (path);
}
#endif

#ifdef WRAP_fflush
WRAPPER2(int, fflush, FILE *stream)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (stream, sizeof (*stream), __MF_CHECK_WRITE,
    "fflush stream");
  return fflush (stream);
}
#endif

#ifdef WRAP_fseek
WRAPPER2(int, fseek, FILE *stream, long offset, int whence)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (stream, sizeof (*stream), __MF_CHECK_WRITE,
    "fseek stream");
  return fseek (stream, offset, whence);
}
#endif

#ifdef HAVE_FSEEKO64
#ifdef WRAP_fseeko64
WRAPPER2(int, fseeko64, FILE *stream, off64_t offset, int whence)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (stream, sizeof (*stream), __MF_CHECK_WRITE,
    "fseeko64 stream");
  return fseeko64 (stream, offset, whence);
}
#endif
#endif

#ifdef WRAP_ftell
WRAPPER2(long, ftell, FILE *stream)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (stream, sizeof (*stream), __MF_CHECK_WRITE,
    "ftell stream");
  return ftell (stream);
}
#endif

#ifdef HAVE_FTELLO64
#ifdef WRAP_ftello64
WRAPPER2(off64_t, ftello64, FILE *stream)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (stream, sizeof (*stream), __MF_CHECK_WRITE,
    "ftello64 stream");
  return ftello64 (stream);
}
#endif
#endif

#ifdef WRAP_rewind
WRAPPER2(void, rewind, FILE *stream)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (stream, sizeof (*stream), __MF_CHECK_WRITE,
    "rewind stream");
  rewind (stream);
}
#endif

#ifdef WRAP_fgetpos
WRAPPER2(int, fgetpos, FILE *stream, fpos_t *pos)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (stream, sizeof (*stream), __MF_CHECK_WRITE,
    "fgetpos stream");
  MF_VALIDATE_EXTENT (pos, sizeof (*pos), __MF_CHECK_WRITE, "fgetpos pos");
  return fgetpos (stream, pos);
}
#endif

#ifdef WRAP_fsetpos
WRAPPER2(int, fsetpos, FILE *stream, fpos_t *pos)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (stream, sizeof (*stream), __MF_CHECK_WRITE,
    "fsetpos stream");
  MF_VALIDATE_EXTENT (pos, sizeof (*pos), __MF_CHECK_READ, "fsetpos pos");
  return fsetpos (stream, pos);
}
#endif

#ifdef WRAP_stat
#include <sys/stat.h>
WRAPPER2(int , stat, const char *path, struct stat *buf)
{
  size_t n;
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  n = strlen (path);
  MF_VALIDATE_EXTENT (path, CLAMPADD(n, 1), __MF_CHECK_READ, "stat path");
  MF_VALIDATE_EXTENT (buf, sizeof (*buf), __MF_CHECK_READ, "stat buf");
  return stat (path, buf);
}
#endif

#ifdef HAVE_STAT64
#ifdef WRAP_stat64
#include <sys/stat.h>
WRAPPER2(int , stat64, const char *path, struct stat64 *buf)
{
  size_t n;
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  n = strlen (path);
  MF_VALIDATE_EXTENT (path, CLAMPADD(n, 1), __MF_CHECK_READ, "stat64 path");
  MF_VALIDATE_EXTENT (buf, sizeof (*buf), __MF_CHECK_READ, "stat64 buf");
  return stat64 (path, buf);
}
#endif
#endif

#ifdef WRAP_fstat
#include <sys/stat.h>
WRAPPER2(int , fstat, int filedes, struct stat *buf)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (buf, sizeof (*buf), __MF_CHECK_READ, "fstat buf");
  return fstat (filedes, buf);
}
#endif

#ifdef WRAP_lstat
#include <sys/stat.h>
WRAPPER2(int , lstat, const char *path, struct stat *buf)
{
  size_t n;
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  n = strlen (path);
  MF_VALIDATE_EXTENT (path, CLAMPADD(n, 1), __MF_CHECK_READ, "lstat path");
  MF_VALIDATE_EXTENT (buf, sizeof (*buf), __MF_CHECK_READ, "lstat buf");
  return lstat (path, buf);
}
#endif

#ifdef WRAP_mkfifo
#include <sys/stat.h>
WRAPPER2(int , mkfifo, const char *path, mode_t mode)
{
  size_t n;
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  n = strlen (path);
  MF_VALIDATE_EXTENT (path, CLAMPADD(n, 1), __MF_CHECK_READ, "mkfifo path");
  return mkfifo (path, mode);
}
#endif

#ifdef WRAP_setvbuf
WRAPPER2(int, setvbuf, FILE *stream, char *buf, int mode , size_t size)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (stream, sizeof (*stream), __MF_CHECK_WRITE,
    "setvbuf stream");
  if (NULL != buf)
    MF_VALIDATE_EXTENT (buf, size, __MF_CHECK_READ, "setvbuf buf");
  return setvbuf (stream, buf, mode, size);
}
#endif

#ifdef WRAP_setbuf
WRAPPER2(void, setbuf, FILE *stream, char *buf)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (stream, sizeof (*stream), __MF_CHECK_WRITE,
    "setbuf stream");
  if (NULL != buf)
    MF_VALIDATE_EXTENT (buf, BUFSIZ, __MF_CHECK_READ, "setbuf buf");
  setbuf (stream, buf);
}
#endif

#ifdef WRAP_opendir
#include <dirent.h>
WRAPPER2(DIR *, opendir, const char *path)
{
  DIR *p;
  size_t n;
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  n = strlen (path);
  MF_VALIDATE_EXTENT (path, CLAMPADD(n, 1), __MF_CHECK_READ, "opendir path");

  p = opendir (path);
  if (NULL != p) {
#ifdef MF_REGISTER_opendir
    __mf_register (p, MF_RESULT_SIZE_opendir, MF_REGISTER_opendir,
      "opendir result");
#endif
    MF_VALIDATE_EXTENT (p, MF_RESULT_SIZE_opendir, __MF_CHECK_WRITE,
      "opendir result");
  }
  return p;
}
#endif

#ifdef WRAP_closedir
#include <dirent.h>
WRAPPER2(int, closedir, DIR *dir)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (dir, 0, __MF_CHECK_WRITE, "closedir dir");
#ifdef MF_REGISTER_opendir
  __mf_unregister (dir, MF_RESULT_SIZE_opendir);
#endif
  return closedir (dir);
}
#endif

#ifdef WRAP_readdir
#include <dirent.h>
WRAPPER2(struct dirent *, readdir, DIR *dir)
{
  struct dirent *p;
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (dir, 0, __MF_CHECK_READ, "readdir dir");
  p = readdir (dir);
  if (NULL != p) {
#ifdef MF_REGISTER_readdir
    __mf_register (p, sizeof (*p), MF_REGISTER_readdir, "readdir result");
#endif
    MF_VALIDATE_EXTENT (p, sizeof (*p), __MF_CHECK_WRITE, "readdir result");
  }
  return p;
}
#endif

#ifdef WRAP_recv
#include <sys/socket.h>
WRAPPER2(int, recv, int s, void *buf, size_t len, int flags)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (buf, len, __MF_CHECK_WRITE, "recv buf");
  return recv (s, buf, len, flags);
}
#endif

#ifdef WRAP_recvfrom
#include <sys/socket.h>
WRAPPER2(int, recvfrom, int s, void *buf, size_t len, int flags,
		struct sockaddr *from, socklen_t *fromlen)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (buf, len, __MF_CHECK_WRITE, "recvfrom buf");
  MF_VALIDATE_EXTENT (from, (size_t)*fromlen, __MF_CHECK_WRITE,
    "recvfrom from");
  return recvfrom (s, buf, len, flags, from, fromlen);
}
#endif

#ifdef WRAP_recvmsg
#include <sys/socket.h>
WRAPPER2(int, recvmsg, int s, struct msghdr *msg, int flags)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (msg, sizeof (*msg), __MF_CHECK_WRITE, "recvmsg msg");
  return recvmsg (s, msg, flags);
}
#endif

#ifdef WRAP_send
#include <sys/socket.h>
WRAPPER2(int, send, int s, const void *msg, size_t len, int flags)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (msg, len, __MF_CHECK_READ, "send msg");
  return send (s, msg, len, flags);
}
#endif

#ifdef WRAP_sendto
#include <sys/socket.h>
WRAPPER2(int, sendto, int s, const void *msg, size_t len, int flags,
		const struct sockaddr *to, socklen_t tolen)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (msg, len, __MF_CHECK_READ, "sendto msg");
  MF_VALIDATE_EXTENT (to, (size_t)tolen, __MF_CHECK_WRITE, "sendto to");
  return sendto (s, msg, len, flags, to, tolen);
}
#endif

#ifdef WRAP_sendmsg
#include <sys/socket.h>
WRAPPER2(int, sendmsg, int s, const void *msg, int flags)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (msg, sizeof (*msg), __MF_CHECK_READ, "sendmsg msg");
  return sendmsg (s, msg, flags);
}
#endif

#ifdef WRAP_setsockopt
#include <sys/socket.h>
WRAPPER2(int, setsockopt, int s, int level, int optname, const void *optval,
	socklen_t optlen)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (optval, (size_t)optlen, __MF_CHECK_READ,
    "setsockopt optval");
  return setsockopt (s, level, optname, optval, optlen);
}
#endif

#ifdef WRAP_getsockopt
#include <sys/socket.h>
WRAPPER2(int, getsockopt, int s, int level, int optname, void *optval,
		socklen_t *optlen)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (optval, (size_t)*optlen, __MF_CHECK_WRITE,
    "getsockopt optval");
  return getsockopt (s, level, optname, optval, optlen);
}
#endif

#ifdef WRAP_accept
#include <sys/socket.h>
WRAPPER2(int, accept, int s, struct  sockaddr *addr, socklen_t *addrlen)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (addr, (size_t)*addrlen, __MF_CHECK_WRITE, "accept addr");
  return accept (s, addr, addrlen);
}
#endif

#ifdef WRAP_bind
#include <sys/socket.h>
WRAPPER2(int, bind, int sockfd, struct  sockaddr *addr, socklen_t addrlen)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (addr, (size_t)addrlen, __MF_CHECK_WRITE, "bind addr");
  return bind (sockfd, addr, addrlen);
}
#endif

#ifdef WRAP_connect
#include <sys/socket.h>
WRAPPER2(int, connect, int sockfd, const struct sockaddr  *addr,
	socklen_t addrlen)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (addr, (size_t)addrlen, __MF_CHECK_READ,
    "connect addr");
  return connect (sockfd, addr, addrlen);
}
#endif

#ifdef WRAP_gethostname
WRAPPER2(int, gethostname, char *name, size_t len)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (name, len, __MF_CHECK_WRITE, "gethostname name");
  return gethostname (name, len);
}
#endif

#ifdef WRAP_sethostname
WRAPPER2(int, sethostname, const char *name, size_t len)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (name, len, __MF_CHECK_READ, "sethostname name");
  return sethostname (name, len);
}
#endif

#ifdef WRAP_gethostbyname
#include <netdb.h>
WRAPPER2(struct hostent *, gethostbyname, const char *name)
{
  struct hostent *p;
  char **ss;
  char *s;
  size_t n;
  int nreg;
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  n = strlen (name);
  MF_VALIDATE_EXTENT (name, CLAMPADD(n, 1), __MF_CHECK_READ,
    "gethostbyname name");
  p = gethostbyname (name);
  if (NULL != p) {
#ifdef MF_REGISTER_gethostbyname
    __mf_register (p, sizeof (*p), MF_REGISTER_gethostbyname,
      "gethostbyname result");
#endif
    MF_VALIDATE_EXTENT (p, sizeof (*p), __MF_CHECK_WRITE,
      "gethostbyname result");
    if (NULL != (s = p->h_name)) {
      n = strlen (s);
      n = CLAMPADD(n, 1);
#ifdef MF_REGISTER_gethostbyname_items
      __mf_register (s, n, MF_REGISTER_gethostbyname_items,
        "gethostbyname result->h_name");
#endif
      MF_VALIDATE_EXTENT (s, n, __MF_CHECK_WRITE,
        "gethostbyname result->h_name");
    }

    if (NULL != (ss = p->h_aliases)) {
      for (nreg = 1;; ++nreg) {
        s = *ss++;
        if (NULL == s)
          break;
        n = strlen (s);
        n = CLAMPADD(n, 1);
#ifdef MF_REGISTER_gethostbyname_items
        __mf_register (s, n, MF_REGISTER_gethostbyname_items,
          "gethostbyname result->h_aliases[]");
#endif
        MF_VALIDATE_EXTENT (s, n, __MF_CHECK_WRITE,
          "gethostbyname result->h_aliases[]");
      }
      nreg *= sizeof (*p->h_aliases);
#ifdef MF_REGISTER_gethostbyname_items
      __mf_register (p->h_aliases, nreg, MF_REGISTER_gethostbyname_items,
        "gethostbyname result->h_aliases");
#endif
      MF_VALIDATE_EXTENT (p->h_aliases, nreg, __MF_CHECK_WRITE,
        "gethostbyname result->h_aliases");
    }

    if (NULL != (ss = p->h_addr_list)) {
      for (nreg = 1;; ++nreg) {
        s = *ss++;
        if (NULL == s)
          break;
#ifdef MF_REGISTER_gethostbyname_items
        __mf_register (s, p->h_length, MF_REGISTER_gethostbyname_items,
          "gethostbyname result->h_addr_list[]");
#endif
        MF_VALIDATE_EXTENT (s, p->h_length, __MF_CHECK_WRITE,
          "gethostbyname result->h_addr_list[]");
      }
      nreg *= sizeof (*p->h_addr_list);
#ifdef MF_REGISTER_gethostbyname_items
      __mf_register (p->h_addr_list, nreg, MF_REGISTER_gethostbyname_items,
        "gethostbyname result->h_addr_list");
#endif
      MF_VALIDATE_EXTENT (p->h_addr_list, nreg, __MF_CHECK_WRITE,
        "gethostbyname result->h_addr_list");
    }
  }
  return p;
}
#endif

#ifdef WRAP_wait
#include <sys/wait.h>
WRAPPER2(pid_t, wait, int *status)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  if (NULL != status)
    MF_VALIDATE_EXTENT (status, sizeof (*status), __MF_CHECK_WRITE,
      "wait status");
  return wait (status);
}
#endif

#ifdef WRAP_waitpid
#include <sys/wait.h>
WRAPPER2(pid_t, waitpid, pid_t pid, int *status, int options)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  if (NULL != status)
    MF_VALIDATE_EXTENT (status, sizeof (*status), __MF_CHECK_WRITE,
      "waitpid status");
  return waitpid (pid, status, options);
}
#endif

#ifdef WRAP_popen
WRAPPER2(FILE *, popen, const char *command, const char *mode)
{
  size_t n;
  FILE *p;
  TRACE ("%s\n", __PRETTY_FUNCTION__);

  n = strlen (command);
  MF_VALIDATE_EXTENT (command, CLAMPADD(n, 1), __MF_CHECK_READ, "popen path");

  n = strlen (mode);
  MF_VALIDATE_EXTENT (mode, CLAMPADD(n, 1), __MF_CHECK_READ, "popen mode");

  p = popen (command, mode);
  if (NULL != p) {
#ifdef MF_REGISTER_fopen
    __mf_register (p, sizeof (*p), MF_REGISTER_fopen, "popen result");
#endif
    MF_VALIDATE_EXTENT (p, sizeof (*p), __MF_CHECK_WRITE, "popen result");
  }
  return p;
}
#endif

#ifdef WRAP_pclose
WRAPPER2(int, pclose, FILE *stream)
{
  int resp;
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (stream, sizeof (*stream), __MF_CHECK_WRITE,
    "pclose stream");
  resp = pclose (stream);
#ifdef MF_REGISTER_fopen
  __mf_unregister (stream, sizeof (*stream));
#endif
  return resp;
}
#endif

#ifdef WRAP_execve
WRAPPER2(int, execve, const char *path, char *const argv [],
	char *const envp[])
{
  size_t n;
  char *const *p;
  const char *s;
  TRACE ("%s\n", __PRETTY_FUNCTION__);

  n = strlen (path);
  MF_VALIDATE_EXTENT (path, CLAMPADD(n, 1), __MF_CHECK_READ, "execve path");

  for (p = argv;;) {
    MF_VALIDATE_EXTENT (p, sizeof (*p), __MF_CHECK_READ, "execve *argv");
    s = *p++;
    if (NULL == s)
      break;
    n = strlen (s);
    MF_VALIDATE_EXTENT (s, CLAMPADD(n, 1), __MF_CHECK_READ, "execve **argv");
  }

  for (p = envp;;) {
    MF_VALIDATE_EXTENT (p, sizeof (*p), __MF_CHECK_READ, "execve *envp");
    s = *p++;
    if (NULL == s)
      break;
    n = strlen (s);
    MF_VALIDATE_EXTENT (s, CLAMPADD(n, 1), __MF_CHECK_READ, "execve **envp");
  }
  return execve (path, argv, envp);
}
#endif

#ifdef WRAP_execv
WRAPPER2(int, execv, const char *path, char *const argv [])
{
  size_t n;
  char *const *p;
  const char *s;
  TRACE ("%s\n", __PRETTY_FUNCTION__);

  n = strlen (path);
  MF_VALIDATE_EXTENT (path, CLAMPADD(n, 1), __MF_CHECK_READ, "execv path");

  for (p = argv;;) {
    MF_VALIDATE_EXTENT (p, sizeof (*p), __MF_CHECK_READ, "execv *argv");
    s = *p++;
    if (NULL == s)
      break;
    n = strlen (s);
    MF_VALIDATE_EXTENT (s, CLAMPADD(n, 1), __MF_CHECK_READ, "execv **argv");
  }
  return execv (path, argv);
}
#endif

#ifdef WRAP_execvp
WRAPPER2(int, execvp, const char *path, char *const argv [])
{
  size_t n;
  char *const *p;
  const char *s;
  TRACE ("%s\n", __PRETTY_FUNCTION__);

  n = strlen (path);
  MF_VALIDATE_EXTENT (path, CLAMPADD(n, 1), __MF_CHECK_READ, "execvp path");

  for (p = argv;;) {
    MF_VALIDATE_EXTENT (p, sizeof (*p), __MF_CHECK_READ, "execvp *argv");
    s = *p++;
    if (NULL == s)
      break;
    n = strlen (s);
    MF_VALIDATE_EXTENT (s, CLAMPADD(n, 1), __MF_CHECK_READ, "execvp **argv");
  }
  return execvp (path, argv);
}
#endif

#ifdef WRAP_system
WRAPPER2(int, system, const char *string)
{
  size_t n;
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  n = strlen (string);
  MF_VALIDATE_EXTENT (string, CLAMPADD(n, 1), __MF_CHECK_READ,
    "system string");
  return system (string);
}
#endif

#ifdef WRAP_dlopen
WRAPPER2(void *, dlopen, const char *path, int flags)
{
  void *p;
  size_t n;
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  n = strlen (path);
  MF_VALIDATE_EXTENT (path, CLAMPADD(n, 1), __MF_CHECK_READ, "dlopen path");
  p = dlopen (path, flags);
  if (NULL != p) {
#ifdef MF_REGISTER_dlopen
    __mf_register (p, 0, MF_REGISTER_dlopen, "dlopen result");
#endif
    MF_VALIDATE_EXTENT (p, 0, __MF_CHECK_WRITE, "dlopen result");
  }
  return p;
}
#endif

#ifdef WRAP_dlclose
WRAPPER2(int, dlclose, void *handle)
{
  int resp;
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (handle, 0, __MF_CHECK_READ, "dlclose handle");
  resp = dlclose (handle);
#ifdef MF_REGISTER_dlopen
  __mf_unregister (handle, 0);
#endif
  return resp;
}
#endif

#ifdef WRAP_dlerror
WRAPPER2(char *, dlerror)
{
  char *p;
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  p = dlerror ();
  if (NULL != p) {
    size_t n;
    n = strlen (p);
    n = CLAMPADD(n, 1);
#ifdef MF_REGISTER_dlerror
    __mf_register (p, n, MF_REGISTER_dlerror, "dlerror result");
#endif
    MF_VALIDATE_EXTENT (p, n, __MF_CHECK_WRITE, "dlerror result");
  }
  return p;
}
#endif

#ifdef WRAP_dlsym
WRAPPER2(void *, dlsym, void *handle, char *symbol)
{
  size_t n;
  void *p;
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (handle, 0, __MF_CHECK_READ, "dlsym handle");
  n = strlen (symbol);
  MF_VALIDATE_EXTENT (symbol, CLAMPADD(n, 1), __MF_CHECK_READ, "dlsym symbol");
  p = dlsym (handle, symbol);
  if (NULL != p) {
#ifdef MF_REGISTER_dlsym
    __mf_register (p, 0, MF_REGISTER_dlsym, "dlsym result");
#endif
    MF_VALIDATE_EXTENT (p, 0, __MF_CHECK_WRITE, "dlsym result");
  }
  return p;
}
#endif

#ifdef WRAP_semop
#include <sys/ipc.h>
#include <sys/sem.h>
WRAPPER2(int, semop, int semid, struct sembuf *sops, unsigned nsops)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  MF_VALIDATE_EXTENT (sops, sizeof (*sops) * nsops, __MF_CHECK_READ,
    "semop sops");
  return semop (semid, sops, nsops);
}
#endif

#ifdef WRAP_semctl
#include <sys/ipc.h>
#include <sys/sem.h>
#ifndef HAVE_UNION_SEMUN
union semun {
	int val;			/* value for SETVAL */
	struct semid_ds *buf;		/* buffer for IPC_STAT, IPC_SET */
	unsigned short int *array;	/* array for GETALL, SETALL */
	struct seminfo *__buf;		/* buffer for IPC_INFO */
};
#endif
WRAPPER2(int, semctl, int semid, int semnum, int cmd, union semun arg)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  switch (cmd) {
  case IPC_STAT:
    MF_VALIDATE_EXTENT (arg.buf, sizeof (*arg.buf), __MF_CHECK_WRITE,
      "semctl buf");
    break;
  case IPC_SET:
    MF_VALIDATE_EXTENT (arg.buf, sizeof (*arg.buf), __MF_CHECK_READ,
      "semctl buf");
    break;
  case GETALL:
    MF_VALIDATE_EXTENT (arg.array, sizeof (*arg.array), __MF_CHECK_WRITE,
      "semctl array");
  case SETALL:
    MF_VALIDATE_EXTENT (arg.array, sizeof (*arg.array), __MF_CHECK_READ,
      "semctl array");
    break;
#ifdef IPC_INFO
  /* FreeBSD 5.1 headers include IPC_INFO but not the __buf field.  */
#if !defined(__FreeBSD__)
  case IPC_INFO:
    MF_VALIDATE_EXTENT (arg.__buf, sizeof (*arg.__buf), __MF_CHECK_WRITE,
      "semctl __buf");
    break;
#endif
#endif
  default:
    break;
  }
  return semctl (semid, semnum, cmd, arg);
}
#endif

#ifdef WRAP_shmctl
#include <sys/ipc.h>
#include <sys/shm.h>
WRAPPER2(int, shmctl, int shmid, int cmd, struct shmid_ds *buf)
{
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  switch (cmd) {
  case IPC_STAT:
    MF_VALIDATE_EXTENT (buf, sizeof (*buf), __MF_CHECK_WRITE,
      "shmctl buf");
    break;
  case IPC_SET:
    MF_VALIDATE_EXTENT (buf, sizeof (*buf), __MF_CHECK_READ,
      "shmctl buf");
    break;
  default:
    break;
  }
  return shmctl (shmid, cmd, buf);
}
#endif

#ifdef WRAP_shmat
#include <sys/ipc.h>
#include <sys/shm.h>
WRAPPER2(void *, shmat, int shmid, const void *shmaddr, int shmflg)
{
  void *p;
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  p = shmat (shmid, shmaddr, shmflg);
#ifdef MF_REGISTER_shmat
  if (NULL != p) {
    struct shmid_ds buf;
    __mf_register (p, shmctl (shmid, IPC_STAT, &buf) ? 0 : buf.shm_segsz,
      MF_REGISTER_shmat, "shmat result");
  }
#endif
  return p;
}
#endif

#ifdef WRAP_shmdt
#include <sys/ipc.h>
#include <sys/shm.h>
WRAPPER2(int, shmdt, const void *shmaddr)
{
  int resp;
  TRACE ("%s\n", __PRETTY_FUNCTION__);
  resp = shmdt (shmaddr);
#ifdef MF_REGISTER_shmat
  __mf_unregister ((void *)shmaddr, 0);
#endif
  return resp;
}
#endif

