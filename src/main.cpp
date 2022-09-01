#include <Arduino.h>

// Do not build on GCC8, GCC8 has a compiler bug

#if defined(ARDUINO_ARCH_RP2040) || ((__GNUC__ == 8) && (__XTENSA__))
void setup() {}
void loop() {}
#else
#ifdef ESP32
    #include <WiFi.h>
    #include "SPIFFS.h"
#else
    #include <ESP8266WiFi.h>
#endif

#include <AudioOutputNull.h>
#include <AudioOutputI2SNoDAC.h>
#include <AudioGeneratorMIDI.h>
#include <AudioFileSourceSPIFFS.h>

AudioFileSourceSPIFFS *sf2;
AudioFileSourceSPIFFS *mid;
AudioOutputI2SNoDAC *dac;
AudioGeneratorMIDI *midi;

void setup()
{
  const char *soundfont = "/1mgm.sf2";
  const char *midifile = "/quanyecha.mid";

  WiFi.mode(WIFI_OFF);

  Serial.begin(115200);
  Serial.println("Starting up...\n");
    bool ok = SPIFFS.begin();
    if (!ok) {
        Serial.println("SPIFFS.begin() error");
    } else {
        FSInfo info{};
        bool ret = SPIFFS.info(info);
        Serial.printf("FS info : ret: %d, size: %u, size: %u\r\n", ret, info.totalBytes, info.usedBytes);
    }

    {
        Serial.printf("wtf: 1: %d\r\n", SPIFFS.exists(soundfont));
        Serial.printf("wtf: 2: %d\r\n", SPIFFS.exists("/furelise.mid"));
        Serial.printf("wtf: 3: %d\r\n", SPIFFS.exists("/SuperMario.mid"));
        Serial.printf("wtf: 4: %d\r\n", SPIFFS.exists("/quanyecha.mid"));
        Serial.printf("wtf: 5: %d\r\n", SPIFFS.exists("/abc.txt"));
    }

  audioLogger = &Serial;
  sf2 = new AudioFileSourceSPIFFS(soundfont);
  mid = new AudioFileSourceSPIFFS(midifile);

  dac = new AudioOutputI2SNoDAC();
  midi = new AudioGeneratorMIDI();
  midi->SetSoundfont(sf2);
  midi->SetSampleRate(22050);
  Serial.printf("BEGIN...\n");
  midi->begin(mid, dac);
}

void loop()
{
  if (midi->isRunning()) {
    if (!midi->loop()) {
      midi->stop();
    }
  } else {
    Serial.printf("MIDI done\n");
    delay(1000);
    midi->begin(mid, dac);
  }
}

#endif
