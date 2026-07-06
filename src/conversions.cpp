#include "conversions.hpp"

#include "py_object.hpp"

extern "C" {
#include <lauxlib.h>
}

namespace luna {

static PyObject* lua_table_to_python(lua_State* L, int idx) {
    idx = lua_absindex(L, idx);

    // A table counts as a "simple list" if its raw length covers every
    // key present, i.e. it's a dense 1..n integer-keyed array with no
    // extra string keys mixed in. Anything else becomes a dict.
    lua_Integer array_len = static_cast<lua_Integer>(lua_rawlen(L, idx));

    lua_Integer total_keys = 0;
    lua_pushnil(L);
    while (lua_next(L, idx) != 0) {
        total_keys++;
        lua_pop(L, 1); // pop value, keep key for the next lua_next
    }

    if (total_keys == 0) {
        return PyList_New(0);
    }

    bool is_array = (total_keys == array_len);

    if (is_array) {
        PyObject* list = PyList_New(array_len);
        if (!list) {
            return nullptr;
        }
        for (lua_Integer i = 1; i <= array_len; ++i) {
            lua_rawgeti(L, idx, i);
            PyObject* item = lua_to_python(L, -1);
            lua_pop(L, 1);
            if (!item) {
                Py_DECREF(list);
                return nullptr;
            }
            PyList_SET_ITEM(list, i - 1, item); // steals reference
        }
        return list;
    }

    PyObject* dict = PyDict_New();
    if (!dict) {
        return nullptr;
    }
    lua_pushnil(L);
    while (lua_next(L, idx) != 0) {
        // key at -2, value at -1
        PyObject* pkey = lua_to_python(L, -2);
        PyObject* pval = pkey ? lua_to_python(L, -1) : nullptr;
        if (!pkey || !pval) {
            Py_XDECREF(pkey);
            Py_XDECREF(pval);
            lua_pop(L, 2); // pop key + value, table conversion failed
            Py_DECREF(dict);
            return nullptr;
        }
        PyDict_SetItem(dict, pkey, pval); // does not steal
        Py_DECREF(pkey);
        Py_DECREF(pval);
        lua_pop(L, 1); // pop value, keep key for the next lua_next
    }
    return dict;
}

PyObject* lua_to_python(lua_State* L, int idx) {
    idx = lua_absindex(L, idx);

    switch (lua_type(L, idx)) {
        case LUA_TNIL:
            Py_RETURN_NONE;

        case LUA_TBOOLEAN:
            return PyBool_FromLong(lua_toboolean(L, idx));

        case LUA_TNUMBER:
            if (lua_isinteger(L, idx)) {
                return PyLong_FromLongLong(static_cast<long long>(lua_tointeger(L, idx)));
            }
            return PyFloat_FromDouble(static_cast<double>(lua_tonumber(L, idx)));

        case LUA_TSTRING: {
            size_t len = 0;
            const char* s = lua_tolstring(L, idx, &len);
            return PyUnicode_FromStringAndSize(s, static_cast<Py_ssize_t>(len));
        }

        case LUA_TTABLE:
            return lua_table_to_python(L, idx);

        case LUA_TUSERDATA:
            if (pyobject_is(L, idx)) {
                PyObjectHandle* handle = pyobject_check(L, idx);
                Py_INCREF(handle->obj);
                return handle->obj;
            }
            return nullptr;

        default:
            // Functions, threads, light userdata, etc: no mapping.
            return nullptr;
    }
}

void push_python_to_lua(lua_State* L, PyObject* obj, PyObject* bound_self) {
    if (obj == Py_None) {
        lua_pushnil(L);
        return;
    }

    if (PyBool_Check(obj)) {
        lua_pushboolean(L, obj == Py_True);
        return;
    }

    if (PyLong_Check(obj)) {
        int overflow = 0;
        long long v = PyLong_AsLongLongAndOverflow(obj, &overflow);
        if (overflow == 0) {
            lua_pushinteger(L, static_cast<lua_Integer>(v));
        } else {
            // Doesn't fit in a 64-bit integer; fall back to a double
            // rather than failing outright.
            PyErr_Clear();
            lua_pushnumber(L, PyLong_AsDouble(obj));
        }
        return;
    }

    if (PyFloat_Check(obj)) {
        lua_pushnumber(L, PyFloat_AS_DOUBLE(obj));
        return;
    }

    if (PyUnicode_Check(obj)) {
        Py_ssize_t len = 0;
        const char* s = PyUnicode_AsUTF8AndSize(obj, &len);
        if (s) {
            lua_pushlstring(L, s, static_cast<size_t>(len));
        } else {
            PyErr_Clear();
            lua_pushstring(L, "<unencodable python string>");
        }
        return;
    }

    if (PyList_Check(obj)) {
        Py_ssize_t n = PyList_GET_SIZE(obj);
        lua_createtable(L, static_cast<int>(n), 0);
        for (Py_ssize_t i = 0; i < n; ++i) {
            push_python_to_lua(L, PyList_GET_ITEM(obj, i), nullptr);
            lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
        }
        return;
    }

    if (PyTuple_Check(obj)) {
        Py_ssize_t n = PyTuple_GET_SIZE(obj);
        lua_createtable(L, static_cast<int>(n), 0);
        for (Py_ssize_t i = 0; i < n; ++i) {
            push_python_to_lua(L, PyTuple_GET_ITEM(obj, i), nullptr);
            lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
        }
        return;
    }

    if (PyDict_Check(obj)) {
        lua_newtable(L);
        PyObject* key = nullptr;
        PyObject* value = nullptr;
        Py_ssize_t pos = 0;
        while (PyDict_Next(obj, &pos, &key, &value)) {
            push_python_to_lua(L, key, nullptr);
            push_python_to_lua(L, value, nullptr);
            lua_settable(L, -3);
        }
        return;
    }

    // Everything else -- modules, classes, functions, bound methods,
    // instances, numpy arrays, whatever -- stays a live Python object.
    pyobject_push(L, obj, bound_self);
}

} // namespace luna