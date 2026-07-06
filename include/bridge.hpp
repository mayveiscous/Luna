#pragma once

extern "C" {
#include <lua.h>
}

// The small set of Lua-callable C functions that make up the public
// `python` module surface (currently just `import`; attribute access,
// calls, and assignment all live in py_object.cpp's metamethods once
// you're holding a wrapped object).

namespace luna {

// Lua C function: python.import(name: string) -> wrapped Python module.
// Equivalent to Python's `import name`.
int bridge_import(lua_State* L);

} // namespace luna