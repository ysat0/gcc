/* CPP Library - charsets
   Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003
   Free Software Foundation, Inc.

   Broken out of c-lex.c Apr 2003, adding valid C99 UCN ranges.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option) any
later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "cpplib.h"
#include "cpphash.h"
#include "cppucnid.h"

/* Character set handling for C-family languages.

   Terminological note: In what follows, "charset" or "character set"
   will be taken to mean both an abstract set of characters and an
   encoding for that set.

   The C99 standard discusses two character sets: source and execution.
   The source character set is used for internal processing in translation
   phases 1 through 4; the execution character set is used thereafter.
   Both are required by 5.2.1.2p1 to be multibyte encodings, not wide
   character encodings (see 3.7.2, 3.7.3 for the standardese meanings
   of these terms).  Furthermore, the "basic character set" (listed in
   5.2.1p3) is to be encoded in each with values one byte wide, and is
   to appear in the initial shift state.

   It is not explicitly mentioned, but there is also a "wide execution
   character set" used to encode wide character constants and wide
   string literals; this is supposed to be the result of applying the
   standard library function mbstowcs() to an equivalent narrow string
   (6.4.5p5).  However, the behavior of hexadecimal and octal
   \-escapes is at odds with this; they are supposed to be translated
   directly to wchar_t values (6.4.4.4p5,6).

   The source character set is not necessarily the character set used
   to encode physical source files on disk; translation phase 1 converts
   from whatever that encoding is to the source character set.

   The presence of universal character names in C99 (6.4.3 et seq.)
   forces the source character set to be isomorphic to ISO 10646,
   that is, Unicode.  There is no such constraint on the execution
   character set; note also that the conversion from source to
   execution character set does not occur for identifiers (5.1.1.2p1#5).

   For convenience of implementation, the source character set's
   encoding of the basic character set should be identical to the
   execution character set OF THE HOST SYSTEM's encoding of the basic
   character set, and it should not be a state-dependent encoding.

   cpplib uses UTF-8 or UTF-EBCDIC for the source character set,
   depending on whether the host is based on ASCII or EBCDIC (see
   respectively Unicode section 2.3/ISO10646 Amendment 2, and Unicode
   Technical Report #16).  It relies on the system library's iconv()
   primitive to do charset conversion (specified in SUSv2).  If this
   primitive is not present, the source and execution character sets
   must be identical and are limited to the basic ASCII or EBCDIC
   range, and wide characters are implemented by padding narrow
   characters to the size of wchar_t.  */

#if !HAVE_ICONV
/* Make certain that the uses of iconv(), iconv_open(), iconv_close()
   below, which are guarded only by if statements with compile-time
   constant conditions, do not cause link errors.  */
#define iconv_open(x, y) (errno = EINVAL, (iconv_t)-1)
#define iconv(a,b,c,d,e) (errno = EINVAL, (size_t)-1)
#define iconv_close(x)   0
#endif

#if HOST_CHARSET == HOST_CHARSET_ASCII
#define SOURCE_CHARSET "UTF-8"
#elif HOST_CHARSET == HOST_CHARSET_EBCDIC
#define SOURCE_CHARSET "UTF-EBCDIC"
#else
#error "Unrecognized basic host character set"
#endif

/* This structure is used for a resizable string buffer, mostly by
   convert_cset and cpp_interpret_string.  */
struct strbuf
{
  uchar *text;
  size_t asize;
  size_t len;
};

/* This is enough to hold any string that fits on a single 80-column
   line, even if iconv quadruples its size (e.g. conversion from
   ASCII to UCS-4) rounded up to a power of two.  */
#define OUTBUF_BLOCK_SIZE 256

/* Subroutine of cpp_init_iconv: initialize and return an iconv
   descriptor for conversion from FROM to TO.  If iconv_open() fails,
   issue an error and return (iconv_t) -1.  Silently return
   (iconv_t) -1 if FROM and TO are identical.  */
