/*
  Teensy FMCW acoustic ranger (minimal, headphone output)
  - TX: linear chirp (wave1)
  - RX: on-board microphone / line-in (AudioInputI2S)
  - multiply TX * RX -> lowpass -> FFT (1024)
  - find peak FFT bin -> compute beat freq -> distance (meters)
  Notes:
    - Uses AudioAnalyzeFFT1024 from Teensy Audio library.
    - Tune chirp parameters (startFreq, endFreq, chirpDuration) for your environment.
    - This is a simple single-frame FFT peak detector; for noisy envs average multiple frames.
    - Requires AudioAdapter, mic or line-in with sufficient SNR.
*/

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>

// ---------- Audio objects ----------
AudioInputI2S            i2sIn;             // microphone / line in (left or right)
AudioSynthWaveform       waveTx;            // transmit chirp
AudioEffectMultiply      multiply1;         // multiply TX * RX to get sum/diff
AudioFilterBiquad        lp;                // lowpass to isolate beat freq
AudioAnalyzeFFT1024      fft1024;           // FFT to find beat frequency
AudioOutputI2S           audioOut;          // headphones (stereo)
AudioControlSGTL5000     sgtl5000;

// ---------- Patch cords ----------
// i2sIn (left) -> multiply1 input B
AudioConnection          patch_in(i2sIn, 0, multiply1, 1);
// waveTx -> multiply1 input A
AudioConnection          patch_tx(waveTx, 0, multiply1, 0);
// multiply -> lowpass -> FFT
AudioConnection          patch_mul_lp(multiply1, lp);
AudioConnection          patch_lp_fft(lp, fft1024);
// Also route TX to LEFT headphone so you can hear chirp (optional)
AudioConnection          patch_tx_out(waveTx, 0, audioOut, 0);
// route bandpassed beat (or debug tone) to RIGHT headphone (optional)
// We'll route lowpass output to right out via a record queue for debug if desired.
// For now, use the multiply -> lp signal routed to audioOut right for monitoring:
AudioConnection          patch_lp_out(lp, 0, audioOut, 1);

// ---------- FMCW params ----------
const float c_sound = 343.0f;    // speed of sound m/s (adjust to temp)
float f_start = 1000.0f;         // chirp start Hz (tune)
float f_end = 6000.0f;           // chirp end Hz (tune)
float T_chirp = 0.02f;           // chirp duration seconds (20 ms); tune to avoid overlap/ambiguity
// Note: shorter T -> larger beat freq per unit distance; must fit in FFT bin range

// Derived
float B_band = 0.0f;             // bandwidth Hz
float sampleRateF = (float)AUDIO_SAMPLE_RATE_EXACT; // typical 44117.x (use constant)
const int FFT_N = 1024;          // FFT size of AudioAnalyzeFFT1024

// FFT & peak detection
const float MIN_BEEP_FREQ = 10.0f;   // ignore near-DC
const float MAX_BEEP_FREQ = 3000.0f; // limit search to reasonable beat band (tune)

// runtime
unsigned long chirpStartMillis = 0;

void setup() {
  Serial.begin(115200);
  delay(100);

  B_band = f_end - f_start;
  Serial.println("FMCW acoustic ranger starting...");
  Serial.print("Audio sample rate: "); Serial.println(sampleRateF, 2);
  Serial.print("Chirp: "); Serial.print(f_start); Serial.print(" -> "); Serial.print(f_end);
  Serial.print(" Hz, T = "); Serial.print(T_chirp, 4); Serial.println(" s");

  AudioMemory(120); // FFT1024 prefers a fair bit of memory; increase if possible

  // Configure audio codec (headphones)
  sgtl5000.enable();
  sgtl5000.inputSelect(AUDIO_INPUT_MIC);
  sgtl5000.micGain(36);       // adjust to avoid clipping
  sgtl5000.volume(0.55);     // headphone output volume
  sgtl5000.lineOutLevel(0);  // disable external line out to favor headphone

  // Configure waveTx
  waveTx.begin(WAVEFORM_SINE);
  waveTx.amplitude(0.9); // near max

  // Configure LP filter: very-low cutoff: keep beat frequencies (tunable)
  // Set an initial lowpass cutoff to e.g. 2000 Hz; Q not critical
  lp.setLowpass(0, 3000.0f, 0.707f);

  // start chirp timing
  chirpStartMillis = millis();
}

void loop() {
  // ===== generate continuous linear chirp (repeats every T_chirp) =====
  // We produce a chirp that restarts every T_chirp seconds.
  // time within chirp [0..T_chirp)
  unsigned long now = millis();
  float t = ( (now - chirpStartMillis) % (unsigned long)(T_chirp * 1000.0f) ) / 1000.0f;
  // linear frequency interpolation
  float f_now = f_start + (B_band) * (t / T_chirp);
  waveTx.frequency(f_now);

  // ===== FFT available? analyze =====
  if (fft1024.available()) {
    // find the largest magnitude bin in a frequency band of interest
    // AudioAnalyzeFFT1024::read(i) returns magnitude (0..1 roughly)
    int peakBin = -1;
    float peakMag = 0.0f;

    // compute bin width: sampleRate / FFT_N
    float binWidth = sampleRateF / (float)FFT_N;

    // search bins corresponding to MIN_BEEP_FREQ .. MAX_BEEP_FREQ
    int binMin = max(2, (int)floor(MIN_BEEP_FREQ / binWidth)); // skip DC bins
    int binMax = min(FFT_N/2 - 1, (int)ceil(MAX_BEEP_FREQ / binWidth));

    for (int i = binMin; i <= binMax; ++i) {
      float mag = fft1024.read(i); // magnitude for bin i
      if (mag > peakMag) {
        peakMag = mag;
        peakBin = i;
      }
    }

    if (peakBin >= 0 && peakMag > 0.01f) { // threshold to ignore noise (tune)
      float beatFreq = peakBin * binWidth;
      // compute distance from beat frequency:
      // d = (fb * c * T) / (2 * B)
      float distance_m = (beatFreq * c_sound * T_chirp) / (2.0f * B_band);

      Serial.print("peakBin: "); Serial.print(peakBin);
      Serial.print("  mag: "); Serial.print(peakMag, 4);
      Serial.print("  fb: "); Serial.print(beatFreq, 2);
      Serial.print(" Hz  dist: "); Serial.print(distance_m, 4);
      Serial.println(" m");
    } else {
      // no clear peak
      Serial.println("no peak");
    }
  }

  // small yield
  delay(5);
}
