// Vector implementation -*- C++ -*-

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
 * Copyright (c) 1996
 * Silicon Graphics Computer Systems, Inc.
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Silicon Graphics makes no
 * representations about the suitability of this  software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 */

/** @file stl_vector.h
 *  This is an internal header file, included by other library headers.
 *  You should not attempt to use it directly.
 */

#ifndef __GLIBCPP_INTERNAL_VECTOR_H
#define __GLIBCPP_INTERNAL_VECTOR_H

#include <bits/stl_iterator_base_funcs.h>
#include <bits/functexcept.h>
#include <bits/concept_check.h>

// Since this entire file is within namespace std, there's no reason to
// waste two spaces along the left column.  Thus the leading indentation is
// slightly violated from here on.
namespace std
{

/// @if maint Primary default version.  @endif
/**
 *  @if maint
 *  See bits/stl_deque.h's _Deque_alloc_base for an explanation.
 *  @endif
*/
template <class _Tp, class _Allocator, bool _IsStatic>
class _Vector_alloc_base
{
public:
  typedef typename _Alloc_traits<_Tp, _Allocator>::allocator_type
          allocator_type;

  allocator_type
  get_allocator() const { return _M_data_allocator; }

  _Vector_alloc_base(const allocator_type& __a)
    : _M_data_allocator(__a), _M_start(0), _M_finish(0), _M_end_of_storage(0)
  {}

protected:
  allocator_type _M_data_allocator;
  _Tp* _M_start;
  _Tp* _M_finish;
  _Tp* _M_end_of_storage;

  _Tp*
  _M_allocate(size_t __n) { return _M_data_allocator.allocate(__n); }

  void
  _M_deallocate(_Tp* __p, size_t __n)
    { if (__p) _M_data_allocator.deallocate(__p, __n); }
};

/// @if maint Specialization for instanceless allocators.  @endif
template <class _Tp, class _Allocator>
class _Vector_alloc_base<_Tp, _Allocator, true>
{
public:
  typedef typename _Alloc_traits<_Tp, _Allocator>::allocator_type
          allocator_type;

  allocator_type
  get_allocator() const { return allocator_type(); }

  _Vector_alloc_base(const allocator_type&)
    : _M_start(0), _M_finish(0), _M_end_of_storage(0)
  {}

protected:
  _Tp* _M_start;
  _Tp* _M_finish;
  _Tp* _M_end_of_storage;

  typedef typename _Alloc_traits<_Tp, _Allocator>::_Alloc_type _Alloc_type;

  _Tp*
  _M_allocate(size_t __n) { return _Alloc_type::allocate(__n); }

  void
  _M_deallocate(_Tp* __p, size_t __n) { _Alloc_type::deallocate(__p, __n);}
};


/**
 *  @if maint
 *  See bits/stl_deque.h's _Deque_base for an explanation.
 *  @endif
*/
template <class _Tp, class _Alloc>
struct _Vector_base
  : public _Vector_alloc_base<_Tp, _Alloc,
                              _Alloc_traits<_Tp, _Alloc>::_S_instanceless>
{
  typedef _Vector_alloc_base<_Tp, _Alloc,
                             _Alloc_traits<_Tp, _Alloc>::_S_instanceless>
          _Base;
  typedef typename _Base::allocator_type allocator_type;

  _Vector_base(const allocator_type& __a)
    : _Base(__a) {}
  _Vector_base(size_t __n, const allocator_type& __a)
    : _Base(__a)
  {
    _M_start = _M_allocate(__n);
    _M_finish = _M_start;
    _M_end_of_storage = _M_start + __n;
  }

  ~_Vector_base() { _M_deallocate(_M_start, _M_end_of_storage - _M_start); }
};


/**
 *  @brief  A standard container which offers fixed time access to individual
 *  elements in any order.
 *
 *  @ingroup Containers
 *  @ingroup Sequences
 *
 *  Meets the requirements of a <a href="tables.html#65">container</a>, a
 *  <a href="tables.html#66">reversible container</a>, and a
 *  <a href="tables.html#67">sequence</a>, including the
 *  <a href="tables.html#68">optional sequence requirements</a> with the
 *  %exception of @c push_front and @c pop_front.
 *
 *  In some terminology a %vector can be described as a dynamic C-style array,
 *  it offers fast and efficient access to individual elements in any order
 *  and saves the user from worrying about memory and size allocation.
 *  Subscripting ( @c [] ) access is also provided as with C-style arrays.
*/
template <class _Tp, class _Alloc = allocator<_Tp> >
class vector : protected _Vector_base<_Tp, _Alloc>
{
  // concept requirements
  __glibcpp_class_requires(_Tp, _SGIAssignableConcept)

