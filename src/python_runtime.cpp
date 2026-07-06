#include "python_runtime.hpp"

#include <Python.h>

#if defined(__linux__) || defined(__APPLE__)
    #if defined(LUNA_LIBPYTHON_PATH)
        #include <dlfcn.h>
    #endif
#endif

namespace luna {

static bool g_initialized = false;

void runtime_init()
{
    if (g_initialized)
        return;

#if (defined(__linux__) || defined(__APPLE__)) && defined(LUNA_LIBPYTHON_PATH)
    dlopen(LUNA_LIBPYTHON_PATH, RTLD_NOW | RTLD_GLOBAL);
#endif

    Py_Initialize();
    g_initialized = true;
}

bool runtime_is_initialized()
{
    return g_initialized;
}

} // namespace luna