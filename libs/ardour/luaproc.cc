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

#include <glibmm/base64.h>

#include "pbd/pthread_utils.h"

#include "ardour/audio_buffer.h"
#include "ardour/buffer_set.h"
#include "ardour/luabindings.h"
#include "ardour/luaproc.h"
#include "ardour/luascripting.h"
#include "ardour/midi_buffer.h"
#include "ardour/rc_configuration.h"
#include "ardour/session.h"

#include "lua/LuaBridge/LuaBridge.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;

LuaProc::LuaProc (Session& s, const std::string &script)
	: Processor (s, "LuaProc")
	, _mempool ("LuaProc", 1048576) // 1 MB is plenty. (64K would be enough)
	, lua (lua_newstate (&PBD::ReallocPool::lalloc, &_mempool))
	, _lua_dsp (0)
	, _script (script)
{
	init ();

	if (load_script ()) {
		throw failed_constructor ();
	}
}

LuaProc::LuaProc (Session& s)
	: Processor (s, "LuaProc")
	, _mempool ("LuaProc", 1048576) // 1 MB is plenty. (64K would be enough)
	, lua (lua_newstate (&PBD::ReallocPool::lalloc, &_mempool))
	, _lua_dsp (0)
{
	init ();
}

LuaProc::~LuaProc () {
	lua.do_command ("collectgarbage();");
	delete (_lua_dsp);
}

void
LuaProc::init ()
{
#ifndef NDEBUG
	lua.Print.connect (sigc::mem_fun (*this, &LuaProc::lua_print));
#endif
	// register session object
	lua_State* L = lua.getState ();
	LuaBindings::stddef (L);
	LuaBindings::rtsafe_common (L);
	LuaBindings::rtsafe_session (L);

	// add session to global lua namespace
	luabridge::push <Session *> (L, &_session);
	lua_setglobal (L, "Session");

	// sandbox - no not allow i/o (even during init)
	lua.do_command ("io = nil;");
	lua.do_command ("function ardour () end");
}

bool
LuaProc::load_script ()
{
	// TODO: define API for lua-plugin
	// - dsp method
	// - initialization function (optional)
	// - cleanup function (optional)
	// - query description, plugin-info
	// - query port-count (audio i/o, control ports)
	// - control-port API
	// - MIDI ?

	lua_State* L = lua.getState ();
	lua.do_command (
			"ardourluainfo = {}"
			"function ardour (entry)"
			"  ardourluainfo['type'] = assert(entry['type'])"
			"  ardourluainfo['name'] = assert(entry['name'])"
			" end"
			);

	try {
		if (lua.do_command (_script)) {
			return true;
		}
	} catch (...) { // luabridge::LuaException
		return true;
	}

	luabridge::LuaRef nfo = luabridge::getGlobal (L, "ardourluainfo");
	if (nfo.type() != LUA_TTABLE) {
		return true;
	}
	if (nfo["name"].type() != LUA_TSTRING || nfo["type"].type() != LUA_TSTRING) {
		return true;
	}
	if (LuaScriptInfo::DSP != LuaScriptInfo::str2type (nfo["type"].cast<std::string>())) {
		return true;
	}
	std::string name = nfo["name"].cast<std::string>();

	set_name (nfo["name"].cast<std::string>());

	// check if script has a DSP callback
	luabridge::LuaRef lua_dsp = luabridge::getGlobal (L, "dsp");
	if (lua_dsp.type() != LUA_TFUNCTION) {
		return true;
	}

	_lua_dsp = new luabridge::LuaRef (lua_dsp);

	// initialize the DSP if needed
	luabridge::LuaRef lua_dsp_init = luabridge::getGlobal (L, "dsp_init");
	if (lua_dsp_init.type() == LUA_TFUNCTION) {
		lua_dsp_init (_session.nominal_frame_rate ());
	}
	return false; // no error
}

bool
LuaProc::can_support_io_configuration (const ChanCount& in, ChanCount& out)
{
	out = in;
	return true;
}

bool
LuaProc::configure_io (ChanCount in, ChanCount out)
{
	if (out != in) { // always 1:1 currently
		return false;
	}

	return Processor::configure_io (in, out);
}


void
LuaProc::run (BufferSet& bufs, framepos_t /*start_frame*/, framepos_t /*end_frame*/, pframes_t nframes, bool)
{
	if (!_active && !_pending_active) {
		return;
	}
	if (!_lua_dsp) {
		return;
	}

	// This is needed for ARDOUR::Session requests :(
	if (! SessionEvent::has_per_thread_pool ()) {
		pthread_set_name (X_("LuaProc"));
		SessionEvent::create_per_thread_pool (X_("LuaProc"), 64);
		PBD::notify_event_loops_about_thread_creation (pthread_self(), X_("LuaProc"), 64);
	}

	try {
		(*_lua_dsp)(&bufs, nframes);
		//lua.collect_garbage(); // rt-safe, slight *regular* performance overhead
	} catch (...) { // luabridge::LuaException
		PBD::error << _("Lua Exception in DSP process.") << endmsg;
		_pending_active = _active = false;
		return;
	}

	_active = _pending_active;
}


XMLNode&
LuaProc::state (bool full_state)
{
	XMLNode& node (Processor::state (full_state));
	node.add_property("type", "luaproc");

	XMLNode* script_node = new XMLNode (X_("script"));
	script_node->add_property (X_("lua"), LUA_VERSION);
	script_node->add_content (Glib::Base64::encode (_script));
	node.add_child_nocopy (*script_node);
	return node;
}

int
LuaProc::set_state (const XMLNode& node, int version)
{
	Processor::set_state (node, version);
	XMLNode* child;
	if ((child = node.child (X_("script"))) != 0) {
		for (XMLNodeList::const_iterator n = child->children ().begin (); n != child->children ().end (); ++n) {
			if (!(*n)->is_content ()) { continue; }
			_script = Glib::Base64::decode ((*n)->content());
			if (load_script ()) {
				PBD::error << _("Failed to load Lua script from session state.") << endmsg;
#ifndef NDEBUG
				std::cerr << "Failed Lua Script: " << _script << std::endl;
#endif
				_script = "";
			}
		}
	}
	if (_script.empty()) {
		PBD::error << _("Session State for LuaProcessor did not include a Lua script.") << endmsg;
	}
	if (!_lua_dsp) {
		PBD::error << _("Invalid/incompatible Lua script found for LuaProcessor.") << endmsg;
	}
	return 0;
}

void
LuaProc::lua_print (std::string s) {
	std::cout <<"LuaProc: " << s << "\n";
}
