#pragma once

extern "C" {
#include <lua.h>
}

// Converts whatever the Python interpreter is currently unhappy about
// into a plain Lua error, so callers never have to touch a Python
// traceback unless they go looking for one.

namespace luna {

// Must only be called when PyErr_Occurred() is true. Fetches and clears
// the current Python exception, formats it as "ExceptionType: message",
// and raises it via luaL_error.
//
// luaL_error itself never returns (it does a longjmp back into the
// nearest lua_pcall / the interpreter's top-level error handler), so in
// practice neither does this, but it isn't marked [[noreturn]] because
// luaL_error's own C signature doesn't guarantee that to the compiler.
// Callers should still write a `return 0;` (or similar) immediately
// after calling this, both for clarity and in case that ever changes.
void errors_raise_from_python(lua_State* L);

} // namespace luna