// 2001-09-17 Benjamin Kosnik  <bkoz@redhat.com>

// Copyright (C) 2001, 2002, 2003 Free Software Foundation
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

// 22.2.5.3.1 time_put members

#include <locale>
#include <sstream>
#include <testsuite_hooks.h>

void test01()
{
  using namespace std;
  typedef ostreambuf_iterator<char> iterator_type;

  bool test = true;

  // create "C" time objects
  tm time1 = { 0, 0, 12, 4, 3, 71 };
  const char* all = "%a %A %b %B %c %d %H %I %j %m %M %p %s %U "
    "%w %W %x %X %y %Y %Z %%";
  const char* date = "%A, the second of %B";
  const char* date_ex = "%Ex";

  // basic construction and sanity checks.
  locale loc_c = locale::classic();
  locale loc_hk("en_HK");
  locale loc_fr("fr_FR@euro");
  locale loc_de("de_DE");
  VERIFY( loc_hk != loc_c );
  VERIFY( loc_hk != loc_fr );
  VERIFY( loc_hk != loc_de );
  VERIFY( loc_de != loc_fr );

  // cache the __timepunct facets, for quicker gdb inspection
  const __timepunct<char>& time_c = use_facet<__timepunct<char> >(loc_c); 
  const __timepunct<char>& time_de = use_facet<__timepunct<char> >(loc_de); 
  const __timepunct<char>& time_hk = use_facet<__timepunct<char> >(loc_hk); 
  const __timepunct<char>& time_fr = use_facet<__timepunct<char> >(loc_fr); 

  // create an ostream-derived object, cache the time_put facet
  const string empty;
  ostringstream oss;
  oss.imbue(loc_c);
  const time_put<char>& tim_put = use_facet<time_put<char> >(oss.getloc()); 

  // 1
  // iter_type 
  // put(iter_type s, ios_base& str, char_type fill, const tm* t,
  //	 char format, char modifier = 0) const;
  oss.str(empty);
  iterator_type os_it01 = tim_put.put(oss.rdbuf(), oss, '*', &time1, 'a');
  string result1 = oss.str();
  VERIFY( result1 == "Sun" );

  oss.str(empty);
  iterator_type os_it21 = tim_put.put(oss.rdbuf(), oss, '*', &time1, 'x');
  string result21 = oss.str(); // "04/04/71"
  oss.str(empty);
  iterator_type os_it22 = tim_put.put(oss.rdbuf(), oss, '*', &time1, 'X');
  string result22 = oss.str(); // "12:00:00"
  oss.str(empty);
  iterator_type os_it31 = tim_put.put(oss.rdbuf(), oss, '*', &time1, 'x', 'E');
  string result31 = oss.str(); // "04/04/71"
  oss.str(empty);
  iterator_type os_it32 = tim_put.put(oss.rdbuf(), oss, '*', &time1, 'X', 'E');
  string result32 = oss.str(); // "12:00:00"
}

int main()
{
  __gnu_cxx_test::run_test_wrapped_generic_locale_exception_catcher(test01);
  return 0;
}
