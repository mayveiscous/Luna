# Luna

Luna is a native Lua 5.3 module that embeds the CPython interpreter, letting Lua scripts `import` and use Python libraries — including C-extension packages like NumPy and OpenCV — as if they were ordinary Lua tables and functions.

## Demo

```lua
local python = require("python")

local np = python.import("numpy")
local a = np.array({1, 2, 3, 4})
print(a[1])          -- 1  (Lua-style 1-based indexing)
print(tostring(a))    -- calls Python's __str__ via __tostring

local cv2 = python.import("cv2")
local img = cv2.imread("photo.png")
```

## How it works

Luna links CPython directly into the Lua process and bridges values across the boundary through a small set of building blocks:

- **`python.import(name)`** calls `PyImport_ImportModule` and hands back the module wrapped as a Lua value.
- **`PyObjectHandle`** Every Python object that crosses into Lua is stored as userdata holding a `PyObject*` (plus an optional bound `self`, for methods). A shared metatable gives it Lua semantics:
  - `__index` - attribute/item access; Lua integer keys go through `PySequence_GetItem` (converted to 0-based), string keys go through `PyObject_GetAttrString`. Callable attributes are remembered as bound to their parent, so calling `obj:method(args)` is valid.
  - `__newindex` — mirrors the above for assignment (`PyObject_SetItem` / `PyObject_SetAttrString`).
  - `__call` — invokes the underlying `PyObject` with `PyObject_CallObject`, unpacking Python tuple returns into multiple Lua return values.
  - `__gc` — releases the Python reference (`Py_DECREF`) when the Lua userdata is collected.
  - `__tostring` — delegates to Python's `str()`.
- **Value conversion**  Luna converts plain values automatically in both directions so you rarely touch a `PyObjectHandle` directly:

  | Lua | Python |
  |---|---|
  | `nil` | `None` |
  | `boolean` | `bool` |
  | integer number | `int` |
  | float number | `float` |
  | `string` | `str` (UTF-8) |
  | array-like table (`#t == n`, all keys `1..n`) | `list` |
  | any other table | `dict` |
  | Python objects lacking a native mapping (custom classes, etc.) | pushed back as a wrapped `PyObjectHandle` |

  Numeric Python objects that only implement `__index__`/`__float__` (e.g. NumPy scalar types) still convert to plain Lua numbers rather than staying wrapped.

- **Error propagation** Luna resolves any pending Python exception by fetching, normalizing, and raising  a Lua error formatted as `ExceptionType: message`, so `pcall` works as expected on the Lua side.
- **Runtime bootstrap** Luna initializes the interpreter once per process with `Py_InitializeFromConfig`, pointing its module search path (including `site-packages`) at an embedded Python distribution baked into the build.

## Usage notes

- **Indexing is 1-based** `obj[1]` maps to Python index `0`.
- **Method calls** on wrapped objects work naturally: `obj:method(x, y)` — Luna detects the `self`-binding pattern and won't double-pass it.
- **Tables sent into Python** are inferred as `list` vs `dict` based on whether they're a dense 1-based array; sparse or mixed-key tables become `dict`s with Lua's keys converted individually.
- **Errors** from Python (import failures, attribute errors, exceptions raised inside a call) surface as normal Lua errors — wrap calls in `pcall` if you want to handle them.

## Status / limitations

- Windows-only build path
- No support yet for passing Lua functions as Python callbacks.
- Both Python 313 and any packages must already be installed
- Only tuple-return unpacking is handled for multi-value Python returns; generators/iterators aren't specially bridged.