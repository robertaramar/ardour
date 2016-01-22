/*
    Copyright (C) 2016 Robin Gareus <robin@gareus.org>
    Copyright (C) 2006 Paul Davis

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

#ifndef __ardour_luaproc_h__
#define __ardour_luaproc_h__

#include "pbd/reallocpool.h"
#include "ardour/types.h"
#include "ardour/audio_buffer.h"
#include "ardour/processor.h"

#include "lua/luastate.h"
#include "lua/LuaBridge/LuaBridge.h"

namespace ARDOUR {
class LIBARDOUR_API LuaProc : public Processor {
public:
	LuaProc(Session& s, const std::string&);
	LuaProc(Session& s);
	virtual ~LuaProc();

	bool can_support_io_configuration (const ChanCount& in, ChanCount& out);
	bool configure_io (ChanCount in, ChanCount out);

	void run (BufferSet&, framepos_t, framepos_t, pframes_t nframes, bool);

	XMLNode& state (bool full);
	int set_state (const XMLNode&, int version);

private:
	PBD::ReallocPool _mempool;
	LuaState lua;
	luabridge::LuaRef * _lua_dsp;
	std::string _script;

	void init ();
	bool load_script ();
	void lua_print (std::string s);

	ChanCount _configured_in;
	ChanCount _configured_out;
};

} // namespace ARDOUR

#endif // __ardour_luaproc_h__
