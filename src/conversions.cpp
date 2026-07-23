#include "conversions.hpp"

#include "py_object.hpp"

extern "C" {
#include <lauxlib.h>
}

namespace luna {

// take in a lua table and produce
// either a PyList or PyDict
static PyObject* lua_table_to_python(lua_State* L, int idx) {
    idx = lua_absindex(L, idx);

    lua_Integer array_len = static_cast<lua_Integer>(lua_rawlen(L, idx));

    lua_Integer total_keys = 0;
    lua_pushnil(L);
    while (lua_next(L, idx) != 0) {
        total_keys++;
        lua_pop(L, 1);
    }
    // empty table -> empty list
    if (total_keys == 0) {
        return PyList_New(0);
    }

    bool is_array = (total_keys == array_len);
    
    // if the table is an array
    // conversion is very direct
    if (is_array) {
        PyObject* list = PyList_New(array_len);
        if (!list) {
            return nullptr;
        }
      
        // rawget the value at index 'idx'
        // and convert it to a PyObject
        for (lua_Integer i = 1; i <= array_len; ++i) {
            lua_rawgeti(L, idx, i);
            PyObject* item = lua_to_python(L, -1);
            lua_pop(L, 1);
            if (!item) {
                Py_DECREF(list);
                return nullptr;
            }
            PyList_SET_ITEM(list, i - 1, item);
        }
        return list;
    }
    

    // non-array lua tables
    // need both keys and values 
    // converted
    PyObject* dict = PyDict_New();
    if (!dict) {
        return nullptr;
    }
    lua_pushnil(L);
    while (lua_next(L, idx) != 0) {
        // attempt to convert the key and value
        PyObject* pkey = lua_to_python(L, -2);
        PyObject* pval = pkey ? lua_to_python(L, -1) : nullptr;

        // back out if either failed
        if (!pkey || !pval) {
            Py_XDECREF(pkey);
            Py_XDECREF(pval);
            lua_pop(L, 2);
            Py_DECREF(dict);
            return nullptr;
        }

        // set the new item and pop
        PyDict_SetItem(dict, pkey, pval);
        Py_DECREF(pkey);
        Py_DECREF(pval);
        lua_pop(L, 1);
    }
    return dict;
}

// converts a lua value to a PyObject
PyObject* lua_to_python(lua_State* L, int idx) {
    idx = lua_absindex(L, idx);
   
    // most conversions are direct
    // one-line CPython API calls
    switch (lua_type(L, idx)) {
        // nil maps directly to None
        case LUA_TNIL:
            Py_RETURN_NONE;
        // booleans (0, 1) use
        // PyBool_FromLong() to turn 0 -> False and 1 -> True
        case LUA_TBOOLEAN:
            return PyBool_FromLong(lua_toboolean(L, idx));
        // lua collapses integers and floats together
        // but exposes lua_isinteger() and lua_isfloat()
        // integers -> PyLong_FromLongLong()
        // flosts -> PyFloat_FromDouble()
        case LUA_TNUMBER:
            if (lua_isinteger(L, idx)) {
                return PyLong_FromLongLong(static_cast<long long>(lua_tointeger(L, idx)));
            }
            return PyFloat_FromDouble(static_cast<double>(lua_tonumber(L, idx)));
        // PyUnicode_FromStringAndSize() needs a size
        // so first count length
        // then convert
        case LUA_TSTRING: {
            size_t len = 0;
            const char* s = lua_tolstring(L, idx, &len);
            return PyUnicode_FromStringAndSize(s, static_cast<Py_ssize_t>(len));
        }
        // use above function
        // to split tables into
        // lists or dicts as needed
        case LUA_TTABLE:
            return lua_table_to_python(L, idx);
        
        // convert userdata into a handle
        case LUA_TUSERDATA:
            if (pyobject_is(L, idx)) {
                PyObjectHandle* handle = pyobject_check(L, idx);
                Py_INCREF(handle->obj);
                return handle->obj;
            }
            return nullptr;
        // cant convert, return nullptr
        default:
            return nullptr;
    }
}

// push a python value
// lua-typed value
void push_python_to_lua(lua_State* L, PyObject* obj, PyObject* bound_self) {
    // None -> nil
    if (obj == Py_None) {
        lua_pushnil(L);
        return;
    }

    // True -> true
    // False -> false
    if (PyBool_Check(obj)) {
        lua_pushboolean(L, obj == Py_True);
        return;
    }

    // use PyLongAsLongLongAndOverflow()
    // to differentiate between number and int
    // has overflow -> push int
    // otherwise push number
    if (PyLong_Check(obj)) {
        int overflow = 0;
        long long v = PyLong_AsLongLongAndOverflow(obj, &overflow);
        if (overflow == 0) {
            lua_pushinteger(L, static_cast<lua_Integer>(v));
        } else {
            PyErr_Clear();
            lua_pushnumber(L, PyLong_AsDouble(obj));
        }
        return;
    }
    
    // if PyFloat -> lua number
    // lua automatically resolves
    // int and float
    if (PyFloat_Check(obj)) {
        lua_pushnumber(L, PyFloat_AS_DOUBLE(obj));
        return;
    }

    // use PyUnicodeCheck() 
    // to convert strings
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

    // use PyList_Check() and lua_createtable()
    // to convert lists
    if (PyList_Check(obj)) {
        Py_ssize_t n = PyList_GET_SIZE(obj);
        lua_createtable(L, static_cast<int>(n), 0);
        for (Py_ssize_t i = 0; i < n; ++i) {
            push_python_to_lua(L, PyList_GET_ITEM(obj, i), nullptr);
            lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
        }
        return;
    }

    // use Py_TupleCheck() and lua_createtable()
    // to convert tuples
    if (PyTuple_Check(obj)) {
        Py_ssize_t n = PyTuple_GET_SIZE(obj);
        lua_createtable(L, static_cast<int>(n), 0);
        for (Py_ssize_t i = 0; i < n; ++i) {
            push_python_to_lua(L, PyTuple_GET_ITEM(obj, i), nullptr);
            lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
        }
        return;
    }

    // use Dict_Check and lua_createtable()
    // to convert dicts
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

    if (PyIndex_Check(obj)) {
        PyObject* as_int = PyNumber_Index(obj);
        if (as_int) {
            int overflow = 0;
            long long v = PyLong_AsLongLongAndOverflow(as_int, &overflow);
            if (overflow == 0) {
                lua_pushinteger(L, static_cast<lua_Integer>(v));
            } else {
                PyErr_Clear();
                lua_pushnumber(L, PyLong_AsDouble(as_int));
            }
            Py_DECREF(as_int);
            return;
        }
        PyErr_Clear();
    }

    if (Py_TYPE(obj)->tp_as_number && Py_TYPE(obj)->tp_as_number->nb_float) {
        double d = PyFloat_AsDouble(obj);
        if (!(d == -1.0 && PyErr_Occurred())) {
            lua_pushnumber(L, d);
            return;
        }
        PyErr_Clear();
    }

    pyobject_push(L, obj, bound_self);
}

} // namespace luna