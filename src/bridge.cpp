#include "bridge.hpp"

#include "errors.hpp"
#include "py_object.hpp"
#include "python_runtime.hpp"

extern "C" {
#include <lauxlib.h>
}
#include <Python.h>

namespace luna {

int bridge_import(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);

    runtime_init();

    PyObject* module = PyImport_ImportModule(name);
    if (!module) {
        errors_raise_from_python(L);
        return 0;
    }

    pyobject_push(L, module, nullptr);
    Py_DECREF(module);
    return 1;
}

} // namespace luna