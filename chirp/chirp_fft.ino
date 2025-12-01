#include <Audio.h>
#include <Wire.h>
#include <SPI.h>

// -----------------------------
// FMCW CHIRP SETTINGS
// -----------------------------
const float f_start = 5000.0f;   // Hz
const float f_end   = 9000.0f;   // Hz
const float T_chirp = 0.15;     // seconds
const float amplitude = 0.7f;    // sweep amplitude

const float speed_sound = 343.0f; // m/s
const float K = speed_sound * T_chirp / (2.0f * (f_end - f_start));

// -----------------------------
// AUDIO OBJECTS
// -----------------------------
AudioSynthToneSweep chirp;
AudioInputI2S       mic;
AudioAnalyzeFFT1024 fft;
AudioOutputI2S      outI2S;

AudioConnection patchCord1(chirp, 0, outI2S, 0);  // chirp to left
AudioConnection patchCord2(mic,   0, fft,   0);   // mic left channel -> FFT

AudioControlSGTL5000 audioShield;


// record recieving data from mic b4 fft 
// record output wave 
// -----------------------------
// Moving average buffer (declare BEFORE loop)
// -----------------------------
const int AVG_SIZE = 10;
float avgBuffer[AVG_SIZE];
int avgIndex = 0;
bool avgFilled = false;

float addToAverage(float x) {
  avgBuffer[avgIndex] = x;
  avgIndex = (avgIndex + 1) % AVG_SIZE;

  if (avgIndex == 0) avgFilled = true;

  float sum = 0.0f;
  int count = avgFilled ? AVG_SIZE : avgIndex;
  if (count == 0) return x; // first sample guard

  for (int i = 0; i < count; i++) sum += avgBuffer[i];
  return sum / (float)count;
}
// -----------------------------
// FFT PROCESSING CONSTANTS
// -----------------------------
const float NOISE_THRESHOLD = 0.005f; 
const float MIN_DISTANCE_CM = 5.0f;
const float MAX_DISTANCE_CM = 150.0f;
// -----------------------------
// SETUP
// -----------------------------
void setup() {
  Serial.begin(115200);
  delay(200);

  AudioMemory(40);

  audioShield.enable();
  audioShield.inputSelect(AUDIO_INPUT_MIC);
  audioShield.micGain(36);   // adjust as needed for clipping / SNR
  audioShield.volume(0.5);

  // Correct ToneSweep signature:
  // play(amplitude, freqStart, freqEnd, sweepTime)
  chirp.play(amplitude, (int)f_start, (int)f_end, T_chirp);

  Serial.println("=== FMCW continuous ranging started ===");
}

// -----------------------------
// LOOP
// -----------------------------
// -----------------------------
// LOOP (MODIFIED)
// -----------------------------
void loop() {

    // Keep chirp continuous
    if (!chirp.isPlaying()) {
        chirp.play(amplitude, (int)f_start, (int)f_end, T_chirp);
    }

    if (fft.available()) {
        // Calculate the bin width
        float binWidth = AUDIO_SAMPLE_RATE_EXACT / 1024.0f; // sampleRate / N

        // --- 1. Define Search Window in Bins ---
        // Formula: f_beat = distance_m / K
        // f_min = MIN_DISTANCE_CM / 100 / K
        // f_max = MAX_DISTANCE_CM / 100 / K

        float K_meters = K; // K is calculated in meters/Hz at the top

        int min_freq_hz = (int)((MIN_DISTANCE_CM / 100.0f) / K_meters);
        int max_freq_hz = (int)((MAX_DISTANCE_CM / 100.0f) / K_meters);

        // Convert frequencies (Hz) to FFT bins
        int startBin = (int)(min_freq_hz / binWidth);
        int endBin = (int)(max_freq_hz / binWidth);

        // Safety check for search bounds
        startBin = max(2, startBin); // Ensure we skip DC/low-freq noise (bin 0, 1)
        endBin= min(511, endBin); // Ensure we don't exceed the N/2 limit of 511

        // -------- 2. Find FFT peak with Threshold and Window --------
        float maxVal = 0.0f;
        int maxBin = 0;

        // Search only within the calculated window
        for (int i = startBin; i <= endBin; i++) {
            float v = fft.read(i);
            if (v > maxVal) {
                maxVal = v;
                maxBin = i;
            }
        }

        // -------- 3. Apply Noise Threshold Filter --------
        if (maxVal < NOISE_THRESHOLD) {
            // No reliable peak found, skip this measurement
            Serial.println("Raw: Peak below threshold. Smoothed: --");
            // Optional: Skip the moving average update to avoid corrupting the average
            // return;
        } else {
            // Peak is valid, proceed with calculations
            float peakFreq = maxBin * binWidth;

            // Convert beat frequency -> distance (centimeters)
            // distance_cm = K * peakFreq * 100
            float distance_cm = K_meters * peakFreq * 100.0f;

            // ---- 10-point moving average ----
            float distance_avg = addToAverage(distance_cm);

            // Serial output: raw + smoothed
            Serial.print("Raw: ");
            Serial.print(distance_cm, 3);
            Serial.print(" cm   Smoothed: ");
            Serial.print(distance_avg, 3);
            Serial.println(" cm");
        }
    }

    // small delay to avoid flooding serial (optional)
    delay(1);
}