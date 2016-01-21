/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */
#include <cstring>

#include "pbd/error.h"
#include "pbd/file_utils.h"
#include "pbd/compose.h"

#include "ardour/luascripting.h"
#include "ardour/search_paths.h"
#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

LuaScripting* LuaScripting::_instance = 0;

LuaScripting&
LuaScripting::instance ()
{
	if (!_instance) {
		_instance = new LuaScripting;
	}
	return *_instance;
}

LuaScripting::LuaScripting ()
	: _sl_dsp (0)
	, _sl_session (0)
	, _sl_hook (0)
	, _sl_action (0)
{
	;
}

LuaScripting::~LuaScripting ()
{
	if (getenv ("ARDOUR_RUNNING_UNDER_VALGRIND")) {
		// don't bother, just exit quickly.
		delete _sl_dsp;
		delete _sl_session;
		delete _sl_hook;
		delete _sl_action;
	}
}

void
LuaScripting::check_scan ()
{
	if (!_sl_dsp || !_sl_session || !_sl_hook || !_sl_action) {
		scan ();
	}
}

void
LuaScripting::scan ()
{
	Glib::Threads::Mutex::Lock lm (_lock, Glib::Threads::TRY_LOCK);

	if (!lm.locked()) {
		return;
	}

#define CLEAR_OR_NEW(LIST) \
	if (LIST) { LIST->clear (); } else { LIST = new LuaScriptList (); }

	CLEAR_OR_NEW (_sl_dsp)
	CLEAR_OR_NEW (_sl_session)
	CLEAR_OR_NEW (_sl_hook)
	CLEAR_OR_NEW (_sl_action)

#undef CLEAR_OR_NEW

	vector<string> luascripts;
	find_files_matching_pattern (luascripts, lua_search_path (), "*.lua");

	for (vector<string>::iterator i = luascripts.begin(); i != luascripts.end (); ++i) {
		if (scan_file (*i)) {
			PBD::info << string_compose (_("Script '%1' has no valid descriptor."), *i) << endmsg;
		}
	}
}

void
LuaScripting::lua_print (std::string s) {
	PBD::info << "Lua: " << s << "\n";
}

bool
LuaScripting::scan_file (const std::string &fn)
{
	LuaState lua;

	lua_State* L = lua.getState();
	lua.Print.connect (sigc::mem_fun (*this, &LuaScripting::lua_print));

	lua.do_command ("io = nil;");

	lua.do_command (
			"ardourluainfo = {}"
			"function ardour (entry)"
			"  ardourluainfo['type'] = assert(entry['type'])"
			"  ardourluainfo['name'] = assert(entry['name'])"
			"  ardourluainfo['author'] = entry['author'] or 'Unknown'"
			"  ardourluainfo['license'] = entry['license'] or ''"
			"  ardourluainfo['description'] = entry['description'] or ''"
			" end"
			);

	try {
		if (lua.do_file (fn)) {
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

	std::string name = nfo["name"].cast<std::string>();
	LuaScriptInfo::ScriptType type = LuaScriptInfo::str2type (nfo["type"].cast<std::string>());

	if (name.empty() || type == LuaScriptInfo::Invalid) {
		return true;
	}

	LuaScriptInfoPtr lsi (new LuaScriptInfo (type, name, fn));

	for (luabridge::Iterator i(nfo); !i.isNil (); ++i) {
		if (!i.key().isString() || !i.value().isString()) {
			return true;
		}
		std::string key = i.key().tostring();
		std::string val = i.value().tostring();

		if (key == "author") { lsi->author = val; } 
		if (key == "license") { lsi->license = val; } 
		if (key == "description") { lsi->description = val; } 
	}

	switch (type) {
		case LuaScriptInfo::DSP:
			_sl_dsp->push_back(lsi);
			break;
		case LuaScriptInfo::Session:
			_sl_session->push_back(lsi);
			break;
		case LuaScriptInfo::EditorHook:
			_sl_hook->push_back(lsi);
			break;
		case LuaScriptInfo::EditorAction:
			_sl_action->push_back(lsi);
			break;
		default:
			break;
	}

	return false; // no error
}

LuaScriptList &
LuaScripting::scripts (LuaScriptInfo::ScriptType type) {
	check_scan();

	switch (type) {
		case LuaScriptInfo::DSP:
			return *_sl_dsp;
			break;
		case LuaScriptInfo::Session:
			return *_sl_session;
			break;
		case LuaScriptInfo::EditorHook:
			return *_sl_hook;
			break;
		case LuaScriptInfo::EditorAction:
			return *_sl_action;
			break;
		default:
			return _empty_script_info;
			break;
	}
}


std::string
LuaScriptInfo::type2str (const ScriptType t) {
	switch (t) {
		case LuaScriptInfo::DSP: return "DSP";
		case LuaScriptInfo::Session: return "Session";
		case LuaScriptInfo::EditorHook: return "EditorHook";
		case LuaScriptInfo::EditorAction: return "EditorAction";
		default: return "Invalid";
	}
}

LuaScriptInfo::ScriptType
LuaScriptInfo::str2type (const std::string& str) {
	const char* type = str.c_str();
	if (!strcasecmp (type, "DSP")) {return LuaScriptInfo::DSP;}
	if (!strcasecmp (type, "Session")) {return LuaScriptInfo::Session;}
	if (!strcasecmp (type, "EditorHook")) {return LuaScriptInfo::EditorHook;}
	if (!strcasecmp (type, "EditorAction")) {return LuaScriptInfo::EditorAction;}
	return LuaScriptInfo::Invalid;
}
