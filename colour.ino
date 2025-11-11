////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <OctoWS2811.h>

const int numPins = 1;
byte pinList[numPins] = {2};
const int ledsPerStrip = 24;

DMAMEM int displayMemory[ledsPerStrip * numPins * 3 / 4];
int drawingMemory[ledsPerStrip * numPins * 3 / 4];

const int config = WS2811_GRB | WS2811_800kHz;
OctoWS2811 leds(ledsPerStrip, displayMemory, drawingMemory, config, numPins, pinList);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>

// GUItool: begin automatically generated code
AudioInputI2S            i2s2;           
AudioSynthWaveformSine   sine1;          
AudioFilterBiquad        biquad2;        
AudioEffectMultiply      multiply1;      
AudioFilterBiquad        biquad1;        
AudioAmplifier           amp1;           // amplifier doubles amplitude
AudioOutputI2S           i2s1;           
AudioRecordQueue         queue1;         
AudioConnection          patchCord1(i2s2, 0, biquad2, 0);
AudioConnection          patchCord2(sine1, 0, i2s1, 0);
AudioConnection          patchCord3(sine1, 0, multiply1, 0);
AudioConnection          patchCord4(biquad2, 0, multiply1, 1);
AudioConnection          patchCord5(multiply1, biquad1);
AudioConnection          patchCord6(biquad1, amp1);
AudioConnection          patchCord7(amp1, 0, i2s1, 1);
AudioConnection          patchCord8(amp1, queue1);
AudioControlSGTL5000     sgtl5000_1;     
// GUItool: end automatically generated code


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void setup() 
{
  delay(200);     
  AudioMemory(100);  
  Serial.begin(1000000);

  sgtl5000_1.enable();    
  sgtl5000_1.inputSelect(AUDIO_INPUT_MIC);
  sgtl5000_1.micGain(8);
  sgtl5000_1.lineOutLevel(13);
  sgtl5000_1.volume(0.8);

  sine1.amplitude(1);
  sine1.frequency(3520);

  const float IFLOWPASS = 150;
  biquad1.setLowpass(0, IFLOWPASS, 0.707);
  biquad2.setHighpass(0, 10, 0.707);

  amp1.gain(1.5);   // double amplitude

  queue1.begin();

  leds.begin();
  leds.show();

  for (int i=0; i < leds.numPixels(); i++) {
    leds.setPixel(i, 0x00000000);
  }
  leds.show();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Convert hue (0–360) to RGB (0–255)
uint32_t hueToRGB(float hue) {
  float r, g, b;
  float c = 1.0f;
  float x = 1.0f - fabs(fmod(hue / 60.0f, 2.0f) - 1.0f);
  float m = 0.0f;

  if (hue < 60)       { r = c; g = x; b = 0; }
  else if (hue < 120) { r = x; g = c; b = 0; }
  else if (hue < 180) { r = 0; g = c; b = x; }
  else if (hue < 240) { r = 0; g = x; b = c; }
  else if (hue < 300) { r = x; g = 0; b = c; }
  else                { r = c; g = 0; b = x; }

  uint8_t R = (uint8_t)((r + m) * 255);
  uint8_t G = (uint8_t)((g + m) * 255);
  uint8_t B = (uint8_t)((b + m) * 255);

  return (R << 16) | (G << 8) | B;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

volatile uint32_t sampcount = 0;
elapsedMillis colorTimer = 0;
float hue = 0.0f;

void loop() 
{
  // update hue every millisecond
  if (colorTimer >= 1) {
    hue += 1.0f;
    if (hue >= 360.0f) hue -= 360.0f;
    colorTimer = 0;
  }

  if (queue1.available() > 2) 
  {
    int16_t *audioData = queue1.readBuffer();

    Serial.print(0);
    Serial.print("\t");
    Serial.print(audioData[0]);
    Serial.print("\n");

    Serial.print(0);
    Serial.print("\t");
    Serial.print(audioData[63]);
    Serial.print("\n");

    // clear LEDs
    for (int i=0; i < leds.numPixels(); i++) {
      leds.setPixel(i, 0x00000000);
    }

    int dota_gain = 64;
    int dota = map(audioData[0] * dota_gain, -32768, 32768, 0, leds.numPixels() - 1);
    dota = constrain(dota, 0, leds.numPixels() - 1);

    uint32_t color = hueToRGB(hue);

    leds.setPixel(dota, color);
    leds.show();

    queue1.freeBuffer();
    sampcount++;
  }
}
