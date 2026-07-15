local py = require("python")

local cv = py.import("cv2")
local mp = py.import("mediapipe")
local np = py.import("numpy")

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

local connections = {
    {1,2}, {2,3}, {3,4}, {4,5},
    {1,6}, {6,7}, {7,8}, {8,9},
    {1,10}, {10,11}, {11,12}, {12,13},
    {1,14}, {14,15}, {15,16}, {16,17},
    {1,18}, {18,19}, {19,20}, {20,21},
    {1,6}, {1,10}, {1,14}, {1,18}
}

local points = {}

local smoothX = 0
local smoothY = 0

local alpha = 0.25

local lastX = 0
local lastY = 0


local function landmarkPosition(frame, landmark)
    return {
        math.floor(landmark.x * frame.shape[2]),
        math.floor(landmark.y * frame.shape[1])
    }
end


local function drawHandSkeleton(frame, hand)
    for _, c in ipairs(connections) do
        local a = landmarkPosition(frame, hand[c[1]])
        local b = landmarkPosition(frame, hand[c[2]])

        cv.line(
            frame,
            a,
            b,
            {255, 0, 0},
            2
        )
    end

    for _, landmark in ipairs(hand) do
        local p = landmarkPosition(frame, landmark)

        cv.circle(
            frame,
            p,
            5,
            {0, 255, 0},
            -1
        )
    end
end


local function isIndexUp(hand)
    return hand[9].y < hand[7].y
end


local canvas = nil


while true do
    local ok, frame = camera:read()

    if not ok then
        break
    end


    frame = cv.flip(frame, 1)


    if canvas == nil then
        canvas = np.zeros(
            {
                frame.shape[1],
                frame.shape[2],
                3
            },
            np.uint8
        )
    end


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

        for _, hand in ipairs(results.hand_landmarks) do

            drawHandSkeleton(frame, hand)


            local index = hand[9]

            local x = index.x * frame.shape[2]
            local y = index.y * frame.shape[1]


            smoothX = smoothX + alpha * (x - smoothX)
            smoothY = smoothY + alpha * (y - smoothY)


            local drawX = math.floor(smoothX)
            local drawY = math.floor(smoothY)


            if isIndexUp(hand) then

                local dx = drawX - lastX
                local dy = drawY - lastY

                local speed = math.sqrt(
                    dx * dx + dy * dy
                )


                local color = {
                    math.min(speed * 10, 255),
                    255,
                    255 - math.min(speed * 10, 255)
                }


                cv.line(
                    canvas,
                    {lastX, lastY},
                    {drawX, drawY},
                    color,
                    5
                )


                lastX = drawX
                lastY = drawY

            end
        end

    else
        lastX = 0
        lastY = 0
    end


    local output = cv.addWeighted(
        frame,
        0.7,
        canvas,
        1,
        0
    )


    cv.imshow(
        "Air Drawing",
        output
    )


    if cv.waitKey(1) == 27 then
        break
    end
end


camera:release()
cv.destroyAllWindows()