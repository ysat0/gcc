// 2001-06-05 Benjamin Kosnik  <bkoz@redhat.com>

// Copyright (C) 2001, 2002 Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 2, or (at your option)
// any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License along
// with this library; see the file COPYING.  If not, write to the Free
// Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
// USA.

// As a special exception, you may use this file as part of a free software
// library without restriction.  Specifically, if other files instantiate
// templates or use macros or inline functions from this file, or you compile
// this file and link it with other files to produce an executable, this
// file does not by itself cause the resulting executable to be covered by
// the GNU General Public License.  This exception does not however
// invalidate any other reasons why the executable file might be covered by
// the GNU General Public License.

// 27.4.2.1.6 class ios_base::init

#include <fstream>
#include <sstream>
#include <iostream>
#include <testsuite_hooks.h>

class gnu_filebuf: public std::filebuf
{
  int i;
public:
  gnu_filebuf(int j = 1): i(j) { }
  ~gnu_filebuf() { --i; }
  int get_i() { return i;}
};

const int initial = 4;
gnu_filebuf buf(initial);

// libstdc++/3045, in a vague way.
void test01()
{
  bool test = true;
  int k1;

  // 1 normal
  k1 = buf.get_i();
  VERIFY( k1 == initial );
  {
    std::cout.rdbuf(&buf);
  }
  k1 = buf.get_i();
  VERIFY( k1 == initial );

  // 2 syncd off
  k1 = buf.get_i();
  VERIFY( k1 == initial );
  {
    std::cout.rdbuf(&buf);
    std::ios_base::sync_with_stdio(false); // make sure doesn't clobber buf
  }
  k1 = buf.get_i();
  VERIFY( k1 == initial );

  // 3 callling init
  k1 = buf.get_i();
  VERIFY( k1 == initial );
  {
    std::cout.rdbuf(&buf);
    std::ios_base::Init make_sure_initialized;
  }
  k1 = buf.get_i();
  VERIFY( k1 == initial );
}

// Non-required instantiations don't have the required facets inbued,
// by default, into the locale object. As such, basic_ios::init is
// required to return a bad_cast for the first use of fill() call.
// See 27.4.4.1
void test02() 
{
  bool test = true;

  // 01: Doesn't call basic_ios::init, which uses ctype<char_type>..
  try
    {
      std::basic_ostringstream<unsigned short> 	oss;
    }
  catch(...)
    { 
      test = false; 
    }

  // 02: Calls basic_ios::init, which uses ctype<char_type>..
  try
    {
      std::basic_string<unsigned short>        	str;
      std::basic_ostringstream<unsigned short> 	oss(str);
      
      // Shouldn't get this far.
      test = false; 

      // Try each member functions for unformatted io.
      // put
      oss.put(324);

      // write
      const unsigned short us[4] = {1246, 433, 520, 0};
      oss.write(us, 4);

      // flush
      oss.flush();
    }
  catch(const std::bad_cast& obj)
    {
      test = true;
    }
  catch(...)
    {
      test = false;
    }
  VERIFY( test );
}

int main()
{
  test01();
  test02();
  return 0;
}
