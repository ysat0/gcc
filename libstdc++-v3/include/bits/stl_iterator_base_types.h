// Types used in iterator implementation -*- C++ -*-

// Copyright (C) 2001 Free Software Foundation, Inc.
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

/*
 *
 * Copyright (c) 1994
 * Hewlett-Packard Company
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Hewlett-Packard Company makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 *
 * Copyright (c) 1996-1998
 * Silicon Graphics Computer Systems, Inc.
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Silicon Graphics makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 */

/** @file stl_iterator_base_types.h
 *  This is an internal header file, included by other library headers.
 *  You should not attempt to use it directly.
 */

#ifndef __GLIBCPP_INTERNAL_ITERATOR_BASE_TYPES_H
#define __GLIBCPP_INTERNAL_ITERATOR_BASE_TYPES_H

// This file contains all of the general iterator-related utility
// types, such as iterator_traits and struct iterator.
// The internal file stl_iterator.h contains predefined iterators, 
// such as front_insert_iterator and istream_iterator.

#pragma GCC system_header

namespace std
{

  struct input_iterator_tag {};
  struct output_iterator_tag {};
  struct forward_iterator_tag : public input_iterator_tag {};
  struct bidirectional_iterator_tag : public forward_iterator_tag {};
  struct random_access_iterator_tag : public bidirectional_iterator_tag {};

  // The base classes input_iterator, output_iterator, forward_iterator,
  // bidirectional_iterator, and random_access_iterator are not part of
  // the C++ standard.  (They have been replaced by struct iterator.)
  // They are included for backward compatibility with the HP STL.

  template<typename _Tp, typename _Distance>
    struct input_iterator {
      typedef input_iterator_tag iterator_category;
      typedef _Tp                value_type;
      typedef _Distance          difference_type;
      typedef _Tp*               pointer;
      typedef _Tp&               reference;
    };

  struct output_iterator {
    typedef output_iterator_tag iterator_category;
    typedef void                value_type;
    typedef void                difference_type;
    typedef void                pointer;
    typedef void                reference;
  };

  template<typename _Tp, typename _Distance>
    struct forward_iterator {
      typedef forward_iterator_tag iterator_category;
      typedef _Tp                  value_type;
      typedef _Distance            difference_type;
      typedef _Tp*                 pointer;
      typedef _Tp&                 reference;
    };

  template<typename _Tp, typename _Distance>
    struct bidirectional_iterator {
      typedef bidirectional_iterator_tag iterator_category;
      typedef _Tp                        value_type;
      typedef _Distance                  difference_type;
      typedef _Tp*                       pointer;
      typedef _Tp&                       reference;
    };

  template<typename _Tp, typename _Distance>
    struct random_access_iterator {
      typedef random_access_iterator_tag iterator_category;
      typedef _Tp                        value_type;
      typedef _Distance                  difference_type;
      typedef _Tp*                       pointer;
      typedef _Tp&                       reference;
    };

  template<typename _Category, typename _Tp, typename _Distance = ptrdiff_t,
	   typename _Pointer = _Tp*, typename _Reference = _Tp&>
    struct iterator {
      typedef _Category  iterator_category;
      typedef _Tp        value_type;
      typedef _Distance  difference_type;
      typedef _Pointer   pointer;
      typedef _Reference reference;
    };

  template<typename _Iterator>
    struct iterator_traits {
      typedef typename _Iterator::iterator_category iterator_category;
      typedef typename _Iterator::value_type        value_type;
      typedef typename _Iterator::difference_type   difference_type;
      typedef typename _Iterator::pointer           pointer;
      typedef typename _Iterator::reference         reference;
    };

  template<typename _Tp>
    struct iterator_traits<_Tp*> {
      typedef random_access_iterator_tag iterator_category;
      typedef _Tp                         value_type;
      typedef ptrdiff_t                   difference_type;
      typedef _Tp*                        pointer;
      typedef _Tp&                        reference;
    };

  template<typename _Tp>
    struct iterator_traits<const _Tp*> {
      typedef random_access_iterator_tag iterator_category;
      typedef _Tp                         value_type;
      typedef ptrdiff_t                   difference_type;
      typedef const _Tp*                  pointer;
      typedef const _Tp&                  reference;
    };

  // This function is not a part of the C++ standard but is syntactic
  // sugar for internal library use only.

  template<typename _Iter>
    inline typename iterator_traits<_Iter>::iterator_category
    __iterator_category(const _Iter&)
    { return typename iterator_traits<_Iter>::iterator_category(); }

} // namespace std

#endif /* __GLIBCPP_INTERNAL_ITERATOR_BASE_TYPES_H */


// Local Variables:
// mode:C++
// End:
