#pragma once

extern "C" {
#include <lua.h>
}

namespace luna {

int bridge_import(lua_State* L);

} // namespace luna