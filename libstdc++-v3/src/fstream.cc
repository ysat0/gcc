// File based streams -*- C++ -*-

// Copyright (C) 1997, 1998, 1999, 2000, 2001, 2002, 2003
// Free Software Foundation, Inc.
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

//
// ISO C++ 14882: 27.8  File-based streams
//

#include <fstream>

namespace std 
{
  template<> 
    basic_filebuf<char>::int_type 
    basic_filebuf<char>::_M_underflow(bool __bump)
    {
      int_type __ret = traits_type::eof();
      const bool __testin = _M_mode & ios_base::in;
      const bool __testout = _M_mode & ios_base::out;
      const bool __testsync = _M_buf_size <= 1;

      if (__testin)
	{
	  // Check for pback madness, and if so swich back to the
	  // normal buffers and jet outta here before expensive
	  // fileops happen...
	  if (_M_pback_init)
	    _M_destroy_pback();

	  if (_M_in_cur < _M_in_end)
	    {
	      __ret = traits_type::to_int_type(*_M_in_cur);
	      if (__bump)
		_M_move_in_cur(1);
	      return __ret;
	    }

	  // Sync internal and external buffers.
	  // NB: __testget -> __testput as _M_buf_unified here.
	  const bool __testget = _M_in_beg < _M_in_cur;
	  const bool __testinit = _M_is_indeterminate();
	  if (__testget)
	    {
	      if (__testout)
		_M_overflow();
	      else if (_M_in_cur != _M_filepos)
		_M_file.seekoff(_M_in_cur - _M_filepos,
				ios_base::cur, __testsync, ios_base::in);
	    }

	  if (__testinit || __testget)
	    {
	      streamsize __elen = 0;
	      streamsize __ilen = 0;

	      __elen = _M_file.xsgetn(reinterpret_cast<char*>(_M_in_beg), 
				      _M_buf_size, __testsync);
	      __ilen = __elen;

	      if (0 < __ilen)
		{
		  _M_set_determinate(__ilen);
		  if (__testout)
		    _M_out_cur = _M_in_cur;
		  __ret = traits_type::to_int_type(*_M_in_cur);
		  if (__bump)
		    _M_move_in_cur(1);
		  else if (__testsync)
		    {
		      // If we are synced with stdio, we have to unget the
		      // character we just read so that the file pointer
		      // doesn't move.
		      _M_file.sys_ungetc(traits_type::to_int_type(*_M_in_cur));
		      _M_set_indeterminate();
		    }
		}	   
	    }
	}
      _M_last_overflowed = false;	
      return __ret;
    }

  template<>
    basic_filebuf<char>::int_type
    basic_filebuf<char>::underflow() 
    { return _M_underflow(false); }

  template<>
    basic_filebuf<char>::int_type
    basic_filebuf<char>::uflow() 
    { return _M_underflow(true); }

#ifdef _GLIBCPP_USE_WCHAR_T
  template<> 
    basic_filebuf<wchar_t>::int_type 
    basic_filebuf<wchar_t>::_M_underflow(bool __bump)
    {
      int_type __ret = traits_type::eof();
      const bool __testin = _M_mode & ios_base::in;
      const bool __testout = _M_mode & ios_base::out;
      const bool __testsync = _M_buf_size <= 1;

      if (__testin)
	{
	  // Check for pback madness, and if so swich back to the
	  // normal buffers and jet outta here before expensive
	  // fileops happen...
	  if (_M_pback_init)
	    _M_destroy_pback();

	  if (_M_in_cur < _M_in_end)
	    {
	      __ret = traits_type::to_int_type(*_M_in_cur);
	      if (__bump)
		_M_move_in_cur(1);
	      return __ret;
	    }

	  // Sync internal and external buffers.
	  // NB: __testget -> __testput as _M_buf_unified here.
	  const bool __testget = _M_in_beg < _M_in_cur;
	  const bool __testinit = _M_is_indeterminate();
	  if (__testget)
	    {
	      if (__testout)
		_M_overflow();
	      else if (_M_in_cur != _M_filepos)
		_M_file.seekoff(_M_in_cur - _M_filepos,
				ios_base::cur, __testsync, ios_base::in);
	    }

	  if (__testinit || __testget)
	    {
	      streamsize __elen = 0;
	      streamsize __ilen = 0;
	      const locale __loc = this->getloc();
	      const __codecvt_type& __cvt = use_facet<__codecvt_type>(__loc);
	      if (__cvt.always_noconv())
		{
		  __elen = _M_file.xsgetn(reinterpret_cast<char*>(_M_in_beg), 
					  _M_buf_size, __testsync);
		  __ilen = __elen;
		}
	      else
		{
		  char* __buf = static_cast<char*>(__builtin_alloca(_M_buf_size));
		  __elen = _M_file.xsgetn(__buf, _M_buf_size, __testsync);
		  
		  const char* __eend;
		  char_type* __iend;
		  codecvt_base::result __r;
		  __r = __cvt.in(_M_state_cur, __buf, __buf + __elen, __eend, 
				 _M_in_beg, _M_in_beg + _M_buf_size, __iend);
		  if (__r == codecvt_base::ok)
		    __ilen = __iend - _M_in_beg;
		  else 
		    {
		      // Unwind.
		      __ilen = 0;
		      _M_file.seekoff(-__elen, ios_base::cur, __testsync, 
				      ios_base::in);
		    }
		}

	      if (0 < __ilen)
		{
		  _M_set_determinate(__ilen);
		  if (__testout)
		    _M_out_cur = _M_in_cur;
		  __ret = traits_type::to_int_type(*_M_in_cur);
		  if (__bump)
		    _M_move_in_cur(1);
		  else if (__testsync)
		    {
		      // If we are synced with stdio, we have to unget the
		      // character we just read so that the file pointer
		      // doesn't move.
		      _M_file.sys_ungetc(traits_type::to_int_type(*_M_in_cur));
		      _M_set_indeterminate();
		    }
		}	   
	    }
	}
      _M_last_overflowed = false;	
      return __ret;
    }

  template<>
    basic_filebuf<wchar_t>::int_type
    basic_filebuf<wchar_t>::underflow() 
    { return _M_underflow(false); }

  template<>
    basic_filebuf<wchar_t>::int_type
    basic_filebuf<wchar_t>::uflow() 
    { return _M_underflow(true); }
#endif
} // namespace std
