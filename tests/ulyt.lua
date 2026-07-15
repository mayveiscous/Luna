local py = require("python")

local cv2 = py.import("cv2")
local numpy = py.import("numpy")
local ulyt = py.import("ultralytics")

local model = ulyt.YOLO("yolov8n.pt")
local camera = cv2.VideoCapture(0)

local function drawBox(box)
    local x1 = math.floor(tonumber(tostring(box[1])))
    local y1 = math.floor(tonumber(tostring(box[2])))
    local x2 = math.floor(tonumber(tostring(box[3])))
    local y2 = math.floor(tonumber(tostring(box[4])))

    cv2.rectangle(frame, {x1, y1}, {x2, y2}, {0, 255, 0}, 2)
end

while true do 
    local ok, frame = camera:read()
    if not ok then
        break
    end

    local result = model(frame)
    cv2.imshow("frame", frame)

    -- lua tables are 1 based
    -- luna converts a python list to a lua table
    -- making this a 1 based lua table
    local boxes = result[1].boxes.xyxy:numpy()
    for _, box in ipairs(boxes) do 
        drawBox(box)
    end

    if cv2.waitKey(1) == 27 then
        break
    end
end

camera:release()
cv2.destroyAllWindows()