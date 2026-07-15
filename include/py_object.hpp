#pragma once

extern "C" {
#include <lua.h>
}
#include <Python.h>


namespace luna {

struct PyObjectHandle {
    PyObject* obj;
    PyObject* bound_self;
};

constexpr const char* PYOBJECT_METATABLE = "luna.PyObject";

void pyobject_register_metatable(lua_State* L);

void pyobject_push(lua_State* L, PyObject* obj, PyObject* bound_self);

bool pyobject_is(lua_State* L, int idx);

PyObjectHandle* pyobject_check(lua_State* L, int idx);

} // namespace luna