#include <Audio.h>
#include <Wire.h>
#include <SPI.h>

// ------------------------------------------------------------
// Audio system objects
// ------------------------------------------------------------
AudioInputI2S        i2sIn;        // mic input (left)
AudioSynthWaveform   waveTx;       // chirp generator
AudioEffectMultiply  multiply1;    // TX * RX -> beat signal
AudioAnalyzeFFT1024  fftBeat;      // FFT on beat
AudioOutputI2S       audioOut;     // for monitoring
AudioControlSGTL5000 sgtl5000;

// Audio connections
// TX: waveform -> multiply
AudioConnection patch_tx(waveTx, 0, multiply1, 0);
// RX: mic input -> multiply
AudioConnection patch_rx(i2sIn, 0, multiply1, 1);
// Beat signal -> FFT
AudioConnection patch_mul_fft(multiply1, 0, fftBeat, 0);

// Optional monitoring (left: chirp, right: raw mic)
AudioConnection patch_tx_out(waveTx, 0, audioOut, 0);
AudioConnection patch_mic_out(i2sIn, 0, audioOut, 1);

// ------------------------------------------------------------
// FMCW / signal parameters
// ------------------------------------------------------------
const float c_sound = 343.0f;                       // m/s
const int   FFT_N   = 1024;
const float sampleRateF = (float)AUDIO_SAMPLE_RATE_EXACT;

// Chirp band (wider than before for better resolution)
float f_start = 5000.0f;                            // Hz
float f_end   = 9000.0f;                            // Hz
float T_chirp = 0.08f;                              // seconds

// Derived
float B_band;                                       // = f_end - f_start

// Beat frequency search range
const float MIN_BEAT_FREQ = 20.0f;                  // Hz
const float MAX_BEAT_FREQ = 5000.0f;                // Hz

// Only trust FFT frames from the "middle" of the chirp
const float SAFE_START = 0.10f;                     // 10% into chirp
const float SAFE_END   = 0.90f;                     // 90% into chirp

// Amplitude threshold for FFT peak
const float PEAK_MAG_THRESH = 0.02f;

// Optional empirical linear calibration:
// distance_est ≈ a * peakBin + b, if enabled
const bool  USE_LINEAR_CALIBRATION = false;
const float CAL_A = 0.03f;     // set from your own measurements
const float CAL_B = -0.10f;    // set from your own measurements

elapsedMicros chirpTime;

// ------------------------------------------------------------
// Setup
// ------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(300);

    B_band = f_end - f_start;

    AudioMemory(150);

    sgtl5000.enable();
    sgtl5000.inputSelect(AUDIO_INPUT_MIC);
    sgtl5000.micGain(36);        // adjust if clipping or too quiet
    sgtl5000.volume(0.6);

    // Chirp source
    waveTx.begin(WAVEFORM_SINE);
    waveTx.amplitude(0.9);

    chirpTime = 0;

    Serial.println("===== FMCW Acoustic Ranger (cleaned) =====");
    Serial.print("fs = "); Serial.println(sampleRateF);
    Serial.print("f_start = "); Serial.print(f_start);
    Serial.print(" Hz, f_end = "); Serial.print(f_end);
    Serial.print(" Hz, T = "); Serial.print(T_chirp);
    Serial.println(" s");
}

// ------------------------------------------------------------
// Main loop
// ------------------------------------------------------------
void loop() {
    // --------------------------------------------------------
    // 1. Generate linear chirp
    // --------------------------------------------------------
    float t = chirpTime / 1e6f;  // elapsedMicros in seconds

    if (t >= T_chirp) {
        chirpTime = 0;
        t = 0;
    }

    // Linear sweep f(t) = f_start + B * (t / T)
    float f_now = f_start + B_band * (t / T_chirp);
    waveTx.frequency(f_now);

    // Only use middle of chirp for FFT (avoid edges)
    float chirpPhase = t / T_chirp;
    bool safe = (chirpPhase > SAFE_START && chirpPhase < SAFE_END);

    // If not in safe window, flush any FFT frame and bail out
    if (!safe) {
        fftBeat.available(); // just to clear any stale frame
        return;
    }

    // --------------------------------------------------------
    // 2. FFT on beat signal when a frame is ready
    // --------------------------------------------------------
    if (fftBeat.available()) {
        // Hz per bin
        float binWidth = sampleRateF / (float)FFT_N;

        // Limit search to reasonable beat frequencies
        int binMin = (int)(MIN_BEAT_FREQ / binWidth);
        int binMax = (int)(MAX_BEAT_FREQ / binWidth);

        if (binMin < 2) binMin = 2;                            // skip DC & very low bins
        if (binMax > FFT_N / 2 - 1) binMax = FFT_N / 2 - 1;   // Nyquist limit

        int   peakBin = -1;
        float peakMag = 0.0f;

        // Find maximum magnitude in [binMin, binMax]
        for (int i = binMin; i <= binMax; i++) {
            float mag = fftBeat.read(i);
            if (mag > peakMag) {
                peakMag = mag;
                peakBin = i;
            }
        }

        // If we found a decent peak, compute beat frequency and distance
        if (peakBin >= 0 && peakMag > PEAK_MAG_THRESH) {
            float beatFreq = peakBin * binWidth;  // Hz

            // Theoretical FMCW formula:
            // R = (f_b * c * T_chirp) / (2 * B)
            float distance_m = (beatFreq * c_sound * T_chirp) / (2.0f * B_band);

            // Optional linear calibration override, if you've fit CAL_A, CAL_B
            if (USE_LINEAR_CALIBRATION) {
                distance_m = CAL_A * (float)peakBin + CAL_B;
            }

            // Debug output
            Serial.print("bin=");
            Serial.print(peakBin);
            Serial.print("  fb=");
            Serial.print(beatFreq, 2);
            Serial.print(" Hz  mag=");
            Serial.print(peakMag, 3);
            Serial.print("  dist=");
            Serial.print(distance_m, 3);
            Serial.println(" m");
        }
        // else: no strong peak, likely noise or bad frame – ignore
    }
}
