#include <Arduino.h>
#include <HardwareSerial.h>

#include "fre_table.h"
#include "midi.h"
#include "music.h"

#define PLAY_PIN 5

static bool s_enable_note_off;
static float s_play_speed = 1;

static void notePlayer(const NoteEvent& e) {
  auto delayMs = (uint16_t)((float)e.tickMs / s_play_speed);
  if (s_enable_note_off) {
    if (e.isOn) {
      tone(PLAY_PIN, (uint16_t)fre_table[e.key - 21], delayMs);
    } else {
      tone(PLAY_PIN, 0, delayMs);
    }
  } else {
    if (e.isOn) {
      tone(PLAY_PIN, (uint16_t)fre_table[e.key - 21]);
    } else {
      delay(delayMs);
    }
  }
}

void setup() {
  Serial.begin(115200);
}

void loop() {
  s_enable_note_off = true;
  playMusic(SuperMario, sizeof(SuperMario) / sizeof(NoteEvent), notePlayer);
  s_enable_note_off = false;
  playMusic(QuanYeCha, sizeof(QuanYeCha) / sizeof(NoteEvent), notePlayer);
}
