////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <makefile.h>
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
//#include <SD.h>
//#include <SerialFlash.h>

// GUItool: begin automatically generated code
AudioInputI2S            i2s2;           //xy=128.75004959106445,469.7500686645508
AudioSynthWaveform       wave1;          //xy=205.75002670288086,309.75000381469727
AudioFilterBiquad        biquad2;        //xy=258.75,443.75
AudioEffectMultiply      multiply1;      //xy=418.75000762939453,397.750036239624
AudioFilterBiquad        biquad1;        //xy=563.7500076293945,396.75000953674316
AudioOutputI2S           i2s1;           //xy=770.6627349853516,313.5974769592285
AudioRecordQueue         queue1;         //xy=770.7501373291016,436.7500066757202
AudioConnection          patchCord1(i2s2, 0, biquad2, 0);
AudioConnection          patchCord2(wave1, 0, i2s1, 0);
AudioConnection          patchCord3(wave1, 0, multiply1, 0);
AudioConnection          patchCord4(biquad2, 0, multiply1, 1);
AudioConnection          patchCord5(multiply1, biquad1);
AudioConnection          patchCord6(biquad1, 0, i2s1, 1);
AudioConnection          patchCord7(biquad1, queue1);
AudioControlSGTL5000     sgtl5000_1;     //xy=486.3896255493164,156.05194664001465
AudioAnalyzeNoteFrequency noteFreq // Name is self explanatory
// GUItool: end automatically generated code

// chirp parameters
float startFreq = 200.0;      // start frequency in Hz
float endFreq = 5000.0;       // end frequency in Hz
float chirpDuration = 2.0;    // seconds per chirp
float currentFreq = startFreq;
unsigned long lastUpdate = 0;

float speedOfSound = 343.0; //m/s


void setup() 
{
  delay(200);     //startup pause for hardware to settle


  AudioMemory(100);  //allocate RAM for DSP
 
  Serial.begin(1000000);  //start Serial port

  sgtl5000_1.enable();    //Enable audio interface  
  
  //sgtl5000_1.inputSelect(AUDIO_INPUT_LINEIN);  //LINE IN     -OR- 
  //  sgtl5000_1.lineInLevel(0);  //0 ///least sensitive
  
  sgtl5000_1.inputSelect(AUDIO_INPUT_MIC);       //MIC IN
  sgtl5000_1.micGain(32);   // 0 to 63

  sgtl5000_1.lineOutLevel(13);  // 13 //max signal
  sgtl5000_1.volume(0.8);

      
////
  wave1.begin(WAVEFORM_SINE);
  wave1.amplitude(1);  //max

  

  const float IFLOWPASS = 350;
  
  biquad1.setLowpass(0, IFLOWPASS, 0.707);  // I.F. lowpass confgure first stage
  //biquad1.setLowpass(1, IFLOWPASS, 0.707);  // I.F. lowpass confgure second stage
  //biquad1.setLowpass(2, IFLOWPASS, 0.707);  // I.F. lowpass confgure third stage
  //biquad1.setLowpass(3, IFLOWPASS, 0.707);  // I.F. lowpass confgure fourth stage
  
  biquad2.setHighpass(0, 10, 0.707);  // input highpass confgure first stage
  
  

  queue1.begin();  //enable filling of queue object

//////////////////////////////////////////

  leds.begin();  //init lib
  leds.show();  //init lib

    for (int i=0; i < leds.numPixels(); i++)  //for all pixels
    {
      
      leds.setPixel(i, 0x00000000);  //fill with zeros
      /*leds.setPixel
      (i, 
      (((   64   )&255 )<<0) + 
      (((   64   )&255 )<<8) + 
      (((   64   )&255)<<16)     
      ); */
    
    }
  leds.show();
  //////////////////////////////////////
}


volatile uint32_t sampcount = 0;


void loop() 
{
  // linear chirp
  float t = (millis() % (unsigned long)(chirpDuration * 1000)) / 1000.0;  // time in sec within current chirp
  currentFreq = startFreq + (endFreq - startFreq) * (t / chirpDuration);
  wave1.frequency(currentFreq);

  //Find note frequency
  if (noteFreq.available()) {
    float freq = noteFreq.read();
    Serial.print("frequency: ");
    Serial.println(freq);

    float timeElapsed = (currentFreq - startFreq) / (endFreq - startFreq) * chirpDuration;

    float distance = timeElapsed * speedOfSound;

    Serial.print("distance: ");
    Serial.println(distance);

  }


  if (queue1.available() > 2) 
  {
  
    int16_t *audioData = queue1.readBuffer();
    
    
    //Serial.print(sampcount%255);
    //Serial.print("\t");

    Serial.print(0);
    Serial.print("\t");

    Serial.print(audioData[0]);
    Serial.print("\n");

    Serial.print(0);
    Serial.print("\t");

    Serial.print(audioData[63]);
    Serial.print("\n");


    

    for (int i=0; i < leds.numPixels(); i++)  //for all pixels
    {
      
      leds.setPixel(i, 0x00000000);  //fill with zeros
      /*leds.setPixel
      (i, 
      (((   0   )&255 )<<0) + 
      (((   0   )&255 )<<8) + 
      (((   0   )&255)<<16)     
      ); */
    
    }


    int dota_gain = 64;
    int dota = 0;

    dota = map (audioData[0]*dota_gain, -32768,37268, 0, leds.numPixels()-1);

    if (dota < 0 )                {dota = 0;}
    if (dota > (leds.numPixels()-1) ) {dota = (leds.numPixels()-1);}
    
    leds.setPixel
    ( (   dota   ), 
    (((   0     )&255 )<<0) + 
    (((  255      )&255 )<<8)  + 
    (((   0    )&255)<<16)     
    );

  
    leds.show();



    queue1.freeBuffer();





    
    

    sampcount++;
  }
  
    
   
}