  typedef _Vector_base<_Tp, _Alloc>                     _Base;
  typedef vector<_Tp, _Alloc>                           vector_type;

public:
  typedef _Tp 						value_type;
  typedef value_type* 					pointer;
  typedef const value_type* 				const_pointer;
  typedef __gnu_cxx::__normal_iterator<pointer, vector_type> 	iterator;
  typedef __gnu_cxx::__normal_iterator<const_pointer, vector_type>
                                                        const_iterator;
  typedef reverse_iterator<const_iterator>              const_reverse_iterator;
  typedef reverse_iterator<iterator>                    reverse_iterator;
  typedef value_type& 					reference;
  typedef const value_type& 				const_reference;
  typedef size_t 					size_type;
  typedef ptrdiff_t 					difference_type;
  typedef typename _Base::allocator_type                allocator_type;

protected:
  /** @if maint
   *  These two functions and three data members are all from the top-most
   *  base class, which varies depending on the type of %allocator.  They
   *  should be pretty self-explanatory, as %vector uses a simple contiguous 
   *  allocation scheme.
   *  @endif
  */
  using _Base::_M_allocate;
  using _Base::_M_deallocate;
  using _Base::_M_start;
  using _Base::_M_finish;
  using _Base::_M_end_of_storage;

protected:
  void _M_insert_aux(iterator __position, const _Tp& __x);
#ifdef _GLIBCPP_DEPRECATED
  void _M_insert_aux(iterator __position);
#endif

public:
  // [23.2.4.1] construct/copy/destroy
  // (assign() and get_allocator() are also listed in this section)
  /**
   *  @brief  Default constructor creates no elements.
  */
  explicit
  vector(const allocator_type& __a = allocator_type())
    : _Base(__a) {}

  /**
   *  @brief  Create a %vector with copies of an exemplar element.
   *  @param  n  The number of elements to initially create.
   *  @param  value  An element to copy.
   * 
   *  This constructor fills the %vector with @a n copies of @a value.
  */
  vector(size_type __n, const _Tp& __value,
         const allocator_type& __a = allocator_type())
    : _Base(__n, __a)
    { _M_finish = uninitialized_fill_n(_M_start, __n, __value); }

  /**
   *  @brief  Create a %vector with default elements.
   *  @param  n  The number of elements to initially create.
   * 
   *  This constructor fills the %vector with @a n copies of a
   *  default-constructed element.
  */
  explicit
  vector(size_type __n)
    : _Base(__n, allocator_type())
    { _M_finish = uninitialized_fill_n(_M_start, __n, _Tp()); }

  /**
   *  @brief  %Vector copy constructor.
   *  @param  x  A %vector of identical element and allocator types.
   * 
   *  The newly-created %vector uses a copy of the allocation object used
   *  by @a x.  All the elements of @a x are copied, but any extra memory in
   *  @a x (for fast expansion) will not be copied.
  */
  vector(const vector<_Tp, _Alloc>& __x)
    : _Base(__x.size(), __x.get_allocator())
    { _M_finish = uninitialized_copy(__x.begin(), __x.end(), _M_start); }

  /**
   *  @brief  Builds a %vector from a range.
   *  @param  first  An input iterator.
   *  @param  last  An input iterator.
   * 
   *  Creats a %vector consisting of copies of the elements from [first,last).
   *
   *  If the iterators are forward, bidirectional, or random-access, then
   *  this will call the elements' copy constructor N times (where N is
   *  distance(first,last)) and do no memory reallocation.  But if only
   *  input iterators are used, then this will do at most 2N calls to the
   *  copy constructor, and logN memory reallocations.
  */
  template <class _InputIterator>
    vector(_InputIterator __first, _InputIterator __last,
           const allocator_type& __a = allocator_type())
	: _Base(__a)
    {
      // Check whether it's an integral type.  If so, it's not an iterator.
      typedef typename _Is_integer<_InputIterator>::_Integral _Integral;
      _M_initialize_aux(__first, __last, _Integral());
    }

protected:
  template<class _Integer>
    void
    _M_initialize_aux(_Integer __n, _Integer __value, __true_type)
    {
      _M_start = _M_allocate(__n);
      _M_end_of_storage = _M_start + __n;
      _M_finish = uninitialized_fill_n(_M_start, __n, __value);
    }

  template<class _InputIterator>
    void
    _M_initialize_aux(_InputIterator __first,_InputIterator __last,__false_type)
    {
      typedef typename iterator_traits<_InputIterator>::iterator_category
                       _IterCategory;
      _M_range_initialize(__first, __last, _IterCategory());
    }

public:
  /**
   *  Creats a %vector consisting of copies of the elements from [first,last).
   *
   *  The dtor only erases the elements, and that if the elements
   *  themselves are pointers, the pointed-to memory is not touched in any
   *  way.  Managing the pointer is the user's responsibilty.
  */
  ~vector() { _Destroy(_M_start, _M_finish); }

  /**
   *  @brief  %Vector assignment operator.
   *  @param  x  A %vector of identical element and allocator types.
   * 
   *  All the elements of @a x are copied, but any extra memory in @a x (for
   *  fast expansion) will not be copied.  Unlike the copy constructor, the
   *  allocator object is not copied.
  */
  vector<_Tp, _Alloc>&
  operator=(const vector<_Tp, _Alloc>& __x);

