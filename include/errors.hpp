#pragma once

extern "C" {
#include <lua.h>
}

namespace luna {

void errors_raise_from_python(lua_State* L);

} // namespace luna