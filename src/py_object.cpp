#include "py_object.hpp"

#include "conversions.hpp"
#include "errors.hpp"

extern "C" {
#include <lauxlib.h>
}

namespace luna {

static int pyobject_gc(lua_State* L) {
    auto* handle = static_cast<PyObjectHandle*>(luaL_checkudata(L, 1, PYOBJECT_METATABLE));
    if (handle->obj) {
        Py_DECREF(handle->obj);
        handle->obj = nullptr;
    }
    handle->bound_self = nullptr;
    return 0;
}

static int pyobject_tostring(lua_State* L) {
    PyObjectHandle* handle = pyobject_check(L, 1);
    PyObject* str = PyObject_Str(handle->obj);
    if (!str) {
        PyErr_Clear();
        lua_pushfstring(L, "<python object %p>", static_cast<void*>(handle->obj));
        return 1;
    }
    const char* s = PyUnicode_AsUTF8(str);
    lua_pushstring(L, s ? s : "<python object>");
    Py_DECREF(str);
    return 1;
}

// __index(self, key) -- attribute lookup, e.g. `np.array` or `camera.read`.
static int pyobject_index(lua_State* L) {
    PyObjectHandle* handle = pyobject_check(L, 1);
    const char* key = luaL_checkstring(L, 2);

    PyObject* attr = PyObject_GetAttrString(handle->obj, key);
    if (!attr) {
        errors_raise_from_python(L);
        return 0;
    }

    // Only remember the parent object as a binding source when the
    // attribute is actually callable -- that's the only case __call's
    // self-stripping logic needs to look at.
    PyObject* parent_for_binding = PyCallable_Check(attr) ? handle->obj : nullptr;

    push_python_to_lua(L, attr, parent_for_binding);
    Py_DECREF(attr);
    return 1;
}

// __newindex(self, key, value) -- attribute assignment, e.g. `object.value = 42`.
static int pyobject_newindex(lua_State* L) {
    PyObjectHandle* handle = pyobject_check(L, 1);
    const char* key = luaL_checkstring(L, 2);

    PyObject* value = lua_to_python(L, 3);
    if (!value) {
        return luaL_error(L, "unsupported Lua value for python attribute '%s'", key);
    }

    int rc = PyObject_SetAttrString(handle->obj, key, value);
    Py_DECREF(value);
    if (rc != 0) {
        errors_raise_from_python(L);
        return 0;
    }
    return 0;
}

// __call(self, ...) -- invoking a Python callable, including the
// self-stripping needed to make `obj:method(args)` behave correctly
// against Python's already-bound methods. See PyObjectHandle::bound_self
// in py_object.hpp for the full explanation.
static int pyobject_call(lua_State* L) {
    PyObjectHandle* handle = pyobject_check(L, 1);

    int first_arg_index = 2;
    int nargs = lua_gettop(L) - 1;

    if (handle->bound_self && nargs >= 1 && pyobject_is(L, 2)) {
        PyObjectHandle* first_arg = pyobject_check(L, 2);
        if (first_arg->obj == handle->bound_self) {
            first_arg_index = 3;
            nargs -= 1;
        }
    }

    PyObject* py_args = PyTuple_New(nargs);
    if (!py_args) {
        return luaL_error(L, "failed to allocate python argument tuple");
    }

    for (int i = 0; i < nargs; ++i) {
        PyObject* converted = lua_to_python(L, first_arg_index + i);
        if (!converted) {
            Py_DECREF(py_args);
            return luaL_error(L, "unsupported Lua value at argument %d", i + 1);
        }
        PyTuple_SET_ITEM(py_args, i, converted); // steals reference
    }

    PyObject* result = PyObject_CallObject(handle->obj, py_args);
    Py_DECREF(py_args);

    if (!result) {
        errors_raise_from_python(L);
        return 0;
    }

    // Python's convention for a function returning "multiple values" is
    // a tuple; Lua's is genuinely separate return values. Map one to the
    // other right here, at the call boundary -- this is exactly what
    // makes `local ok, frame = camera:read()` from the brief's own
    // example work. A tuple encountered anywhere else (an attribute, a
    // dict value, a list element) still becomes a single Lua table via
    // push_python_to_lua, since a table field can't hold "multiple
    // values"; this special case only applies to a call's direct result.
    if (PyTuple_Check(result)) {
        Py_ssize_t n = PyTuple_GET_SIZE(result);
        for (Py_ssize_t i = 0; i < n; ++i) {
            push_python_to_lua(L, PyTuple_GET_ITEM(result, i), nullptr);
        }
        Py_DECREF(result);
        return static_cast<int>(n);
    }

    push_python_to_lua(L, result, nullptr);
    Py_DECREF(result);
    return 1;
}

void pyobject_register_metatable(lua_State* L) {
    if (luaL_newmetatable(L, PYOBJECT_METATABLE)) {
        lua_pushcfunction(L, pyobject_index);
        lua_setfield(L, -2, "__index");

        lua_pushcfunction(L, pyobject_newindex);
        lua_setfield(L, -2, "__newindex");

        lua_pushcfunction(L, pyobject_call);
        lua_setfield(L, -2, "__call");

        lua_pushcfunction(L, pyobject_gc);
        lua_setfield(L, -2, "__gc");

        lua_pushcfunction(L, pyobject_tostring);
        lua_setfield(L, -2, "__tostring");
    }
    lua_pop(L, 1);
}

void pyobject_push(lua_State* L, PyObject* obj, PyObject* bound_self) {
    Py_INCREF(obj);
    auto* handle = static_cast<PyObjectHandle*>(lua_newuserdata(L, sizeof(PyObjectHandle)));
    handle->obj = obj;
    handle->bound_self = bound_self;
    luaL_getmetatable(L, PYOBJECT_METATABLE);
    lua_setmetatable(L, -2);
}

bool pyobject_is(lua_State* L, int idx) {
    if (!lua_isuserdata(L, idx)) {
        return false;
    }
    if (!lua_getmetatable(L, idx)) {
        return false;
    }
    luaL_getmetatable(L, PYOBJECT_METATABLE);
    bool equal = lua_rawequal(L, -1, -2);
    lua_pop(L, 2);
    return equal;
}

PyObjectHandle* pyobject_check(lua_State* L, int idx) {
    return static_cast<PyObjectHandle*>(luaL_checkudata(L, idx, PYOBJECT_METATABLE));
}

} // namespace luna