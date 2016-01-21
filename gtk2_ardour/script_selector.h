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
 */

#include <gtkmm.h>
#include "ardour/luascripting.h"

#include "ardour_dialog.h"

class ScriptSelector : public ArdourDialog
{
public:
	ScriptSelector (std::string title, ARDOUR::LuaScriptInfo::ScriptType t);
	const std::string& path () const { return _path; }

private:
	void setup_list ();
	void script_combo_changed ();

	Gtk::Button* _add;
	Gtk::ComboBoxText _script_combo;

	std::string _path;
	Gtk::Label  _type;
	Gtk::Label  _author;
	Gtk::Label  _description;

	ARDOUR::LuaScriptList & _scripts;
};
