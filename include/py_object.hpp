#pragma once

extern "C" {
#include <lua.h>
}
#include <Python.h>

// Represents a Python object wrapped as Lua userdata, plus the metatable
// that gives it __index / __newindex / __call / __gc / __tostring
// behavior. This is the mechanism the whole bridge is built on: every
// non-primitive Python value (modules, classes, functions, bound
// methods, instances, numpy arrays, whatever) is pushed to Lua as one of
// these, never copied.

namespace luna {

// Userdata payload. One of these lives inside every wrapped Python
// value's Lua userdata block.
struct PyObjectHandle {
    // Strong reference. Released (Py_DECREF'd) in __gc.
    PyObject* obj;

    // Borrowed reference, may be null. Set only when `obj` is callable
    // and was retrieved via attribute lookup on another wrapped object
    // (e.g. `camera.read` fetched off `camera`). Python's attribute
    // lookup already returns a bound method in that case, so if Lua's
    // colon-call sugar (`camera:read()`) later hands this same parent
    // object back as an implicit first argument, __call needs to
    // recognize and drop it rather than pass `self` twice. Not owned:
    // this pointer is never dereferenced except for identity comparison
    // against another handle's `obj`.
    PyObject* bound_self;
};

// Name of the Lua registry entry for the PyObject metatable.
constexpr const char* PYOBJECT_METATABLE = "luna.PyObject";

// Registers the PyObject metatable in the Lua registry. Must be called
// once during module setup (from luaopen_python), before any wrapped
// object is pushed.
void pyobject_register_metatable(lua_State* L);

// Wraps `obj` into a new Lua userdata and pushes it onto the stack.
// Takes a new reference to `obj` internally (via Py_INCREF); the caller
// keeps ownership of whatever reference they passed in and must release
// it themselves as usual.
//
// `bound_self`, if non-null, records the parent object `obj` was
// retrieved from, for __call's self-stripping logic. Pass nullptr when
// there is no such parent (e.g. values returned from a function call,
// or top-level module imports).
void pyobject_push(lua_State* L, PyObject* obj, PyObject* bound_self);

// True if the value at `idx` is a wrapped PyObject userdata.
bool pyobject_is(lua_State* L, int idx);

// Returns the handle at `idx`, raising a Lua error via luaL_error (and
// not returning) if the value there isn't a wrapped PyObject.
PyObjectHandle* pyobject_check(lua_State* L, int idx);

} // namespace luna