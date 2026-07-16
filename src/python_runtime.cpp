#include "python_runtime.hpp"

#include <Python.h>
#include <windows.h>
#include <string>
#include <cstdio>

#if defined(__linux__) || defined(__APPLE__)
    #if defined(LUNA_LIBPYTHON_PATH)
        #include <dlfcn.h>
    #endif
#endif

#ifndef LUNA_PYTHON_HOME
    #error "LUNA_PYTHON_HOME must be defined by the build system (wide string literal, e.g. L\"C:/Path/To/PythonXYZ\")"
#endif

namespace luna {

static DLL_DIRECTORY_COOKIE g_home_cookie = nullptr;
static DLL_DIRECTORY_COOKIE g_dlls_cookie = nullptr;
static DLL_DIRECTORY_COOKIE g_site_cookie = nullptr;
static DLL_DIRECTORY_COOKIE g_cv2_cookie = nullptr;

static bool g_initialized = false;

static std::wstring join_path(const std::wstring& base, const wchar_t* suffix) {
    return base + L"\\" + suffix;
}

static DLL_DIRECTORY_COOKIE add_dll_dir_checked(const std::wstring& path, const wchar_t* label) {
    DLL_DIRECTORY_COOKIE cookie = AddDllDirectory(path.c_str());
    if (!cookie) {
        DWORD err = GetLastError();
        fwprintf(stderr, L"[luna] AddDllDirectory failed for %ls (%ls) - GetLastError=%lu\n",
                 label, path.c_str(), err);
    } else {
        fwprintf(stderr, L"[luna] AddDllDirectory OK for %ls (%ls)\n", label, path.c_str());
    }
    return cookie;
}

void runtime_init()
{
    if (g_initialized)
        return;

    const std::wstring home = LUNA_PYTHON_HOME;
    const std::wstring exe = join_path(home, L"python.exe");
    const std::wstring dlls = join_path(home, L"DLLs");
    const std::wstring lib = join_path(home, L"Lib");
    const std::wstring site_packages = join_path(home, L"Lib\\site-packages");
    const std::wstring cv2_dir = join_path(site_packages, L"cv2");

    SetDefaultDllDirectories(
        LOAD_LIBRARY_SEARCH_DEFAULT_DIRS |
        LOAD_LIBRARY_SEARCH_USER_DIRS
    );

    g_home_cookie = add_dll_dir_checked(home, L"home");
    g_dlls_cookie = add_dll_dir_checked(dlls, L"DLLs");
    g_site_cookie = add_dll_dir_checked(site_packages, L"site-packages");
    g_cv2_cookie  = add_dll_dir_checked(cv2_dir, L"cv2");

    PyStatus status;
    PyConfig config;

    PyConfig_InitPythonConfig(&config);

    PyConfig_SetString(&config, &config.home, home.c_str());
    PyConfig_SetString(&config, &config.program_name, exe.c_str());

    config.module_search_paths_set = 1;

    PyWideStringList_Append(&config.module_search_paths, dlls.c_str());
    PyWideStringList_Append(&config.module_search_paths, home.c_str());
    PyWideStringList_Append(&config.module_search_paths, lib.c_str());
    PyWideStringList_Append(&config.module_search_paths, site_packages.c_str());

    status = Py_InitializeFromConfig(&config);

    PyConfig_Clear(&config);

    if (PyStatus_Exception(status))
    {
        Py_ExitStatusException(status);
    }

    g_initialized = true;
}

bool runtime_is_initialized()
{
    return g_initialized;
}

} // namespace luna