  /**
   *  @brief  Assigns a given value to a %vector.
   *  @param  n  Number of elements to be assigned.
   *  @param  val  Value to be assigned.
   *
   *  This function fills a %vector with @a n copies of the given value.
   *  Note that the assignment completely changes the %vector and that the
   *  resulting %vector's size is the same as the number of elements assigned.
   *  Old data may be lost.
  */
  void
  assign(size_type __n, const _Tp& __val) { _M_fill_assign(__n, __val); }

protected:
  void
  _M_fill_assign(size_type __n, const _Tp& __val);

public:
  /**
   *  @brief  Assigns a range to a %vector.
   *  @param  first  An input iterator.
   *  @param  last   An input iterator.
   *
   *  This function fills a %vector with copies of the elements in the
   *  range [first,last).
   *
   *  Note that the assignment completely changes the %vector and that the
   *  resulting %vector's size is the same as the number of elements assigned.
   *  Old data may be lost.
  */
  template<class _InputIterator>
    void
    assign(_InputIterator __first, _InputIterator __last)
    {
      typedef typename _Is_integer<_InputIterator>::_Integral _Integral;
      _M_assign_dispatch(__first, __last, _Integral());
    }

protected:
  template<class _Integer>
    void
     _M_assign_dispatch(_Integer __n, _Integer __val, __true_type)
     { _M_fill_assign((size_type) __n, (_Tp) __val); }

  template<class _InputIter>
    void
    _M_assign_dispatch(_InputIter __first, _InputIter __last, __false_type)
    {
      typedef typename iterator_traits<_InputIter>::iterator_category
                       _IterCategory;
      _M_assign_aux(__first, __last, _IterCategory());
    }

  template <class _InputIterator>
    void 
    _M_assign_aux(_InputIterator __first, _InputIterator __last,
		  input_iterator_tag);

  template <class _ForwardIterator>
    void 
    _M_assign_aux(_ForwardIterator __first, _ForwardIterator __last,
		  forward_iterator_tag);

public:
  /// Get a copy of the memory allocation object.
  allocator_type
  get_allocator() const { return _Base::get_allocator(); }

  // iterators
  /**
   *  Returns a read/write iterator that points to the first element in the
   *  %vector.  Iteration is done in ordinary element order.
  */
  iterator
  begin() { return iterator (_M_start); }

  /**
   *  Returns a read-only (constant) iterator that points to the first element
   *  in the %vector.  Iteration is done in ordinary element order.
  */
  const_iterator
  begin() const { return const_iterator (_M_start); }

  /**
   *  Returns a read/write iterator that points one past the last element in
   *  the %vector.  Iteration is done in ordinary element order.
  */
  iterator
  end() { return iterator (_M_finish); }

  /**
   *  Returns a read-only (constant) iterator that points one past the last
   *  element in the %vector.  Iteration is done in ordinary element order.
  */
  const_iterator
  end() const { return const_iterator (_M_finish); }

  /**
   *  Returns a read/write reverse iterator that points to the last element in
   *  the %vector.  Iteration is done in reverse element order.
  */
  reverse_iterator
  rbegin() { return reverse_iterator(end()); }

  /**
   *  Returns a read-only (constant) reverse iterator that points to the last
   *  element in the %vector.  Iteration is done in reverse element order.
  */
  const_reverse_iterator
  rbegin() const { return const_reverse_iterator(end()); }

  /**
   *  Returns a read/write reverse iterator that points to one before the
   *  first element in the %vector.  Iteration is done in reverse element
   *  order.
  */
  reverse_iterator
  rend() { return reverse_iterator(begin()); }

  /**
   *  Returns a read-only (constant) reverse iterator that points to one
   *  before the first element in the %vector.  Iteration is done in reverse
   *  element order.
  */
  const_reverse_iterator
  rend() const { return const_reverse_iterator(begin()); }

  // [23.2.4.2] capacity
  /**  Returns the number of elements in the %vector.  */
  size_type
  size() const { return size_type(end() - begin()); }

  /**  Returns the size() of the largest possible %vector.  */
  size_type
  max_size() const { return size_type(-1) / sizeof(_Tp); }

  /**
   *  @brief  Resizes the %vector to the specified number of elements.
   *  @param  new_size  Number of elements the %vector should contain.
   *  @param  x  Data with which new elements should be populated.
   *
   *  This function will %resize the %vector to the specified number of
   *  elements.  If the number is smaller than the %vector's current size the
   *  %vector is truncated, otherwise the %vector is extended and new elements
   *  are populated with given data.
  */
  void
  resize(size_type __new_size, const _Tp& __x)
  {
    if (__new_size < size())
      erase(begin() + __new_size, end());
    else
      insert(end(), __new_size - size(), __x);
  }

  /**
   *  @brief  Resizes the %vector to the specified number of elements.
   *  @param  new_size  Number of elements the %vector should contain.
   *
   *  This function will resize the %vector to the specified number of
   *  elements.  If the number is smaller than the %vector's current size the
   *  %vector is truncated, otherwise the %vector is extended and new elements
   *  are default-constructed.
  */
  void
  resize(size_type __new_size) { resize(__new_size, _Tp()); }

