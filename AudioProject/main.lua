--------------------------------------------------------
-- main.lua — Sine + Linear Chirplet, mouse + key control
--------------------------------------------------------

local mode = "sine"

-- Audio parameters
local FS = 44100
local bufferDuration = 1.0                    -- seconds (longer = clearer sweep)
local bufferSamples = math.floor(FS * bufferDuration)
local source

-- Visual parameters
local length = 2048                           -- samples for drawing

-- Shared: mouse-controlled
local freq = 440                              -- base frequency (start freq for chirp)
local amp  = 0.5                              -- overall amplitude
local artificialPeriod = 50

-- Chirplet parameters (envelope + sweep)
local tc     = 0.5                            -- time center (seconds, 0..bufferDuration)
local logDt  = -1.0                           -- log width of Gaussian
local c_rate = 1000                           -- sweep amount in Hz over the buffer

--------------------------------------------------------
-- Map mouse -> freq (x), amp (y)
--------------------------------------------------------
local function updateMouseParams()
    local x, y = love.mouse.getPosition()
    local w, h = love.graphics.getDimensions()

    -- Frequency 0..2000 Hz
    local maxFreq = 2000
    freq = (x / w) * maxFreq
    if freq < 1 then freq = 1 end

    -- Amplitude 1 at top, 0 at bottom
    amp = 1.0 - (y / h)
    if amp < 0 then amp = 0 end
    if amp > 1 then amp = 1 end
end

--------------------------------------------------------
-- AUDIO: Build buffer for current mode
--------------------------------------------------------
local function rebuildAudio()
    local sd = love.sound.newSoundData(bufferSamples, FS, 16, 1)

    if mode == "sine" then
        for i = 0, bufferSamples - 1 do
            local t = i / FS
            local sample = amp * math.sin(2 * math.pi * freq * t)
            sd:setSample(i, sample)
        end
    else
        -- Linear chirp with Gaussian envelope

        -- f_start = freq
        -- f_end   = freq + c_rate
        local f0 = freq
        local sweep = c_rate                       -- total Hz change over buffer
        local k = sweep / bufferDuration          -- Hz per second

        local Dt = math.exp(logDt)
        if Dt < 1e-4 then Dt = 1e-4 end

        -- clamp inside buffer
        if tc < 0 then tc = 0 end
        if tc > bufferDuration then tc = bufferDuration end

        for i = 0, bufferSamples - 1 do
            local t = i / FS

            -- Gaussian envelope
            local shift = t - tc
            local env = math.exp(-0.5 * (shift / Dt)^2)

            -- Phase for linear chirp: f(t) = f0 + k t
            local phase = 2 * math.pi * (f0 * t + 0.5 * k * t * t)
            local sample = amp * env * math.sin(phase)

            sd:setSample(i, sample)
        end
    end

    if source then source:stop() end
    source = love.audio.newSource(sd, "static")
    source:setLooping(true)
    source:play()
end

--------------------------------------------------------
-- VISUAL: Sine
--------------------------------------------------------
local function generateSineVisual()
    local data = {}
    data.length = length

    for i = 0, length - 1 do
        local t = (i / (length - 1)) * bufferDuration

        t = t / artificialPeriod   -- <<< stretch the displayed period 10×

        data[i] = amp * math.sin(2 * math.pi * freq * t)
    end

    return data
end


--------------------------------------------------------
-- VISUAL: Chirplet (real + imaginary)
--------------------------------------------------------
local function generateChirpletVisual()
    local real = {}
    local imag = {}

    real.length = length
    imag.length = length

    local f0 = freq
    local sweep = c_rate
    local k = sweep / bufferDuration

    local Dt = math.exp(logDt)
    if Dt < 1e-4 then Dt = 1e-4 end

    local tc_clamped = math.min(math.max(tc, 0), bufferDuration)

    for i = 0, length - 1 do
        local t = (i / (length - 1)) * bufferDuration

        -- Gaussian
        local shift = t - tc_clamped
        local env = math.exp(-0.5 * (shift / Dt)^2)

        -- Linear chirp phase
        local phase = 2 * math.pi * (f0 * t + 0.5 * k * t * t)

        -- Real (sin) and imaginary (cos)
        real[i] = amp * env * math.sin(phase)
        imag[i] = amp * env * math.cos(phase)
    end

    return real, imag
