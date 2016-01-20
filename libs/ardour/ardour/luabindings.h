/*
    Copyright (C) 2016 Robin Gareus <robin@gareus.org>

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __ardour_luabindings_h__
#define __ardour_luabindings_h__

#include "lua/lua.h"
#include "ardour/libardour_visibility.h"

namespace ARDOUR { namespace LuaBindings {
	LIBARDOUR_API extern void stddef (lua_State* L);

	LIBARDOUR_API extern void rtsafe_common (lua_State* L);
	LIBARDOUR_API extern void rtsafe_session (lua_State* L);

	LIBARDOUR_API extern void libardour (lua_State* L);

} } // namespaces

#endif /* __ardour_luabindings_h__ */

