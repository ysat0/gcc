// 2001-08-27 Benjamin Kosnik  <bkoz@redhat.com>

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

// 22.2.6.2.1 money_put members

#include <locale>
#include <sstream>
#include <testsuite_hooks.h>

// test string version
void test01()
{
  using namespace std;
  typedef money_base::part part;
  typedef money_base::pattern pattern;
  typedef ostreambuf_iterator<char> iterator_type;

  bool test __attribute__((unused)) = true;

  // basic construction
  locale loc_c = locale::classic();
  locale loc_hk = __gnu_test::try_named_locale("en_HK");
  locale loc_fr = __gnu_test::try_named_locale("fr_FR@euro");
  locale loc_de = __gnu_test::try_named_locale("de_DE@euro");
  VERIFY( loc_c != loc_de );
  VERIFY( loc_hk != loc_fr );
  VERIFY( loc_hk != loc_de );
  VERIFY( loc_de != loc_fr );

  // cache the moneypunct facets
  typedef moneypunct<char, true> __money_true;
  typedef moneypunct<char, false> __money_false;

  // sanity check the data is correct.
  const string empty;

  // total EPA budget FY 2002
  const string digits1("720000000000");

  // est. cost, national missile "defense", expressed as a loss in USD 2001
  const string digits2("-10000000000000");  

  // not valid input
  const string digits3("-A"); 

  // input less than frac_digits
  const string digits4("-1");
  
  // cache the money_put facet
  ostringstream oss;
  oss.imbue(loc_de);
  const money_put<char>& mon_put = use_facet<money_put<char> >(oss.getloc()); 

  iterator_type os_it01 = mon_put.put(oss.rdbuf(), true, oss, ' ', digits1);
  string result1 = oss.str();
  VERIFY( result1 == "7.200.000.000,00 ");

  oss.str(empty);
  iterator_type os_it02 = mon_put.put(oss.rdbuf(), false, oss, ' ', digits1);
  string result2 = oss.str();
  VERIFY( result2 == "7.200.000.000,00 ");

  // intl and non-intl versions should be the same.
  VERIFY( result1 == result2 );

  // now try with showbase, to get currency symbol in format
  oss.setf(ios_base::showbase);

  oss.str(empty);
  iterator_type os_it03 = mon_put.put(oss.rdbuf(), true, oss, ' ', digits1);
  string result3 = oss.str();
  VERIFY( result3 == "7.200.000.000,00 EUR ");

  oss.str(empty);
  iterator_type os_it04 = mon_put.put(oss.rdbuf(), false, oss, ' ', digits1);
  string result4 = oss.str();
  VERIFY( result4 == "7.200.000.000,00 \244");

  // intl and non-intl versions should be different.
  VERIFY( result3 != result4 );
  VERIFY( result3 != result1 );
  VERIFY( result4 != result2 );

  oss.unsetf(ios_base::showbase);

  // test io.width() > length
  // test various fill strategies
  oss.str(empty);
  oss.width(20);
  iterator_type os_it10 = mon_put.put(oss.rdbuf(), true, oss, '*', digits4);
  string result10 = oss.str();
  VERIFY( result10 == "***************-,01*");

  oss.str(empty);
  oss.width(20);
  oss.setf(ios_base::internal);
  iterator_type os_it11 = mon_put.put(oss.rdbuf(), true, oss, '*', digits4);
  string result11 = oss.str();
  VERIFY( result11 == "-,01****************");
}

int main()
{
  test01();
  return 0;
}
