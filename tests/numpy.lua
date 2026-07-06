-- Demonstrates that a real third-party C-extension library (numpy) works
-- through the exact same generic mechanism as builtins/io/math in the
-- test suite -- nothing in the bridge knows numpy exists.

package.cpath = package.cpath .. ";./build/?.dll"
print(package.cpath)

local py = require("python")
local np = py.import("numpy")

print("numpy version:", np.__version__)

local a = np.array({1, 2, 3, 4, 5})
print("array:", tostring(a))

print("shape:", tostring(a.shape))
print("dtype:", tostring(a.dtype))

local total = a:sum()
print("sum:", tostring(total))
assert(tostring(total) == "15", "ndarray sum() should stringify to 15")

local mean = np.mean(a)
print("mean:", mean)
assert(mean == 3.0, "mean should be 3.0")

print("numpy.lua: OK")