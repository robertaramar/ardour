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

#include "gtkmm2ext/utils.h"

#include "script_selector.h"
#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace ARDOUR;

ScriptSelector::ScriptSelector (std::string title, LuaScriptInfo::ScriptType type)
	: ArdourDialog (title)
	, _scripts (LuaScripting::instance ().scripts (type))
	, _type ("", Gtk::ALIGN_START, Gtk::ALIGN_CENTER)
	, _author ("", Gtk::ALIGN_START, Gtk::ALIGN_CENTER)
	, _description ("", Gtk::ALIGN_START, Gtk::ALIGN_START)
{
	Gtk::Label* l;

	Table* t = manage (new Table (4, 3));
	t->set_spacings (6);

	int ty = 0;

	l = manage (new Label (_("<b>Type:</b>"), Gtk::ALIGN_END, Gtk::ALIGN_CENTER, false));
	l->set_use_markup ();
	t->attach (*l, 0, 1, ty, ty+1);
	t->attach (_type, 1, 2, ty, ty+1);
	++ty;

	l = manage (new Label (_("<b>Author:</b>"), Gtk::ALIGN_END, Gtk::ALIGN_CENTER, false));
	l->set_use_markup ();
	t->attach (*l, 0, 1, ty, ty+1);
	t->attach (_author, 1, 2, ty, ty+1);
	++ty;

	l = manage (new Label (_("<b>Description:</b>"), Gtk::ALIGN_END, Gtk::ALIGN_CENTER, false));
	l->set_use_markup ();
	t->attach (*l, 0, 1, ty, ty+1);
	t->attach (_description, 1, 2, ty, ty+1);
	++ty;

	_description.set_line_wrap();

	get_vbox()->set_spacing (6);
	get_vbox()->pack_start (_script_combo, false, false);
	get_vbox()->pack_start (*t, true, true);

	add_button (Stock::CANCEL, RESPONSE_CANCEL);
	_add = add_button (Stock::ADD, RESPONSE_ACCEPT);
	set_default_response (RESPONSE_ACCEPT);

	_add->set_sensitive (false);
	setup_list ();
	show_all ();

	_script_combo.signal_changed().connect (sigc::mem_fun (*this, &ScriptSelector::script_combo_changed));

}

void
ScriptSelector::setup_list ()
{
	vector<string> script_names;
	for (LuaScriptList::const_iterator s = _scripts.begin(); s != _scripts.end(); ++s) {
		script_names.push_back ((*s)->name);
	}

	Gtkmm2ext::set_popdown_strings (_script_combo, script_names);
	if (script_names.size() > 0) {
		_script_combo.set_active(0);
		script_combo_changed ();
	}
}

void
ScriptSelector::script_combo_changed ()
{
	int i = _script_combo.get_active_row_number();
	LuaScriptInfoPtr sp = _scripts[i];

	_path = sp->path;
	_type.set_text(LuaScriptInfo::type2str (sp->type));
	_author.set_text (sp->author);
	_description.set_text (sp->description);

	_add->set_sensitive (Glib::file_test(_path, Glib::FILE_TEST_EXISTS));
}
