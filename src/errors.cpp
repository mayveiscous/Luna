#include "errors.hpp"

extern "C" {
#include <lauxlib.h>
}
#include <Python.h>

#include <string>

namespace luna {

static std::string safe_str(PyObject* obj, const char* fallback) {
    if (!obj) {
        return fallback;
    }
    PyObject* str = PyObject_Str(obj);
    if (!str) {
        PyErr_Clear();
        return fallback;
    }
    const char* s = PyUnicode_AsUTF8(str);
    std::string result = s ? s : fallback;
    Py_DECREF(str);
    return result;
}

void errors_raise_from_python(lua_State* L) {
    PyObject *type = nullptr, *value = nullptr, *traceback = nullptr;
    PyErr_Fetch(&type, &value, &traceback);
    PyErr_NormalizeException(&type, &value, &traceback);

    std::string type_name = "Exception";
    if (type) {
        PyObject* name = PyObject_GetAttrString(type, "__name__");
        if (name) {
            const char* s = PyUnicode_AsUTF8(name);
            if (s) {
                type_name = s;
            }
            Py_DECREF(name);
        } else {
            PyErr_Clear();
        }
    }

    std::string message = safe_str(value, "<no message>");

    Py_XDECREF(type);
    Py_XDECREF(value);
    Py_XDECREF(traceback);

    luaL_error(L, "%s: %s", type_name.c_str(), message.c_str());
}

} // namespace luna