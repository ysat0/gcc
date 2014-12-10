/* Definitions for simple data type for real numbers.
   Copyright (C) 2002-2014 Free Software Foundation, Inc.

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

#ifndef GCC_SREAL_H
#define GCC_SREAL_H

/* SREAL_PART_BITS has to be an even number.  */
#define SREAL_PART_BITS 32

#define UINT64_BITS	64

#define SREAL_MIN_SIG ((int64_t) 1 << (SREAL_PART_BITS - 2))
#define SREAL_MAX_SIG (((int64_t) 1 << (SREAL_PART_BITS - 1)) - 1)
#define SREAL_MAX_EXP (INT_MAX / 4)

#define SREAL_BITS SREAL_PART_BITS

/* Structure for holding a simple real number.  */
class sreal
{
public:
  /* Construct an uninitialized sreal.  */
  sreal () : m_sig (-1), m_exp (-1) {}

  /* Construct a sreal.  */
  sreal (int64_t sig, int exp = 0) : m_sig (sig), m_exp (exp)
  {
    normalize ();
  }

  void dump (FILE *) const;
  int64_t to_int () const;
  sreal operator+ (const sreal &other) const;
  sreal operator- (const sreal &other) const;
  sreal operator* (const sreal &other) const;
  sreal operator/ (const sreal &other) const;

  bool operator< (const sreal &other) const
  {
    if (m_exp == other.m_exp)
      return m_sig < other.m_sig;
    else
    {
      bool negative = m_sig < 0;
      bool other_negative = other.m_sig < 0;

      if (negative != other_negative)
        return negative > other_negative;

      bool r = m_exp < other.m_exp;
      return negative ? !r : r;
    }
  }

  bool operator== (const sreal &other) const
  {
    return m_exp == other.m_exp && m_sig == other.m_sig;
  }

  sreal operator- () const
  {
    sreal tmp = *this;
    tmp.m_sig *= -1;

    return tmp;
  }

  sreal shift (int s) const
  {
    gcc_checking_assert (s <= SREAL_BITS);
    gcc_checking_assert (s >= -SREAL_BITS);

    /* Exponent should never be so large because shift_right is used only by
     sreal_add and sreal_sub ant thus the number cannot be shifted out from
     exponent range.  */
    gcc_checking_assert (m_exp + s <= SREAL_MAX_EXP);
    gcc_checking_assert (m_exp + s >= -SREAL_MAX_EXP);

    sreal tmp = *this;
    tmp.m_exp += s;

    return tmp;
  }

  /* Global minimum sreal can hold.  */
  inline static sreal min ()
  {
    static sreal min = sreal (-SREAL_MAX_SIG, SREAL_MAX_EXP);
    return min;
  }

  /* Global minimum sreal can hold.  */
  inline static sreal max ()
  {
    static sreal max = sreal (SREAL_MAX_SIG, SREAL_MAX_EXP);
    return max;
  }

private:
  void normalize ();
  void shift_right (int amount);
  static sreal signedless_plus (const sreal &a, const sreal &b, bool negative);
  static sreal signedless_minus (const sreal &a, const sreal &b, bool negative);

  int64_t m_sig;			/* Significant.  */
  signed int m_exp;			/* Exponent.  */
};

extern void debug (const sreal &ref);
extern void debug (const sreal *ptr);

inline sreal &operator+= (sreal &a, const sreal &b)
{
  return a = a + b;
}

inline sreal &operator-= (sreal &a, const sreal &b)
{
  return a = a - b;
}

inline sreal &operator/= (sreal &a, const sreal &b)
{
  return a = a / b;
}

inline sreal &operator*= (sreal &a, const sreal &b)
{
  return a = a  * b;
}

inline bool operator!= (const sreal &a, const sreal &b)
{
  return !(a == b);
}

inline bool operator> (const sreal &a, const sreal &b)
{
  return !(a == b || a < b);
}

inline bool operator<= (const sreal &a, const sreal &b)
{
  return a < b || a == b;
}

inline bool operator>= (const sreal &a, const sreal &b)
{
  return a == b || a > b;
}

inline sreal operator<< (const sreal &a, int exp)
{
  return a.shift (exp);
}

inline sreal operator>> (const sreal &a, int exp)
{
  return a.shift (-exp);
}

#endif