static iconv_t
init_iconv_desc (cpp_reader *pfile, const char *to, const char *from)
{
  iconv_t dsc;

  if (!strcmp (to, from))
    return (iconv_t) -1;

  dsc = iconv_open (to, from);
  if (dsc == (iconv_t) -1)
    {
      if (errno == EINVAL)
	cpp_error (pfile, DL_ERROR, /* XXX should be DL_SORRY */
		   "conversion from %s to %s not supported by iconv",
		   from, to);
      else
	cpp_errno (pfile, DL_ERROR, "iconv_open");
    }
  return dsc;
}

/* If charset conversion is requested, initialize iconv(3) descriptors
   for conversion from the source character set to the execution
   character sets.  If iconv is not present in the C library, and
   conversion is requested, issue an error.  */

void
cpp_init_iconv (cpp_reader *pfile)
{
  const char *ncset = CPP_OPTION (pfile, narrow_charset);
  const char *wcset = CPP_OPTION (pfile, wide_charset);
  const char *default_wcset;

  bool be = CPP_OPTION (pfile, bytes_big_endian);

  if (CPP_OPTION (pfile, wchar_precision) >= 32)
    default_wcset = be ? "UCS-4BE" : "UCS-4LE";
  else if (CPP_OPTION (pfile, wchar_precision) >= 16)
    default_wcset = be ? "UCS-2BE" : "UCS-2LE";
  else
    /* This effectively means that wide strings are not supported,
       so don't do any conversion at all.  */
   default_wcset = SOURCE_CHARSET;

  if (!HAVE_ICONV)
    {
      if (ncset && strcmp (ncset, SOURCE_CHARSET))
	cpp_error (pfile, DL_ERROR,  /* XXX should be DL_SORRY */
		   "no iconv implementation, cannot convert to %s", ncset);

      if (wcset && strcmp (wcset, default_wcset))
	cpp_error (pfile, DL_ERROR,  /* XXX should be DL_SORRY */
		   "no iconv implementation, cannot convert to %s", wcset);
    }
  else
    {
      if (!ncset)
	ncset = SOURCE_CHARSET;
      if (!wcset)
	wcset = default_wcset;

      pfile->narrow_cset_desc = init_iconv_desc (pfile, ncset, SOURCE_CHARSET);
      pfile->wide_cset_desc = init_iconv_desc (pfile, wcset, SOURCE_CHARSET);
    }
}

void
_cpp_destroy_iconv (cpp_reader *pfile)
{
  if (HAVE_ICONV)
    {
      if (pfile->narrow_cset_desc != (iconv_t) -1)
	iconv_close (pfile->narrow_cset_desc);
      if (pfile->wide_cset_desc != (iconv_t) -1)
	iconv_close (pfile->wide_cset_desc);
    }
}

/* iconv(3) utility wrapper.  Convert the string FROM, of length FLEN,
   according to the iconv descriptor CD.  The result is appended to
   the string buffer TO.  If DESC is (iconv_t)-1 or iconv is not
   available, the string is simply copied into TO.

   Returns true on success, false on error.  */