  /**
   *  Returns the total number of elements that the %vector can hold before
   *  needing to allocate more memory.
  */
  size_type
  capacity() const
    { return size_type(const_iterator(_M_end_of_storage) - begin()); }

  /**
   *  Returns true if the %vector is empty.  (Thus begin() would equal end().)
  */
  bool
  empty() const { return begin() == end(); }

  /**
   *  @brief  Attempt to preallocate enough memory for specified number of
   *          elements.
   *  @param  n  Number of elements required.
   *  @throw  std::length_error  If @a n exceeds @c max_size().
   *
   *  This function attempts to reserve enough memory for the %vector to hold
   *  the specified number of elements.  If the number requested is more than
   *  max_size(), length_error is thrown.
   *
   *  The advantage of this function is that if optimal code is a necessity
   *  and the user can determine the number of elements that will be required,
   *  the user can reserve the memory in %advance, and thus prevent a possible
   *  reallocation of memory and copying of %vector data.
  */
  void
  reserve(size_type __n)   // FIXME should be out of class
  {
    if (capacity() < __n) {
      const size_type __old_size = size();
      pointer __tmp = _M_allocate_and_copy(__n, _M_start, _M_finish);
      _Destroy(_M_start, _M_finish);
      _M_deallocate(_M_start, _M_end_of_storage - _M_start);
      _M_start = __tmp;
      _M_finish = __tmp + __old_size;
      _M_end_of_storage = _M_start + __n;
    }
  }

  // element access
  /**
   *  @brief  Subscript access to the data contained in the %vector.
   *  @param  n  The index of the element for which data should be accessed.
   *  @return  Read/write reference to data.
   *
   *  This operator allows for easy, array-style, data access.
   *  Note that data access with this operator is unchecked and out_of_range
   *  lookups are not defined. (For checked lookups see at().)
  */
  reference
  operator[](size_type __n) { return *(begin() + __n); }

  /**
   *  @brief  Subscript access to the data contained in the %vector.
   *  @param  n  The index of the element for which data should be accessed.
   *  @return  Read-only (constant) reference to data.
   *
   *  This operator allows for easy, array-style, data access.
   *  Note that data access with this operator is unchecked and out_of_range
   *  lookups are not defined. (For checked lookups see at().)
  */
  const_reference
  operator[](size_type __n) const { return *(begin() + __n); }

protected:
  /// @if maint Safety check used only from at().  @endif
  void
  _M_range_check(size_type __n) const
  {
    if (__n >= this->size())
      __throw_out_of_range("vector [] access out of range");
  }

public:
  /**
   *  @brief  Provides access to the data contained in the %vector.
   *  @param  n  The index of the element for which data should be accessed.
   *  @return  Read/write reference to data.
   *  @throw  std::out_of_range  If @a n is an invalid index.
   *
   *  This function provides for safer data access.  The parameter is first
   *  checked that it is in the range of the vector.  The function throws
   *  out_of_range if the check fails.
  */
  reference
  at(size_type __n) { _M_range_check(__n); return (*this)[__n]; }

  /**
   *  @brief  Provides access to the data contained in the %vector.
   *  @param  n  The index of the element for which data should be accessed.
   *  @return  Read-only (constant) reference to data.
   *  @throw  std::out_of_range  If @a n is an invalid index.
   *
   *  This function provides for safer data access.  The parameter is first
   *  checked that it is in the range of the vector.  The function throws
   *  out_of_range if the check fails.
  */
  const_reference
  at(size_type __n) const { _M_range_check(__n); return (*this)[__n]; }

  /**
   *  Returns a read/write reference to the data at the first element of the
   *  %vector.
  */
  reference
  front() { return *begin(); }

  /**
   *  Returns a read-only (constant) reference to the data at the first
   *  element of the %vector.
  */
  const_reference
  front() const { return *begin(); }

  /**
   *  Returns a read/write reference to the data at the last element of the
   *  %vector.
  */
  reference
  back() { return *(end() - 1); }

  /**
   *  Returns a read-only (constant) reference to the data at the last
   *  element of the %vector.
  */
  const_reference
  back() const { return *(end() - 1); }

  // [23.2.4.3] modifiers
  /**
   *  @brief  Add data to the end of the %vector.
   *  @param  x  Data to be added.
   *
   *  This is a typical stack operation.  The function creates an element at
   *  the end of the %vector and assigns the given data to it.
   *  Due to the nature of a %vector this operation can be done in constant
   *  time if the %vector has preallocated space available.
  */
  void
  push_back(const _Tp& __x)
  {
    if (_M_finish != _M_end_of_storage) {
      _Construct(_M_finish, __x);
      ++_M_finish;
    }
    else
      _M_insert_aux(end(), __x);
  }

  /**
   *  @brief  Removes last element.
   *
   *  This is a typical stack operation. It shrinks the %vector by one.
   *
   *  Note that no data is returned, and if the last element's data is
   *  needed, it should be retrieved before pop_back() is called.
  */
  void
  pop_back()
  {
    --_M_finish;
    _Destroy(_M_finish);
  }