end



--------------------------------------------------------
-- DRAW WAVEFORM
--------------------------------------------------------
--------------------------------------------------------
-- DRAW WAVEFORM (with color)
--------------------------------------------------------
local function drawSignal(sig, r, g, b, yOffset)
    local w, h = love.graphics.getDimensions()
    local midY = yOffset or (h * 0.6)
    local scale = h * 0.25

    local pts = {}
    for i = 0, sig.length - 1 do
        local x = (i / (sig.length - 1)) * w
        local y = midY + sig[i] * scale
        table.insert(pts, x)
        table.insert(pts, y)
    end

    love.graphics.setColor(r or 1, g or 1, b or 1)
    love.graphics.setLineWidth(2)
    if #pts >= 4 then
        love.graphics.line(pts)
    end
end



--------------------------------------------------------
-- LOVE CALLBACKS
--------------------------------------------------------
function love.load()
    love.window.setTitle("Sine + Linear Chirplet (mouse + keys)")
    love.graphics.setBackgroundColor(0.08, 0.08, 0.12)
    rebuildAudio()
end

function love.update(dt)
    updateMouseParams()
    rebuildAudio()  -- cheap: small buffer, single channel
end

function love.keypressed(key)
    if key == "escape" then
        love.event.quit()
    elseif key == "q" then
        mode = (mode == "sine") and "chirplet" or "sine"
        rebuildAudio()

    -- tc controls (0..bufferDuration)
    elseif key == "t" then
        tc = math.min(tc + 0.02, bufferDuration)
        rebuildAudio()
    elseif key == "g" then
        tc = math.max(tc - 0.02, 0)
        rebuildAudio()

    -- logDt controls (Gaussian width)
    elseif key == "y" then
        logDt = math.min(logDt + 0.1, 2)
        rebuildAudio()
    elseif key == "h" then
        logDt = math.max(logDt - 0.1, -5)
        rebuildAudio()

    -- c_rate controls (sweep amount in Hz)
    elseif key == "u" then
        c_rate = math.min(c_rate + 200, 8000)
        rebuildAudio()
    elseif key == "j" then
        c_rate = math.max(c_rate - 200, -8000)
        rebuildAudio()
    end
end

function love.draw()
    local x, y = love.mouse.getPosition()
    local w, h = love.graphics.getDimensions()

    -- Generate signals
    local sigReal, sigImag
    if mode == "sine" then
        sigReal = generateSineVisual()
    else
        sigReal, sigImag = generateChirpletVisual()
    end

    -- Draw signals
    if mode == "sine" then
        -- Sine: green, uses default midY
        drawSignal(sigReal, 0, 0.8, 0.4)
    else
        -- Chirplet real (red), centered lower
        drawSignal(sigReal, 1.0, 0.2, 0.2, h * 0.75)

        -- Chirplet imaginary (indigo), centered higher
        drawSignal(sigImag, 0.3, 0.2, 1.0, h * 0.25)
    end

    -- UI text
    love.graphics.setColor(1, 1, 1)
    love.graphics.print("Mode: " .. mode .. "  (Q to toggle)", 20, 20)
    love.graphics.print(string.format("Mouse X freq: %.1f Hz", freq), 20, 50)
    love.graphics.print(string.format("Mouse Y amp:  %.2f", amp), 20, 70)

    if mode == "chirplet" then
        local f_start = freq
        local f_end = freq + c_rate

        love.graphics.print("Chirplet params:", 20, 110)
        love.graphics.print(string.format("tc: %.3f s (T/G)", tc), 20, 130)
        love.graphics.print(string.format("logDt: %.3f (Y/H)", logDt), 20, 150)
        love.graphics.print(string.format("c_rate: %.1f Hz (U/J)", c_rate), 20, 170)
        love.graphics.print(string.format("f_start: %.1f  f_end: %.1f", f_start, f_end), 20, 190)

        love.graphics.print("Real: red  Imag: indigo", 20, 220)
    end

    -- mouse cursor
    love.graphics.setColor(1, 0.35, 0.35)
    love.graphics.circle("fill", x, y, 4)
end