static bool
convert_cset (iconv_t cd, const uchar *from, size_t flen, struct strbuf *to)
{
  if (!HAVE_ICONV || cd == (iconv_t)-1)
    {
      if (to->len + flen > to->asize)
	{
	  to->asize = to->len + flen;
	  to->text = xrealloc (to->text, to->asize);
	}
      memcpy (to->text + to->len, from, flen);
      to->len += flen;
      return true;
    }
  else
    {
      char *inbuf, *outbuf;
      size_t inbytesleft, outbytesleft;

      /* Reset conversion descriptor and check that it is valid.  */
      if (iconv (cd, 0, 0, 0, 0) == (size_t)-1)
	return false;

      inbuf = (char *)from;
      inbytesleft = flen;
      outbuf = (char *)to->text + to->len;
      outbytesleft = to->asize - to->len;

      for (;;)
	{
	  iconv (cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
	  if (__builtin_expect (inbytesleft == 0, 1))
	    {
	      to->len = to->asize - outbytesleft;
	      return true;
	    }
	  if (errno != E2BIG)
	    return false;

	  outbytesleft += OUTBUF_BLOCK_SIZE;
	  to->asize += OUTBUF_BLOCK_SIZE;
	  to->text = xrealloc (to->text, to->asize);
	  outbuf = (char *)to->text + to->asize - outbytesleft;
	}
    }
}

/* Utility routine that computes a mask of the form 0000...111... with
   WIDTH 1-bits.  */
static inline size_t
width_to_mask (size_t width)
{
  width = MIN (width, BITS_PER_CPPCHAR_T);
  if (width >= CHAR_BIT * sizeof (size_t))
    return ~(size_t) 0;
  else
    return ((size_t) 1 << width) - 1;
}



/* Returns 1 if C is valid in an identifier, 2 if C is valid except at
   the start of an identifier, and 0 if C is not valid in an
   identifier.  We assume C has already gone through the checks of
   _cpp_valid_ucn.  The algorithm is a simple binary search on the
   table defined in cppucnid.h.  */

static int
ucn_valid_in_identifier (cpp_reader *pfile, cppchar_t c)
{
  int mn, mx, md;

  mn = -1;
  mx = ARRAY_SIZE (ucnranges);
  while (mx - mn > 1)
    {
      md = (mn + mx) / 2;
      if (c < ucnranges[md].lo)
	mx = md;
      else if (c > ucnranges[md].hi)
	mn = md;
      else
	goto found;
    }
  return 0;

 found:
  /* When -pedantic, we require the character to have been listed by
     the standard for the current language.  Otherwise, we accept the
     union of the acceptable sets for C++98 and C99.  */
  if (CPP_PEDANTIC (pfile)
      && ((CPP_OPTION (pfile, c99) && !(ucnranges[md].flags & C99))
	  || (CPP_OPTION (pfile, cplusplus)
	      && !(ucnranges[md].flags & CXX))))
    return 0;

  /* In C99, UCN digits may not begin identifiers.  */
  if (CPP_OPTION (pfile, c99) && (ucnranges[md].flags & DIG))
    return 2;

  return 1;
}

/* [lex.charset]: The character designated by the universal character
   name \UNNNNNNNN is that character whose character short name in
   ISO/IEC 10646 is NNNNNNNN; the character designated by the
   universal character name \uNNNN is that character whose character
   short name in ISO/IEC 10646 is 0000NNNN.  If the hexadecimal value
   for a universal character name is less than 0x20 or in the range
   0x7F-0x9F (inclusive), or if the universal character name
   designates a character in the basic source character set, then the
   program is ill-formed.

   *PSTR must be preceded by "\u" or "\U"; it is assumed that the
   buffer end is delimited by a non-hex digit.  Returns zero if UCNs
   are not part of the relevant standard, or if the string beginning
   at *PSTR doesn't syntactically match the form 'NNNN' or 'NNNNNNNN'.

   Otherwise the nonzero value of the UCN, whether valid or invalid,
   is returned.  Diagnostics are emitted for invalid values.  PSTR
   is updated to point one beyond the UCN, or to the syntactically
   invalid character.

   IDENTIFIER_POS is 0 when not in an identifier, 1 for the start of
   an identifier, or 2 otherwise.
*/

cppchar_t
_cpp_valid_ucn (cpp_reader *pfile, const uchar **pstr,
		const uchar *limit, int identifier_pos)
{
  cppchar_t result, c;
  unsigned int length;
  const uchar *str = *pstr;
  const uchar *base = str - 2;

  if (!CPP_OPTION (pfile, cplusplus) && !CPP_OPTION (pfile, c99))
    cpp_error (pfile, DL_WARNING,
	       "universal character names are only valid in C++ and C99");
  else if (CPP_WTRADITIONAL (pfile) && identifier_pos == 0)
    cpp_error (pfile, DL_WARNING,
	       "the meaning of '\\%c' is different in traditional C",
	       (int) str[-1]);

  if (str[-1] == 'u')
    length = 4;
  else if (str[-1] == 'U')
    length = 8;
  else
    abort();

  result = 0;
  do
    {
      c = *str;
      if (!ISXDIGIT (c))
	break;
      str++;
      result = (result << 4) + hex_value (c);
    }
  while (--length && str < limit);

  *pstr = str;
  if (length)
    {
      /* We'll error when we try it out as the start of an identifier.  */
      cpp_error (pfile, DL_ERROR, "incomplete universal character name %.*s",
		 (int) (str - base), base);
      result = 1;
    }
  /* The standard permits $, @ and ` to be specified as UCNs.  We use
     hex escapes so that this also works with EBCDIC hosts.  */
  else if ((result < 0xa0
	    && (result != 0x24 && result != 0x40 && result != 0x60))
	   || (result & 0x80000000)
	   || (result >= 0xD800 && result <= 0xDFFF))
    {
      cpp_error (pfile, DL_ERROR, "%.*s is not a valid universal character",
		 (int) (str - base), base);
      result = 1;
    }
  else if (identifier_pos)
    {
      int validity = ucn_valid_in_identifier (pfile, result);

      if (validity == 0)
	cpp_error (pfile, DL_ERROR,
		   "universal character %.*s is not valid in an identifier",
		   (int) (str - base), base);
      else if (validity == 2 && identifier_pos == 1)
	cpp_error (pfile, DL_ERROR,
   "universal character %.*s is not valid at the start of an identifier",
		   (int) (str - base), base);
    }
  /* We don't accept UCNs if iconv is not available or will not
     convert to the target wide character set.  */
  else if (!HAVE_ICONV || pfile->wide_cset_desc == (iconv_t) -1)
    {
      /* XXX should be DL_SORRY */
      cpp_error (pfile, DL_ERROR,
	"universal character names are not supported in this configuration");
    }


  if (result == 0)
    result = 1;

  return result;
}

/* Convert an UCN, pointed to by FROM, to UTF-8 encoding, then translate
   it to the execution character set and write the result into TBUF.
   An advanced pointer is returned.  Issues all relevant diagnostics.

   UTF-8 encoding looks like this:

   value range	       encoded as
   00000000-0000007F   0xxxxxxx
   00000080-000007FF   110xxxxx 10xxxxxx
   00000800-0000FFFF   1110xxxx 10xxxxxx 10xxxxxx
   00010000-001FFFFF   11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
   00200000-03FFFFFF   111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
   04000000-7FFFFFFF   1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx

   Values in the 0000D800 ... 0000DFFF range (surrogates) are invalid,
   which means that three-byte sequences ED xx yy, with A0 <= xx <= BF,
   never occur.  Note also that any value that can be encoded by a
   given row of the table can also be encoded by all successive rows,
   but this is not done; only the shortest possible encoding for any
   given value is valid.  For instance, the character 07C0 could be
   encoded as any of DF 80, E0 9F 80, F0 80 9F 80, F8 80 80 9F 80, or
   FC 80 80 80 9F 80.  Only the first is valid.  */

static const uchar *
convert_ucn (cpp_reader *pfile, const uchar *from, const uchar *limit,
	     struct strbuf *tbuf, bool wide)
{
  int nbytes;
  uchar buf[6], *p = &buf[6];
  static const uchar masks[6] = { 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };
  cppchar_t ucn;

  from++; /* skip u/U */
  ucn = _cpp_valid_ucn (pfile, &from, limit, 0);
  if (!ucn)
    return from;

  nbytes = 1;
  if (ucn < 0x80)
    *--p = ucn;
  else
    {
      do
	{
	  *--p = ((ucn & 0x3F) | 0x80);
	  ucn >>= 6;
	  nbytes++;
	}
      while (ucn >= 0x3F || (ucn & masks[nbytes-1]));
      *--p = (ucn | masks[nbytes-1]);
    }

  if (!convert_cset (wide ? pfile->wide_cset_desc : pfile->narrow_cset_desc,
		     p, nbytes, tbuf))
    cpp_errno (pfile, DL_ERROR, "converting UCN to execution character set");

  return from;
}

static void
emit_numeric_escape (cpp_reader *pfile, cppchar_t n,
		     struct strbuf *tbuf, bool wide)
{
  if (wide)
    {
      /* We have to render this into the target byte order, which may not
	 be our byte order.  */
      bool bigend = CPP_OPTION (pfile, bytes_big_endian);
      size_t width = CPP_OPTION (pfile, wchar_precision);
      size_t cwidth = CPP_OPTION (pfile, char_precision);
      size_t cmask = width_to_mask (cwidth);
      size_t nbwc = width / cwidth;
      size_t i;
      size_t off = tbuf->len;
      cppchar_t c;

      if (tbuf->len + nbwc > tbuf->asize)
	{
	  tbuf->asize += OUTBUF_BLOCK_SIZE;
	  tbuf->text = xrealloc (tbuf->text, tbuf->asize);
	}

      for (i = 0; i < nbwc; i++)
	{
	  c = n & cmask;
	  n >>= cwidth;
	  tbuf->text[off + (bigend ? nbwc - i - 1 : i)] = c;
	}
      tbuf->len += nbwc;
    }
  else
    {
      if (tbuf->len + 1 > tbuf->asize)
	{
	  tbuf->asize += OUTBUF_BLOCK_SIZE;
	  tbuf->text = xrealloc (tbuf->text, tbuf->asize);
	}
      tbuf->text[tbuf->len++] = n;
    }
}

/* Convert a hexadecimal escape, pointed to by FROM, to the execution
   character set and write it into the string buffer TBUF.  Returns an
   advanced pointer, and issues diagnostics as necessary.
   No character set translation occurs; this routine always produces the
   execution-set character with numeric value equal to the given hex
   number.  You can, e.g. generate surrogate pairs this way.  */
static const uchar *
convert_hex (cpp_reader *pfile, const uchar *from, const uchar *limit,
	     struct strbuf *tbuf, bool wide)
{
  cppchar_t c, n = 0, overflow = 0;
  int digits_found = 0;
  size_t width = (wide ? CPP_OPTION (pfile, wchar_precision)
		  : CPP_OPTION (pfile, char_precision));
  size_t mask = width_to_mask (width);

  if (CPP_WTRADITIONAL (pfile))
    cpp_error (pfile, DL_WARNING,
	       "the meaning of '\\x' is different in traditional C");

  from++;  /* skip 'x' */
  while (from < limit)
    {
      c = *from;
      if (! hex_p (c))
	break;
      from++;
      overflow |= n ^ (n << 4 >> 4);
      n = (n << 4) + hex_value (c);
      digits_found = 1;
    }

  if (!digits_found)
    {
      cpp_error (pfile, DL_ERROR,
		 "\\x used with no following hex digits");
      return from;
    }

  if (overflow | (n != (n & mask)))
    {
      cpp_error (pfile, DL_PEDWARN,
		 "hex escape sequence out of range");
      n &= mask;
    }

  emit_numeric_escape (pfile, n, tbuf, wide);

  return from;
}

/* Convert an octal escape, pointed to by FROM, to the execution
   character set and write it into the string buffer TBUF.  Returns an
   advanced pointer, and issues diagnostics as necessary.
   No character set translation occurs; this routine always produces the
   execution-set character with numeric value equal to the given octal
   number.  */
static const uchar *
convert_oct (cpp_reader *pfile, const uchar *from, const uchar *limit,
	     struct strbuf *tbuf, bool wide)
{
  size_t count = 0;
  cppchar_t c, n = 0;
  size_t width = (wide ? CPP_OPTION (pfile, wchar_precision)
		  : CPP_OPTION (pfile, char_precision));
  size_t mask = width_to_mask (width);
  bool overflow = false;

  while (from < limit && count++ < 3)
    {
      c = *from;
      if (c < '0' || c > '7')
	break;
      from++;
      overflow |= n ^ (n << 3 >> 3);
      n = (n << 3) + c - '0';
    }

  if (n != (n & mask))
    {
      cpp_error (pfile, DL_PEDWARN,
		 "octal escape sequence out of range");
      n &= mask;
    }

  emit_numeric_escape (pfile, n, tbuf, wide);

  return from;
}

/* Convert an escape sequence (pointed to by FROM) to its value on
   the target, and to the execution character set.  Do not scan past
   LIMIT.  Write the converted value into TBUF.  Returns an advanced
   pointer.  Handles all relevant diagnostics.  */
static const uchar *
convert_escape (cpp_reader *pfile, const uchar *from, const uchar *limit,
		struct strbuf *tbuf, bool wide)
{
  /* Values of \a \b \e \f \n \r \t \v respectively.  */
#if HOST_CHARSET == HOST_CHARSET_ASCII
  static const uchar charconsts[] = {  7,  8, 27, 12, 10, 13,  9, 11 };
#elif HOST_CHARSET == HOST_CHARSET_EBCDIC
  static const uchar charconsts[] = { 47, 22, 39, 12, 21, 13,  5, 11 };
#else
#error "unknown host character set"
#endif

  uchar c;

  c = *from;
  switch (c)
    {
      /* UCNs, hex escapes, and octal escapes are processed separately.  */
    case 'u': case 'U':
      return convert_ucn (pfile, from, limit, tbuf, wide);

    case 'x':
      return convert_hex (pfile, from, limit, tbuf, wide);
      break;

    case '0':  case '1':  case '2':  case '3':
    case '4':  case '5':  case '6':  case '7':
      return convert_oct (pfile, from, limit, tbuf, wide);

      /* Various letter escapes.  Get the appropriate host-charset
	 value into C.  */
    case '\\': case '\'': case '"': case '?': break;

    case '(': case '{': case '[': case '%':
      /* '\(', etc, can be used at the beginning of a line in a long
	 string split onto multiple lines with \-newline, to prevent
	 Emacs or other text editors from getting confused.  '\%' can
	 be used to prevent SCCS from mangling printf format strings.  */
      if (CPP_PEDANTIC (pfile))
	goto unknown;
      break;

    case 'b': c = charconsts[1];  break;
    case 'f': c = charconsts[3];  break;
    case 'n': c = charconsts[4];  break;
    case 'r': c = charconsts[5];  break;
    case 't': c = charconsts[6];  break;
    case 'v': c = charconsts[7];  break;

    case 'a':
      if (CPP_WTRADITIONAL (pfile))
	cpp_error (pfile, DL_WARNING,
		   "the meaning of '\\a' is different in traditional C");
      c = charconsts[0];
      break;

    case 'e': case 'E':
      if (CPP_PEDANTIC (pfile))
	cpp_error (pfile, DL_PEDWARN,
		   "non-ISO-standard escape sequence, '\\%c'", (int) c);
      c = charconsts[2];
      break;

    default:
    unknown:
      if (ISGRAPH (c))
	cpp_error (pfile, DL_PEDWARN,
		   "unknown escape sequence '\\%c'", (int) c);
      else
	cpp_error (pfile, DL_PEDWARN,
		   "unknown escape sequence: '\\%03o'", (int) c);
    }

  /* Now convert what we have to the execution character set.  */
  if (!convert_cset (wide ? pfile->wide_cset_desc : pfile->narrow_cset_desc,
		     &c, 1, tbuf))
    cpp_errno (pfile, DL_ERROR,
	       "converting escape sequence to execution character set");

  return from + 1;
}

/* FROM is an array of cpp_string structures of length COUNT.  These
   are to be converted from the source to the execution character set,
   escape sequences translated, and finally all are to be
   concatenated.  WIDE indicates whether or not to produce a wide
   string.  The result is written into TO.  Returns true for success,
   false for failure.  */
bool
cpp_interpret_string (cpp_reader *pfile, const cpp_string *from, size_t count,
		      cpp_string *to, bool wide)
{
  struct strbuf tbuf;
  const uchar *p, *base, *limit;
  size_t i;
  iconv_t cd = wide ? pfile->wide_cset_desc : pfile->narrow_cset_desc;

  tbuf.asize = MAX (OUTBUF_BLOCK_SIZE, from->len);
  tbuf.text = xmalloc (tbuf.asize);
  tbuf.len = 0;

  for (i = 0; i < count; i++)
    {
      p = from[i].text;
      if (*p == 'L') p++;
      p++; /* skip leading quote */
      limit = from[i].text + from[i].len - 1; /* skip trailing quote */

      for (;;)
	{
	  base = p;
	  while (p < limit && *p != '\\')
	    p++;
	  if (p > base)
	    {
	      /* We have a run of normal characters; these can be fed
		 directly to convert_cset.  */
	      if (!convert_cset (cd, base, p - base, &tbuf))
		goto fail;
	    }
	  if (p == limit)
	    break;

	  p = convert_escape (pfile, p + 1, limit, &tbuf, wide);
	}
    }
  /* NUL-terminate the 'to' buffer and translate it to a cpp_string
     structure.  */
  emit_numeric_escape (pfile, 0, &tbuf, wide);
  tbuf.text = xrealloc (tbuf.text, tbuf.len);
  to->text = tbuf.text;
  to->len = tbuf.len;
  return true;

 fail:
  cpp_errno (pfile, DL_ERROR, "converting to execution character set");
  free (tbuf.text);
  return false;
}

/* Subroutine of cpp_interpret_charconst which performs the conversion
   to a number, for narrow strings.  STR is the string structure returned
   by cpp_interpret_string.  PCHARS_SEEN and UNSIGNEDP are as for
   cpp_interpret_charconst.  */
static cppchar_t
narrow_str_to_charconst (cpp_reader *pfile, cpp_string str,
			 unsigned int *pchars_seen, int *unsignedp)
{
  size_t width = CPP_OPTION (pfile, char_precision);
  size_t max_chars = CPP_OPTION (pfile, int_precision) / width;
  size_t mask = width_to_mask (width);
  size_t i;
  cppchar_t result, c;
  bool unsigned_p;

  /* The value of a multi-character character constant, or a
     single-character character constant whose representation in the
     execution character set is more than one byte long, is
     implementation defined.  This implementation defines it to be the
     number formed by interpreting the byte sequence in memory as a
     big-endian binary number.  If overflow occurs, the high bytes are
     lost, and a warning is issued.

     We don't want to process the NUL terminator handed back by
     cpp_interpret_string.  */
  result = 0;
  for (i = 0; i < str.len - 1; i++)
    {
      c = str.text[i] & mask;
      if (width < BITS_PER_CPPCHAR_T)
	result = (result << width) | c;
      else
	result = c;
    }

  if (i > max_chars)
    {
      i = max_chars;
      cpp_error (pfile, DL_WARNING, "character constant too long for its type");
    }
  else if (i > 1 && CPP_OPTION (pfile, warn_multichar))
    cpp_error (pfile, DL_WARNING, "multi-character character constant");

  /* Multichar constants are of type int and therefore signed.  */
  if (i > 1)
    unsigned_p = 0;
  else
    unsigned_p = CPP_OPTION (pfile, unsigned_char);

  /* Truncate the constant to its natural width, and simultaneously
     sign- or zero-extend to the full width of cppchar_t.
     For single-character constants, the value is WIDTH bits wide.
     For multi-character constants, the value is INT_PRECISION bits wide.  */
  if (i > 1)
    width = CPP_OPTION (pfile, int_precision);
  if (width < BITS_PER_CPPCHAR_T)
    {
      mask = ((cppchar_t) 1 << width) - 1;
      if (unsigned_p || !(result & (1 << (width - 1))))
	result &= mask;
      else
	result |= ~mask;
    }
  *pchars_seen = i;
  *unsignedp = unsigned_p;
  return result;
}
			 
/* Subroutine of cpp_interpret_charconst which performs the conversion
   to a number, for wide strings.  STR is the string structure returned
   by cpp_interpret_string.  PCHARS_SEEN and UNSIGNEDP are as for
   cpp_interpret_charconst.  */
static cppchar_t
wide_str_to_charconst (cpp_reader *pfile, cpp_string str,
		       unsigned int *pchars_seen, int *unsignedp)
{
  bool bigend = CPP_OPTION (pfile, bytes_big_endian);
  size_t width = CPP_OPTION (pfile, wchar_precision);
  size_t cwidth = CPP_OPTION (pfile, char_precision);
  size_t mask = width_to_mask (width);
  size_t cmask = width_to_mask (cwidth);
  size_t nbwc = width / cwidth;
  size_t off, i;
  cppchar_t result = 0, c;

  /* This is finicky because the string is in the target's byte order,
     which may not be our byte order.  Only the last character, ignoring
     the NUL terminator, is relevant.  */
  off = str.len - (nbwc * 2);
  result = 0;
  for (i = 0; i < nbwc; i++)
    {
      c = bigend ? str.text[off + i] : str.text[off + nbwc - i - 1];
      result = (result << cwidth) | (c & cmask);
    }

  /* Wide character constants have type wchar_t, and a single
     character exactly fills a wchar_t, so a multi-character wide
     character constant is guaranteed to overflow.  */
  if (off > 0)
    cpp_error (pfile, DL_WARNING, "character constant too long for its type");

  /* Truncate the constant to its natural width, and simultaneously
     sign- or zero-extend to the full width of cppchar_t.  */
  if (width < BITS_PER_CPPCHAR_T)
    {
      if (CPP_OPTION (pfile, unsigned_wchar) || !(result & (1 << (width - 1))))
	result &= mask;
      else
	result |= ~mask;
    }

  *unsignedp = CPP_OPTION (pfile, unsigned_wchar);
  *pchars_seen = 1;
  return result;
}

/* Interpret a (possibly wide) character constant in TOKEN.
   PCHARS_SEEN points to a variable that is filled in with the number
   of characters seen, and UNSIGNEDP to a variable that indicates
   whether the result has signed type.  */
cppchar_t
cpp_interpret_charconst (cpp_reader *pfile, const cpp_token *token,
			 unsigned int *pchars_seen, int *unsignedp)
{
  cpp_string str = { 0, 0 };
  bool wide = (token->type == CPP_WCHAR);
  cppchar_t result;

  /* an empty constant will appear as L'' or '' */
  if (token->val.str.len == (size_t) (2 + wide))
    {
      cpp_error (pfile, DL_ERROR, "empty character constant");
      return 0;
    }
  else if (!cpp_interpret_string (pfile, &token->val.str, 1, &str, wide))
    return 0;

  if (wide)
    result = wide_str_to_charconst (pfile, str, pchars_seen, unsignedp);
  else
    result = narrow_str_to_charconst (pfile, str, pchars_seen, unsignedp);

  if (str.text != token->val.str.text)
    free ((void *)str.text);

  return result;
}
