#pragma once

extern "C" {
#include <lua.h>
}
#include <Python.h>

namespace luna {

PyObject* lua_to_python(lua_State* L, int idx);

void push_python_to_lua(lua_State* L, PyObject* obj, PyObject* bound_self);

} // namespace luna