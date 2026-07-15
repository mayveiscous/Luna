local py = require("python")

local json = py.import("json")

local tbl = json.loads('{"x":1,"y":[2,3]}')

print(tbl.x)
print(tbl.y[1])