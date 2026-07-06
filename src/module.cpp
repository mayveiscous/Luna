#include "bridge.hpp"
#include "py_object.hpp"
#include "python_runtime.hpp"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

extern "C" int luaopen_python(lua_State* L) {
    luna::runtime_init();
    luna::pyobject_register_metatable(L);

    static const luaL_Reg funcs[] = {
        {"import", luna::bridge_import},
        {nullptr, nullptr},
    };

    luaL_newlib(L, funcs);
    return 1;
}