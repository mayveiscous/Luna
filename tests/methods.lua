--[[
    test method calls like:

    local py = require("python")
    local cv = py.import("cv2")

    local cam = cv.VideoCapture(0)
    cam:read() <- this
]]