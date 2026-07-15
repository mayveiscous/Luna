local py = require("python")

local cv = py.import("cv2")
local mp = py.import("mediapipe")

local camera = cv.VideoCapture(0)

local modelPath = "./hand_landmarker.task"

local baseOptions = mp.tasks.BaseOptions(modelPath)

local options = mp.tasks.vision.HandLandmarkerOptions(
    baseOptions,
    mp.tasks.vision.RunningMode.VIDEO,
    1,
    0.8,
    0.8,
    0.8
)

local detector = mp.tasks.vision.HandLandmarker:create_from_options(options)

local timestamp = 0

while true do
    local ok, frame = camera:read()

    if not ok then
        break
    end

    frame = cv.flip(frame, 1)

    local rgbFrame = cv.cvtColor(
        frame,
        cv.COLOR_BGR2RGB
    )

    local mpImage = mp.Image(
        mp.ImageFormat.SRGB,
        rgbFrame
    )

    timestamp = timestamp + 33

    local results = detector:detect_for_video(
        mpImage,
        timestamp
    )

    if results.hand_landmarks and #results.hand_landmarks > 0 then
        print(results)
    end


    cv.imshow("Webcam", frame)

    if cv.waitKey(1) == 27 then
        break
    end
end

camera:release()
cv.destroyAllWindows()