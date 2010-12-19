/* GNU Objective C Runtime selector implementation - Private functions
   Copyright (C) 2010 Free Software Foundation, Inc.
   Contributed by Nicola Pero <nicola.pero@meta-innovation.com>

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation; either version 3, or (at your option) any later version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
details.

Under Section 7 of GPL version 3, you are granted additional
permissions described in the GCC Runtime Library Exception, version
3.1, as published by the Free Software Foundation.

You should have received a copy of the GNU General Public License and
a copy of the GCC Runtime Library Exception along with this program;
see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
<http://www.gnu.org/licenses/>.  */

#ifndef __objc_private_selector_INCLUDE_GNU
#define __objc_private_selector_INCLUDE_GNU

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Private runtime functions that may go away or be rewritten or
   replaced.  */

/* Return whether a selector is mapped or not ("mapped" meaning that
   it has been inserted into the selector table).  This is private as
   only the runtime should ever encounter or need to know about
   unmapped selectors.  */
BOOL sel_is_mapped (SEL aSel);

/* Return selector representing name without registering it if it
   doesn't exist.  Typically used internally by the runtime when it's
   looking up methods that may or may not exist (such as +initialize)
   in the most efficient way.  */
SEL
sel_get_any_uid (const char *name);

SEL
__sel_register_typed_name (const char *name, const char *types, 
			   struct objc_selector *orig, BOOL is_const);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* not __objc_private_selector_INCLUDE_GNU */