  /**
   *  @brief  Inserts given value into %vector before specified iterator.
   *  @param  position  An iterator into the %vector.
   *  @param  x  Data to be inserted.
   *  @return  An iterator that points to the inserted data.
   *
   *  This function will insert a copy of the given value before the specified
   *  location.
   *  Note that this kind of operation could be expensive for a %vector and if
   *  it is frequently used the user should consider using std::list.
  */
  iterator
  insert(iterator __position, const _Tp& __x)
  {
    size_type __n = __position - begin();
    if (_M_finish != _M_end_of_storage && __position == end()) {
      _Construct(_M_finish, __x);
      ++_M_finish;
    }
    else
      _M_insert_aux(iterator(__position), __x);
    return begin() + __n;
  }

#ifdef _GLIBCPP_DEPRECATED
  /**
   *  @brief  Inserts an element into the %vector.
   *  @param  position  An iterator into the %vector.
   *  @return  An iterator that points to the inserted element.
   *
   *  This function will insert a default-constructed element before the
   *  specified location.  You should consider using insert(position,Tp())
   *  instead.
   *  Note that this kind of operation could be expensive for a vector and if
   *  it is frequently used the user should consider using std::list.
   *
   *  @note This was deprecated in 3.2 and will be removed in 3.3.  You must
   *        define @c _GLIBCPP_DEPRECATED to make this visible in 3.2; see
   *        c++config.h.
  */
  iterator
  insert(iterator __position)
  {
    size_type __n = __position - begin();
    if (_M_finish != _M_end_of_storage && __position == end()) {
      _Construct(_M_finish);
      ++_M_finish;
    }
    else
      _M_insert_aux(iterator(__position));
    return begin() + __n;
  }
#endif

  /**
   *  @brief  Inserts a number of copies of given data into the %vector.
   *  @param  position  An iterator into the %vector.
   *  @param  n  Number of elements to be inserted.
   *  @param  x  Data to be inserted.
   *
   *  This function will insert a specified number of copies of the given data
   *  before the location specified by @a position.
   *
   *  Note that this kind of operation could be expensive for a %vector and if
   *  it is frequently used the user should consider using std::list.
  */
  void
  insert (iterator __pos, size_type __n, const _Tp& __x)
    { _M_fill_insert(__pos, __n, __x); }

protected:
  void
  _M_fill_insert (iterator __pos, size_type __n, const _Tp& __x);

public:
  /**
   *  @brief  Inserts a range into the %vector.
   *  @param  pos  An iterator into the %vector.
   *  @param  first  An input iterator.
   *  @param  last   An input iterator.
   *
   *  This function will insert copies of the data in the range [first,last)
   *  into the %vector before the location specified by @a pos.
   *
   *  Note that this kind of operation could be expensive for a %vector and if
   *  it is frequently used the user should consider using std::list.
  */
  template<class _InputIterator>
    void
    insert(iterator __pos, _InputIterator __first, _InputIterator __last)
      {
        // Check whether it's an integral type.  If so, it's not an iterator.
        typedef typename _Is_integer<_InputIterator>::_Integral _Integral;
        _M_insert_dispatch(__pos, __first, __last, _Integral());
      }

protected:
  template<class _Integer>
    void
    _M_insert_dispatch(iterator __pos, _Integer __n, _Integer __val,
                       __true_type)
    {
      _M_fill_insert(__pos, static_cast<size_type>(__n),
                            static_cast<_Tp>(__val));
    }

  template<class _InputIterator>
    void
    _M_insert_dispatch(iterator __pos, _InputIterator __first,
                       _InputIterator __last, __false_type)
    {
      typedef typename iterator_traits<_InputIterator>::iterator_category
                       _IterCategory;
      _M_range_insert(__pos, __first, __last, _IterCategory());
    }

public:
  /**
   *  @brief  Remove element at given position.
   *  @param  position  Iterator pointing to element to be erased.
   *  @return  An iterator pointing to the next element (or end()).
   *
   *  This function will erase the element at the given position and thus
   *  shorten the %vector by one.
   *
   *  Note This operation could be expensive and if it is frequently used the
   *  user should consider using std::list.  The user is also cautioned that
   *  this function only erases the element, and that if the element is itself
   *  a pointer, the pointed-to memory is not touched in any way.  Managing
   *  the pointer is the user's responsibilty.
  */
  iterator
  erase(iterator __position)
  {
    if (__position + 1 != end())
      copy(__position + 1, end(), __position);
    --_M_finish;
    _Destroy(_M_finish);
    return __position;
  }

  /**
   *  @brief  Remove a range of elements.
   *  @param  first  Iterator pointing to the first element to be erased.
   *  @param  last  Iterator pointing to one past the last element to be erased.
   *  @return  An iterator pointing to the element pointed to by @a last
   *           prior to erasing (or end()).
   *
   *  This function will erase the elements in the range [first,last) and
   *  shorten the %vector accordingly.
   *
   *  Note This operation could be expensive and if it is frequently used the
   *  user should consider using std::list.  The user is also cautioned that
   *  this function only erases the elements, and that if the elements
   *  themselves are pointers, the pointed-to memory is not touched in any
   *  way.  Managing the pointer is the user's responsibilty.
  */
  iterator
  erase(iterator __first, iterator __last)
  {
    iterator __i(copy(__last, end(), __first));
    _Destroy(__i, end());
    _M_finish = _M_finish - (__last - __first);
    return __first;
  }

