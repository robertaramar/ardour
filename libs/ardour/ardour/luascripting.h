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
#ifndef _ardour_luascripting_h_
#define _ardour_luascripting_h_
#include <vector>

#include "lua/luastate.h"
#include "lua/LuaBridge/LuaBridge.h"

#include "ardour/libardour_visibility.h"
#include <glibmm/threads.h>

namespace ARDOUR {

class LIBARDOUR_API LuaScriptInfo {
  public:

	enum ScriptType {
		Invalid,
		DSP,
		Session,
		EditorHook,
		EditorAction,
	};

	static std::string type2str (const ScriptType t);
	static ScriptType str2type (const std::string& str);

	LuaScriptInfo (ScriptType t, const std::string &n, const std::string &p)
	: type (t)
	, name (n)
	, path (p)
	, chan_in (-1)
	, chan_out (-1)
	, ctrl_in (0)
	, ctrl_out (0)
	{ }

	virtual ~LuaScriptInfo () { }

	ScriptType type;
	std::string name;
	std::string path;

	std::string author;
	std::string license;
	std::string category;
	std::string description;

	/* for DSP: channel count follows AU spec. see audio_unit.cc
	 * https://developer.apple.com/library/mac/documentation/MusicAudio/Conceptual/AudioUnitProgrammingGuide/TheAudioUnit/TheAudioUnit.html#//apple_ref/doc/uid/TP40003278-CH12-SW20
	 */
	int chan_in;
	int chan_out;
	uint32_t ctrl_in;
	uint32_t ctrl_out;
	// TODO control port key/value table
};

typedef boost::shared_ptr<LuaScriptInfo> LuaScriptInfoPtr;
typedef std::vector<LuaScriptInfoPtr> LuaScriptList;


class LIBARDOUR_API LuaScripting {

public:
	static LuaScripting& instance();

	~LuaScripting ();

	LuaScriptList &scripts (LuaScriptInfo::ScriptType);

	void refresh ();

private:
	static LuaScripting* _instance; // singleton
	LuaScripting ();

	void scan ();
	void check_scan ();
	bool scan_file (const std::string &);
	void lua_print (std::string s);

	LuaScriptList *_sl_dsp;
	LuaScriptList *_sl_session;
	LuaScriptList *_sl_hook;
	LuaScriptList *_sl_action;
	LuaScriptList  _empty_script_info;

	Glib::Threads::Mutex _lock;
};

} // namespace ARDOUR

#endif // _ardour_luascripting_h_
