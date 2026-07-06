py = require("python")

cv = py.import("cv2")

camera = cv.VideoCapture(0)

while true do
    ok, frame = camera:read()

    cv.imshow("Camera", frame)

    if cv.waitKey(1) == 27 then
        break
    end
end