/*
    Copyright (C) 2012 Paul Davis

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

#ifdef WAF_BUILD
#include "libpbd-config.h"
#endif

#include <boost/scoped_ptr.hpp>
#include <string>
#include <glibmm/spawn.h>

#include "pbd/epa.h"
#include "pbd/openuri.h"

#ifdef __APPLE__
	extern bool cocoa_open_url (const char*);
#endif

#ifdef PLATFORM_WINDOWS
	#include <windows.h>
	#include <shellapi.h>
#endif

bool
PBD::open_uri (const char* uri)
{
#ifdef PLATFORM_WINDOWS
	ShellExecute(NULL, "open", uri, NULL, NULL, SW_SHOWNORMAL);
	return true;
#elif __APPLE__
	return cocoa_open_url (uri);
#else
	EnvironmentalProtectionAgency* global_epa = EnvironmentalProtectionAgency::get_global_epa ();
	boost::scoped_ptr<EnvironmentalProtectionAgency> current_epa;

	/* revert all environment settings back to whatever they were when ardour started
	 */

	if (global_epa) {
		current_epa.reset (new EnvironmentalProtectionAgency(true)); /* will restore settings when we leave scope */
		global_epa->restore ();
	}

	std::string command = "xdg-open ";
	command += uri;
	command += " &";
	(void) system (command.c_str());

	return true;
#endif
}

bool
PBD::open_uri (const std::string& uri)
{
	return open_uri (uri.c_str());
}
