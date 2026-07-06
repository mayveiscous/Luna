#pragma once

// Owns the embedded CPython interpreter lifecycle.
//
// Design note: for this MVP the runtime is initialized once, lazily, on
// first use, and is deliberately never finalized (no Py_FinalizeEx call
// anywhere). Lua's garbage collector does not guarantee finalizer order
// at lua_close() time, so any wrapped PyObject userdata that outlives a
// Py_Finalize() call would crash on __gc. Skipping finalization and
// letting the OS reclaim the process on exit sidesteps that entirely.
// If/when explicit shutdown is needed, it has to be paired with a way to
// guarantee every PyObject userdata has already been collected first.

namespace luna {

// Initializes the embedded CPython interpreter if it hasn't been already.
// Safe to call repeatedly from anywhere in the bridge.
void runtime_init();

// Returns whether the interpreter has been initialized.
bool runtime_is_initialized();

} // namespace luna