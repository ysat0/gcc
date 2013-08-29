// class template regex -*- C++ -*-

// Copyright (C) 2010-2013 Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 3, or (at your option)
// any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// Under Section 7 of GPL version 3, you are granted additional
// permissions described in the GCC Runtime Library Exception, version
// 3.1, as published by the Free Software Foundation.

// You should have received a copy of the GNU General Public License and
// a copy of the GCC Runtime Library Exception along with this program;
// see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
// <http://www.gnu.org/licenses/>.

/**
 *  @file bits/regex_compiler.h
 *  This is an internal header file, included by other library headers.
 *  Do not attempt to use it directly. @headername{regex}
 */

namespace std _GLIBCXX_VISIBILITY(default)
{
namespace __detail
{
_GLIBCXX_BEGIN_NAMESPACE_VERSION

  /**
   * @addtogroup regex-detail
   * @{
   */

  template<typename _CharT, typename _TraitsT>
    struct _BracketMatcher;

  /// Builds an NFA from an input iterator interval.
  template<typename _FwdIter, typename _CharT, typename _TraitsT>
    class _Compiler
    {
    public:
      typedef typename _TraitsT::string_type      _StringT;
      typedef _NFA<_CharT, _TraitsT>              _RegexT;
      typedef regex_constants::syntax_option_type _FlagT;

      _Compiler(_FwdIter __b, _FwdIter __e,
		const _TraitsT& __traits, _FlagT __flags);

      std::shared_ptr<_RegexT>
      _M_get_nfa() const
      { return std::shared_ptr<_RegexT>(new _RegexT(_M_state_store)); }

    private:
      typedef _Scanner<_FwdIter>                              _ScannerT;
      typedef typename _ScannerT::_TokenT                     _TokenT;
      typedef _StateSeq<_CharT, _TraitsT>                     _StateSeqT;
      typedef std::stack<_StateSeqT, std::vector<_StateSeqT>> _StackT;
      typedef _BracketMatcher<_CharT, _TraitsT>               _BMatcherT;

      // accepts a specific token or returns false.
      bool
      _M_match_token(_TokenT __token);

      void
      _M_disjunction();

      void
      _M_alternative();

      bool
      _M_term();

      bool
      _M_assertion();

      void
      _M_quantifier();

      bool
      _M_atom();

      bool
      _M_bracket_expression();

      void
      _M_bracket_list(_BMatcherT& __matcher);

      bool
      _M_follow_list(_BMatcherT& __matcher);

      void
      _M_expression_term(_BMatcherT& __matcher);

      bool
      _M_range_expression(_BMatcherT& __matcher);

      bool
      _M_start_range(_BMatcherT& __matcher);

      bool
      _M_collating_symbol(_BMatcherT& __matcher);

      bool
      _M_equivalence_class(_BMatcherT& __matcher);

      bool
      _M_character_class(_BMatcherT& __matcher);

      int
      _M_cur_int_value(int __radix);

      bool
      _M_try_char();

      _CharT
      _M_get_char();

      const _TraitsT& _M_traits;
      _ScannerT       _M_scanner;
      _StringT        _M_value;
      _RegexT         _M_state_store;
      _StackT         _M_stack;
      _FlagT          _M_flags;
    };

  /// Matches a character range (bracket expression)
  template<typename _CharT, typename _TraitsT>
    struct _BracketMatcher
    {
      typedef typename _TraitsT::char_class_type  _CharClassT;
      typedef typename _TraitsT::string_type      _StringT;
      typedef regex_constants::syntax_option_type _FlagT;

      explicit
      _BracketMatcher(bool __is_non_matching,
		      const _TraitsT& __t,
		      _FlagT __flags)
      : _M_is_non_matching(__is_non_matching), _M_traits(__t),
	_M_flags(__flags), _M_class_set(0)
      { }

      bool
      operator()(_CharT) const;

      void
      _M_add_char(_CharT __c)
      { _M_char_set.push_back(_M_translate(__c)); }

      void
      _M_add_collating_element(const _StringT& __s)
      {
	auto __st = _M_traits.lookup_collatename(__s.data(),
						 __s.data() + __s.size());
	if (__st.empty())
	  __throw_regex_error(regex_constants::error_collate);
	// TODO: digraph
	_M_char_set.push_back(__st[0]);
      }

      void
      _M_add_equivalence_class(const _StringT& __s)
      {
	_M_add_character_class(
	  _M_traits.transform_primary(__s.data(),
				      __s.data() + __s.size()));
      }

      void
      _M_add_character_class(const _StringT& __s)
      {
	auto __st = _M_traits.
	  lookup_classname(__s.data(), __s.data() + __s.size(), _M_is_icase());
	if (__st == 0)
	  __throw_regex_error(regex_constants::error_ctype);
	_M_class_set |= __st;
      }

      void
      _M_make_range(_CharT __l, _CharT __r)
      {
	_M_range_set.push_back(
	  make_pair(_M_get_str(_M_translate(__l)),
		    _M_get_str(_M_translate(__r))));
      }

      _CharT
      _M_translate(_CharT __c) const
      {
	if (_M_flags & regex_constants::collate)
	  if (_M_is_icase())
	    return _M_traits.translate_nocase(__c);
	  else
	    return _M_traits.translate(__c);
	else
	  return __c;
      }

      bool
      _M_is_icase() const
      { return _M_flags & regex_constants::icase; }

      _StringT
      _M_get_str(_CharT __c) const
      {
	_StringT __s(1, __c);
	return _M_traits.transform(__s.begin(), __s.end());
      }

      const _TraitsT&                       _M_traits;
      _FlagT                                _M_flags;
      bool                                  _M_is_non_matching;
      std::vector<_CharT>                   _M_char_set;
      std::vector<pair<_StringT, _StringT>> _M_range_set;
      _CharClassT                           _M_class_set;
    };

 //@} regex-detail
_GLIBCXX_END_NAMESPACE_VERSION
} // namespace __detail
} // namespace std

#include <bits/regex_compiler.tcc>