  /**
   *  @brief  Swaps data with another %vector.
   *  @param  x  A %vector of the same element and allocator types.
   *
   *  This exchanges the elements between two vectors in constant time.
   *  (Three pointers, so it should be quite fast.)
   *  Note that the global std::swap() function is specialized such that
   *  std::swap(v1,v2) will feed to this function.
  */
  void
  swap(vector<_Tp, _Alloc>& __x)
  {
    std::swap(_M_start, __x._M_start);
    std::swap(_M_finish, __x._M_finish);
    std::swap(_M_end_of_storage, __x._M_end_of_storage);
  }

  /**
   *  Erases all the elements.  Note that this function only erases the
   *  elements, and that if the elements themselves are pointers, the
   *  pointed-to memory is not touched in any way.  Managing the pointer is
   *  the user's responsibilty.
  */
  void
  clear() { erase(begin(), end()); }

protected:
  template <class _ForwardIterator>
  pointer
    _M_allocate_and_copy(size_type __n, _ForwardIterator __first,
                         _ForwardIterator __last)
  {
    pointer __result = _M_allocate(__n);
    try
      {
        uninitialized_copy(__first, __last, __result);
        return __result;
      }
    catch(...)
      {
	_M_deallocate(__result, __n);
	__throw_exception_again;
      }
  }

  template <class _InputIterator>
  void
    _M_range_initialize(_InputIterator __first,
                        _InputIterator __last, input_iterator_tag)
  {
    for ( ; __first != __last; ++__first)
      push_back(*__first);
  }

  // This function is only called by the constructor.
  template <class _ForwardIterator>
  void _M_range_initialize(_ForwardIterator __first,
                           _ForwardIterator __last, forward_iterator_tag)
  {
    size_type __n = distance(__first, __last);
    _M_start = _M_allocate(__n);
    _M_end_of_storage = _M_start + __n;
    _M_finish = uninitialized_copy(__first, __last, _M_start);
  }

  template <class _InputIterator>
  void _M_range_insert(iterator __pos,
                       _InputIterator __first, _InputIterator __last,
                       input_iterator_tag);

