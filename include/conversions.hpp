#pragma once

extern "C" {
#include <lua.h>
}
#include <Python.h>

// Centralizes every Lua <-> Python value conversion. Nothing outside
// this file should decide how a value crosses the boundary.

namespace luna {

// Converts the Lua value at `idx` into a new Python reference.
//
//   nil               -> None
//   boolean            -> bool
//   number (integer)   -> int
//   number (float)     -> float
//   string             -> str (UTF-8)
//   table, array-shaped -> list (recursively converted)
//   table, otherwise    -> dict (recursively converted)
//   userdata wrapping a PyObject -> that same PyObject (refcounted, not
//                                   copied)
//
// Returns nullptr (without setting a Python exception) if the Lua value
// has no supported conversion, e.g. a Lua function, thread, or light
// userdata. Callers are responsible for turning that into a Lua error
// with whatever context they have (argument index, attribute name...).
PyObject* lua_to_python(lua_State* L, int idx);

// Converts a Python object to Lua and pushes exactly one value.
//
//   None                -> nil
//   bool                -> boolean
//   int                 -> number (integer subtype)
//   float               -> number (float subtype)
//   str                 -> string (UTF-8)
//   list, tuple         -> table, 1-based array (recursively converted)
//   dict                -> table (recursively converted)
//   anything else       -> PyObject userdata (modules, classes,
//                          functions, bound methods, instances, etc.)
//
// `bound_self` is forwarded to pyobject_push when `obj` falls through to
// the userdata-wrapping case; it's ignored for every primitive
// conversion. See py_object.hpp for what it's for.
void push_python_to_lua(lua_State* L, PyObject* obj, PyObject* bound_self);

} // namespace luna