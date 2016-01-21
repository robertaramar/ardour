#include <cstdio>
#include <iostream>
#include <string>
#include <list>
#include <vector>

#define LIBPBD_API
#include "../../libs/pbd/pbd/reallocpool.h"
#include "../../libs/pbd/reallocpool.cc"

#include <readline/readline.h>
#include <readline/history.h>

#include "lua/luastate.h"
#include "LuaBridge/LuaBridge.h"

static void my_lua_print (std::string s) {
	std::cout << s << "\n";
}


class A {
	public:
		A() { printf ("CTOR\n"); for (int i = 0; i < 256; ++i) {arr[i] = i; ar2[i] = i/256.0; ar3[i] = i;} }
		~A() { printf ("DTOR\n"); }

		void set_int (int a) { _int = a; }
		int  get_int () const { return _int; }

		int& get_ref () { return _int; }
		void get_arg (int &a) { a = _int; }
		void set_ref (int const &a) { _int = a; }

		float * get_arr () { return arr; }
		float * get_ar2 () { return ar2; }
		int * get_ar3 () { return ar3; }

		void set_list (std::list<std::string> sl) { _sl = sl; }
		std::list<std::string>& get_list () { return _sl; }

		enum EN {
			RV1 = 1, RV2, RV3
		};

		enum EN ret_enum () { return _en;}
		void set_enum (enum EN en) { _en = en; }

	private:
		std::list<std::string> _sl;
		int _int;
		enum EN _en;
		float arr[256];
		float ar2[256];
		int ar3[256];
};

int main (int argc, char **argv)
{
#if 0
	LuaState lua;
#else
	PBD::ReallocPool _mempool ("LuaProc", 1048576);
	LuaState lua (lua_newstate (&PBD::ReallocPool::lalloc, &_mempool));
#endif
	lua.Print.connect (&my_lua_print);
	lua_State* L = lua.getState();

	luabridge::getGlobalNamespace (L)
		.beginNamespace ("Test")
		.beginStdList <std::string> ("StringList")
		.endClass ()
		.endNamespace ();

	luabridge::getGlobalNamespace (L)
		.beginNamespace ("Test")
		.beginStdVector <std::string> ("StringVector")
		.endClass ()
		.endNamespace ();


	luabridge::getGlobalNamespace (L)
		.beginNamespace ("Test")
		.registerArray <float> ("FloatArray")
		.registerArray <int> ("IntArray")
		.beginClass <A> ("A")
		.addConstructor <void (*) ()> ()
		.addFunction ("set_int", &A::set_int)
		.addFunction ("get_int", &A::get_int)
		.addFunction ("get_ref", &A::get_ref) // XXX
		.addFunction ("get_arg", &A::get_arg) // XXX
		.addFunction ("set_ref", &A::set_ref)
		.addFunction ("get_list", &A::get_list)
		.addFunction ("set_list", &A::set_list)
		.addFunction ("ret_enum", &A::ret_enum)
		.addFunction ("set_enum", &A::set_enum)
		.addFunction ("get_arr", &A::get_arr)
		.addFunction ("get_ar2", &A::get_ar2)
		.addFunction ("get_ar3", &A::get_ar3)
		.endClass ()
		.endNamespace ();

	/////////////////////////////////////////////////////////////////////////////
	char *line;
	while ((line = readline ("> "))) {
		if (!strcmp (line, "quit")) {
			break;
		}
		if (strlen(line) == 0) {
			//lua.do_command("collectgarbage();");
			continue;
		}
		if (!lua.do_command (line)) {
			add_history(line); // OK
		} else {
			add_history(line); // :)
		}
	}
	printf("\n");
	return 0;
}
