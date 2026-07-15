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

static int pyobject_index(lua_State* L) {
    PyObjectHandle* handle = pyobject_check(L, 1);

    if (lua_isinteger(L, 2)) {
        lua_Integer index = lua_tointeger(L, 2);

        // Lua is 1 based, Python is 0 based
        PyObject* item = PySequence_GetItem(
            handle->obj,
            index - 1
        );

        if (!item) {
            if (PyErr_ExceptionMatches(PyExc_IndexError)) {
                PyErr_Clear();
                lua_pushnil(L);
                return 1;
            }

            errors_raise_from_python(L);
            return 0;
        }

        push_python_to_lua(L, item, nullptr);
        Py_DECREF(item);
        return 1;
    }


    const char* key = luaL_checkstring(L, 2);

    PyObject* attr = PyObject_GetAttrString(
        handle->obj,
        key
    );

    if (!attr) {
        errors_raise_from_python(L);
        return 0;
    }

    PyObject* parent_for_binding =
        PyCallable_Check(attr)
            ? handle->obj
            : nullptr;

    push_python_to_lua(
        L,
        attr,
        parent_for_binding
    );

    Py_DECREF(attr);

    return 1;
}


static int pyobject_newindex(lua_State* L) {
    PyObjectHandle* handle = pyobject_check(L, 1);

    PyObject* value = lua_to_python(L, 3);
    if (!value) {
        return luaL_error(L, "unsupported Lua value");
    }

    int rc = 0;

    if (lua_isinteger(L, 2)) {
        // Lua is 1 based, Python is 0 based
        lua_Integer index = lua_tointeger(L, 2);

        PyObject* key = PyLong_FromLong(static_cast<long>(index - 1));

        rc = PyObject_SetItem(handle->obj, key, value);

        Py_DECREF(key);
    } else {
        const char* key = luaL_checkstring(L, 2);

        rc = PyObject_SetAttrString(handle->obj, key, value);
    }

    Py_DECREF(value);

    if (rc != 0) {
        errors_raise_from_python(L);
        return 0;
    }

    return 0;
}

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
        PyTuple_SET_ITEM(py_args, i, converted);
    }

    PyObject* result = PyObject_CallObject(handle->obj, py_args);
    Py_DECREF(py_args);

    if (!result) {
        errors_raise_from_python(L);
        return 0;
    }

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