  template <class _ForwardIterator>
  void _M_range_insert(iterator __pos,
                       _ForwardIterator __first, _ForwardIterator __last,
                       forward_iterator_tag);
};


/**
 *  @brief  Vector equality comparison.
 *  @param  x  A %vector.
 *  @param  y  A %vector of the same type as @a x.
 *  @return  True iff the size and elements of the vectors are equal.
 *
 *  This is an equivalence relation.  It is linear in the size of the
 *  vectors.  Vectors are considered equivalent if their sizes are equal,
 *  and if corresponding elements compare equal.
*/
template <class _Tp, class _Alloc>
inline bool
operator==(const vector<_Tp, _Alloc>& __x, const vector<_Tp, _Alloc>& __y)
{
  return __x.size() == __y.size() &&
         equal(__x.begin(), __x.end(), __y.begin());
}

/**
 *  @brief  Vector ordering relation.
 *  @param  x  A %vector.
 *  @param  y  A %vector of the same type as @a x.
 *  @return  True iff @a x is lexographically less than @a y.
 *
 *  This is a total ordering relation.  It is linear in the size of the
 *  vectors.  The elements must be comparable with @c <.
 *
 *  See std::lexographical_compare() for how the determination is made.
*/
template <class _Tp, class _Alloc>
inline bool
operator<(const vector<_Tp, _Alloc>& __x, const vector<_Tp, _Alloc>& __y)
{
  return lexicographical_compare(__x.begin(), __x.end(),
                                 __y.begin(), __y.end());
}

/// See std::vector::swap().
template <class _Tp, class _Alloc>
inline void swap(vector<_Tp, _Alloc>& __x, vector<_Tp, _Alloc>& __y)
{
  __x.swap(__y);
}

/// Based on operator==
template <class _Tp, class _Alloc>
inline bool
operator!=(const vector<_Tp, _Alloc>& __x, const vector<_Tp, _Alloc>& __y) {
  return !(__x == __y);
}

/// Based on operator<
template <class _Tp, class _Alloc>
inline bool
operator>(const vector<_Tp, _Alloc>& __x, const vector<_Tp, _Alloc>& __y) {
  return __y < __x;
}

/// Based on operator<
template <class _Tp, class _Alloc>
inline bool
operator<=(const vector<_Tp, _Alloc>& __x, const vector<_Tp, _Alloc>& __y) {
  return !(__y < __x);
}

/// Based on operator<
template <class _Tp, class _Alloc>
inline bool
operator>=(const vector<_Tp, _Alloc>& __x, const vector<_Tp, _Alloc>& __y) {
  return !(__x < __y);
}

// XXX begin tcc me
template <class _Tp, class _Alloc>
vector<_Tp,_Alloc>&
vector<_Tp,_Alloc>::operator=(const vector<_Tp, _Alloc>& __x)
{
  if (&__x != this) {
    const size_type __xlen = __x.size();
    if (__xlen > capacity()) {
      pointer __tmp = _M_allocate_and_copy(__xlen, __x.begin(), __x.end());
      _Destroy(_M_start, _M_finish);
      _M_deallocate(_M_start, _M_end_of_storage - _M_start);
      _M_start = __tmp;
      _M_end_of_storage = _M_start + __xlen;
    }
    else if (size() >= __xlen) {
      iterator __i(copy(__x.begin(), __x.end(), begin()));
      _Destroy(__i, end());
    }
    else {
      copy(__x.begin(), __x.begin() + size(), _M_start);
      uninitialized_copy(__x.begin() + size(), __x.end(), _M_finish);
    }
    _M_finish = _M_start + __xlen;
  }
  return *this;
}

template <class _Tp, class _Alloc>
void vector<_Tp, _Alloc>::_M_fill_assign(size_t __n, const value_type& __val)
{
  if (__n > capacity()) {
    vector<_Tp, _Alloc> __tmp(__n, __val, get_allocator());
    __tmp.swap(*this);
  }
  else if (__n > size()) {
    fill(begin(), end(), __val);
    _M_finish = uninitialized_fill_n(_M_finish, __n - size(), __val);
  }
  else
    erase(fill_n(begin(), __n, __val), end());
}

template <class _Tp, class _Alloc> template <class _InputIter>
void vector<_Tp, _Alloc>::_M_assign_aux(_InputIter __first, _InputIter __last,
                                        input_iterator_tag) {
  iterator __cur(begin());
  for ( ; __first != __last && __cur != end(); ++__cur, ++__first)
    *__cur = *__first;
  if (__first == __last)
    erase(__cur, end());
  else
    insert(end(), __first, __last);
}

template <class _Tp, class _Alloc> template <class _ForwardIter>
void
vector<_Tp, _Alloc>::_M_assign_aux(_ForwardIter __first, _ForwardIter __last,
                                   forward_iterator_tag) {
  size_type __len = distance(__first, __last);

  if (__len > capacity()) {
    pointer __tmp(_M_allocate_and_copy(__len, __first, __last));
    _Destroy(_M_start, _M_finish);
    _M_deallocate(_M_start, _M_end_of_storage - _M_start);
    _M_start = __tmp;
    _M_end_of_storage = _M_finish = _M_start + __len;
  }
  else if (size() >= __len) {
    iterator __new_finish(copy(__first, __last, _M_start));
    _Destroy(__new_finish, end());
    _M_finish = __new_finish.base();
  }
  else {
    _ForwardIter __mid = __first;
    advance(__mid, size());
    copy(__first, __mid, _M_start);
    _M_finish = uninitialized_copy(__mid, __last, _M_finish);
  }
}

template <class _Tp, class _Alloc>
void
vector<_Tp, _Alloc>::_M_insert_aux(iterator __position, const _Tp& __x)
{
  if (_M_finish != _M_end_of_storage) {
    _Construct(_M_finish, *(_M_finish - 1));
    ++_M_finish;
    _Tp __x_copy = __x;
    copy_backward(__position, iterator(_M_finish - 2), iterator(_M_finish- 1));
    *__position = __x_copy;
  }
  else {
    const size_type __old_size = size();
    const size_type __len = __old_size != 0 ? 2 * __old_size : 1;
    iterator __new_start(_M_allocate(__len));
    iterator __new_finish(__new_start);
    try {
      __new_finish = uninitialized_copy(iterator(_M_start), __position,
                                        __new_start);
      _Construct(__new_finish.base(), __x);
      ++__new_finish;
      __new_finish = uninitialized_copy(__position, iterator(_M_finish),
                                        __new_finish);
    }
    catch(...)
      {
	_Destroy(__new_start,__new_finish);
	_M_deallocate(__new_start.base(),__len);
	__throw_exception_again;
      }
    _Destroy(begin(), end());
    _M_deallocate(_M_start, _M_end_of_storage - _M_start);
    _M_start = __new_start.base();
    _M_finish = __new_finish.base();
    _M_end_of_storage = __new_start.base() + __len;
  }
}

#ifdef _GLIBCPP_DEPRECATED
template <class _Tp, class _Alloc>
void
vector<_Tp, _Alloc>::_M_insert_aux(iterator __position)
{
  if (_M_finish != _M_end_of_storage) {
    _Construct(_M_finish, *(_M_finish - 1));
    ++_M_finish;
    copy_backward(__position, iterator(_M_finish - 2),
		  iterator(_M_finish - 1));
    *__position = _Tp();
  }
  else {
    const size_type __old_size = size();
    const size_type __len = __old_size != 0 ? 2 * __old_size : 1;
    pointer __new_start = _M_allocate(__len);
    pointer __new_finish = __new_start;
    try {
      __new_finish = uninitialized_copy(iterator(_M_start), __position,
					__new_start);
      _Construct(__new_finish);
      ++__new_finish;
      __new_finish = uninitialized_copy(__position, iterator(_M_finish),
					__new_finish);
    }
    catch(...)
      {
	_Destroy(__new_start,__new_finish);
	_M_deallocate(__new_start,__len);
	__throw_exception_again;
      }
    _Destroy(begin(), end());
    _M_deallocate(_M_start, _M_end_of_storage - _M_start);
    _M_start = __new_start;
    _M_finish = __new_finish;
    _M_end_of_storage = __new_start + __len;
  }
}
#endif

template <class _Tp, class _Alloc>
void vector<_Tp, _Alloc>::_M_fill_insert(iterator __position, size_type __n,
                                         const _Tp& __x)
{
  if (__n != 0) {
    if (size_type(_M_end_of_storage - _M_finish) >= __n) {
      _Tp __x_copy = __x;
      const size_type __elems_after = end() - __position;
      iterator __old_finish(_M_finish);
      if (__elems_after > __n) {
        uninitialized_copy(_M_finish - __n, _M_finish, _M_finish);
        _M_finish += __n;
        copy_backward(__position, __old_finish - __n, __old_finish);
        fill(__position, __position + __n, __x_copy);
      }
      else {
        uninitialized_fill_n(_M_finish, __n - __elems_after, __x_copy);
        _M_finish += __n - __elems_after;
        uninitialized_copy(__position, __old_finish, _M_finish);
        _M_finish += __elems_after;
        fill(__position, __old_finish, __x_copy);
      }
    }
    else {
      const size_type __old_size = size();
      const size_type __len = __old_size + max(__old_size, __n);
      iterator __new_start(_M_allocate(__len));
      iterator __new_finish(__new_start);
      try {
        __new_finish = uninitialized_copy(begin(), __position, __new_start);
        __new_finish = uninitialized_fill_n(__new_finish, __n, __x);
        __new_finish
          = uninitialized_copy(__position, end(), __new_finish);
      }
      catch(...)
	{
	  _Destroy(__new_start,__new_finish);
	  _M_deallocate(__new_start.base(),__len);
	  __throw_exception_again;
	}
      _Destroy(_M_start, _M_finish);
      _M_deallocate(_M_start, _M_end_of_storage - _M_start);
      _M_start = __new_start.base();
      _M_finish = __new_finish.base();
      _M_end_of_storage = __new_start.base() + __len;
    }
  }
}

template <class _Tp, class _Alloc> template <class _InputIterator>
void
vector<_Tp, _Alloc>::_M_range_insert(iterator __pos,
                                     _InputIterator __first,
                                     _InputIterator __last,
                                     input_iterator_tag)
{
  for ( ; __first != __last; ++__first) {
    __pos = insert(__pos, *__first);
    ++__pos;
  }
}

template <class _Tp, class _Alloc> template <class _ForwardIterator>
void
vector<_Tp, _Alloc>::_M_range_insert(iterator __position,
                                     _ForwardIterator __first,
                                     _ForwardIterator __last,
                                     forward_iterator_tag)
{
  if (__first != __last) {
    size_type __n = distance(__first, __last);
    if (size_type(_M_end_of_storage - _M_finish) >= __n) {
      const size_type __elems_after = end() - __position;
      iterator __old_finish(_M_finish);
      if (__elems_after > __n) {
        uninitialized_copy(_M_finish - __n, _M_finish, _M_finish);
        _M_finish += __n;
        copy_backward(__position, __old_finish - __n, __old_finish);
        copy(__first, __last, __position);
      }
      else {
        _ForwardIterator __mid = __first;
        advance(__mid, __elems_after);
        uninitialized_copy(__mid, __last, _M_finish);
        _M_finish += __n - __elems_after;
        uninitialized_copy(__position, __old_finish, _M_finish);
        _M_finish += __elems_after;
        copy(__first, __mid, __position);
      }
    }
    else {
      const size_type __old_size = size();
      const size_type __len = __old_size + max(__old_size, __n);
      iterator __new_start(_M_allocate(__len));
      iterator __new_finish(__new_start);
      try {
        __new_finish = uninitialized_copy(iterator(_M_start),
					  __position, __new_start);
        __new_finish = uninitialized_copy(__first, __last, __new_finish);
        __new_finish
          = uninitialized_copy(__position, iterator(_M_finish), __new_finish);
      }
      catch(...)
	{
	  _Destroy(__new_start,__new_finish);
	  _M_deallocate(__new_start.base(), __len);
	  __throw_exception_again;
	}
      _Destroy(_M_start, _M_finish);
      _M_deallocate(_M_start, _M_end_of_storage - _M_start);
      _M_start = __new_start.base();
      _M_finish = __new_finish.base();
      _M_end_of_storage = __new_start.base() + __len;
    }
  }
}

} // namespace std

#endif /* __GLIBCPP_INTERNAL_VECTOR_H */

