/* 
    Copyright (C) 2006 Paul Davis
    Author: Hans Fugal

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef __lib_pbd_memento_command_h__
#define __lib_pbd_memento_command_h__

#include <iostream>

#include "pbd/command.h"
#include "pbd/stacktrace.h"
#include "pbd/xml++.h"
#include "pbd/demangle.h"

#include <sigc++/slot.h>
#include <typeinfo>

/** A class that can return a Stateful object which is the subject of a MementoCommand.
 *
 *  The existence of this class means that the undo record can refer to objects which
 *  don't exist in the session file.  Currently this is just used for MIDI automation;
 *  when MIDI automation is edited, undo records are written for the AutomationList being
 *  changed.  However this AutomationList is a temporary structure, built by a MidiModel,
 *  which doesn't get written to the session file.  Hence we need to be able to go from
 *  a MidiSource and Parameter to an AutomationList.  This Binder mechanism allows this
 *  through MidiAutomationListBinder; the undo record stores the source and parameter,
 *  and these are bound to an AutomationList by the Binder.
 */
template <class obj_T>
class MementoCommandBinder : public PBD::Destructible
{
public:
	/** @return Stateful object to operate on */
	virtual obj_T* get () = 0;

	/** Add our own state to an XMLNode */
	virtual void add_state (XMLNode *) = 0;
};

/** A simple MementoCommandBinder which binds directly to an object */
template <class obj_T>
class SimpleMementoCommandBinder : public MementoCommandBinder<obj_T>
{
public:
	SimpleMementoCommandBinder (obj_T& o)
		: _object (o)
	{
		_object.Destroyed.connect_same_thread (_object_death_connection, boost::bind (&SimpleMementoCommandBinder::object_died, this));
	}

	obj_T* get () {
		return &_object;
	}

	void add_state (XMLNode* node) {
		node->add_property ("obj_id", _object.id().to_s());
	}
	
	void object_died () {
		this->drop_references ();
	}

private:
	obj_T& _object;
	PBD::ScopedConnection _object_death_connection;
};

/** This command class is initialized with before and after mementos 
 * (from Stateful::get_state()), so undo becomes restoring the before
 * memento, and redo is restoring the after memento.
 */
template <class obj_T>
class MementoCommand : public Command
{
public:
	MementoCommand (obj_T& a_object, XMLNode* a_before, XMLNode* a_after) 
		: _binder (new SimpleMementoCommandBinder<obj_T> (a_object)), before (a_before), after (a_after)
	{
		_binder->Destroyed.connect_same_thread (_binder_death_connection, boost::bind (&MementoCommand::binder_died, this));
	}

	MementoCommand (MementoCommandBinder<obj_T>* b, XMLNode* a_before, XMLNode* a_after) 
		: _binder (b), before (a_before), after (a_after)
	{
		_binder->Destroyed.connect_same_thread (_binder_death_connection, boost::bind (&MementoCommand::binder_died, this));
	}
	
	~MementoCommand () {
		drop_references ();
		delete before;
		delete after;
		delete _binder;
	}

	void binder_died () {
		delete this;
	}

	void operator() () {
		if (after) {
			_binder->get()->set_state(*after, Stateful::current_state_version); 
		}
	}

	void undo() { 
		if (before) {
			_binder->get()->set_state(*before, Stateful::current_state_version); 
		}
	}

	virtual XMLNode &get_state() {
		std::string name;
		if (before && after) {
			name = "MementoCommand";
		} else if (before) {
			name = "MementoUndoCommand";
		} else {
			name = "MementoRedoCommand";
		}

		XMLNode* node = new XMLNode(name);
		_binder->add_state (node);
		
		node->add_property("type_name", demangled_name (*_binder->get()));

		if (before) {
			node->add_child_copy(*before);
		}
		
		if (after) {
			node->add_child_copy(*after);
		}

		return *node;
	}

protected:
	MementoCommandBinder<obj_T>* _binder;
	XMLNode* before;
	XMLNode* after;
	PBD::ScopedConnection _binder_death_connection;
};

#endif // __lib_pbd_memento_h__
