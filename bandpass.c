#include <Audio.h>
#include <Wire.h>
#include <SPI.h>

AudioInputI2S inI2S;  
AudioFilterStateVariable bpFilter; // bandpass filter
AudioOutputI2S outI2S;
AudioControlSGTL5000 codec;

// mic is on left channel so use 0 as input
//AudioConnection n1(inI2S, 0, outI2S, 0);
//AudioConnection n2(inI2S, 0, outI2S, 1);

// AudioConnection f1(inI2S, 0, inHPF, 0);
// AudioConnection f2(inHPF, 0, outI2S, 0);
// AudioConnection f3(inHPF, 0, outI2S, 1);

//can also use hpf to help with excess noise

// mic is on left channel so use channel 0
AudioConnection c1(inI2S, 0, bpFilter, 0);
AudioConnection c2(bpFilter, 1, outI2S, 0);   // output = band-pass
AudioConnection c3(bpFilter, 1, outI2S, 1);

void setup() {
  AudioMemory(30);

  codec.enable();
  codec.inputSelect(AUDIO_INPUT_MIC);
  codec.micGain(30);
  // change to alter gain in dB
  codec.lineOutLevel(29);
  // line level input of 1 vrms, can increase or decrease based on clipping
  // decrease if clipping
  codec.volume(0);
  
  // Bandpass settings 
  float centerFreq= 440.0f; // frequency of interest
  float Q = 2.0f //large is narrow, small is wide

  bpFilter.frequency(centerFreq);
  bpFilter.resonance(Q); // q factor 
}

void loop() {
}