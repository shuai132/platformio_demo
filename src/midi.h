#pragma once

#include <cstdint>
#include <functional>

struct NoteEvent {
  NoteEvent() = default;
  NoteEvent(uint8_t track, bool isOn, uint8_t key, uint16_t tickMs) : track(track), isOn(isOn), key(key), tickMs(tickMs) {}
  struct {
    bool isOn : 1;
    uint8_t track : 7;
  };
  uint8_t key = 0;
  uint16_t tickMs = 0;
};

using NoteHandle = std::function<void(NoteEvent)>;

inline void playMusic(const NoteEvent* music, uint16_t len, const NoteHandle& handle) {
  for (int i = 0; i < len; ++i) {
    handle(music[i]);
  }
}
