#pragma once

#include <stdint.h>

// Detects a deliberate gesture on the light sensor, in either direction:
// - cover it with a hand three times (for a lit room), or
// - flash a light at it three times, e.g. a phone flashlight (for a dark room).
// Each pulse and each gap between pulses must be short
// (GESTURE_MIN_MS..GESTURE_MAX_MS), and all pulses in one sequence must be the
// same kind. Free of Arduino types so tests/ can compile it on a host;
// main.cpp feeds it analogRead(A0) every GESTURE_SAMPLE_MS and shows the
// diagnostics when update() returns true. Readings are 0-1023, higher is
// brighter.
//
// Why this does not fire by accident:
// - pulses are measured against the recent light level (a big, sudden
//   change), so dim and bright rooms both work and slow changes never count;
// - lights switching off or on hold the new level far longer than
//   GESTURE_MAX_MS, which cancels the sequence and re-learns the level;
// - flickers and the individual strokes of a lightning flash are shorter
//   than GESTURE_MIN_MS;
// - three short pulses with short gaps, all within about 5 s, is not what
//   people walking past, headlights or a TV produce.
// Covers need some light to block (level at least COVER_MIN_BASELINE).
// Flashes need the reading to double, which in practice means a dim room.

constexpr uint32_t GESTURE_SAMPLE_MS = 50;      // 20 Hz: cheap, and plenty for a hand
constexpr uint8_t GESTURE_COUNT = 3;
constexpr uint32_t GESTURE_MIN_MS = 150;        // shorter is noise or lightning
constexpr uint32_t GESTURE_MAX_MS = 1500;       // longer is a light switch / someone leaning
constexpr uint32_t GESTURE_COOLDOWN_MS = 10000; // after a gesture, ignore the sensor
constexpr int COVER_MIN_BASELINE = 60;          // darker than this, covers are off
constexpr int COVER_MIN_DROP = 40;              // a cover is below half AND this much lower
constexpr int FLASH_MIN_RISE = 100;             // a flash is double AND this much higher

struct LightGesture {
  enum Pulse : uint8_t { NONE, COVER, FLASH };
  Pulse pulse = NONE;         // the pulse in progress, NONE between pulses
  Pulse kind = NONE;          // the kind of the current sequence
  int baseline = -1;          // recent light level, learned only between pulses
  uint8_t count = 0;          // completed pulses in the current sequence
  uint32_t edgeMs = 0;        // when the current pulse or gap started
  uint32_t cooldownUntil = 0;
  bool cooling = false;

  Pulse startsPulse(int v) const {
    if (baseline >= COVER_MIN_BASELINE && v * 2 < baseline && baseline - v >= COVER_MIN_DROP) {
      return COVER;
    }
    if (v >= baseline * 2 && v - baseline >= FLASH_MIN_RISE) {
      return FLASH;
    }
    return NONE;
  }
  // Back to normal, with hysteresis: above 3/4 of the level after a cover,
  // below 3/2 of it after a flash.
  bool endsPulse(int v) const {
    return pulse == COVER ? v * 4 >= baseline * 3 : v * 2 <= baseline * 3;
  }

  void follow(int v) { baseline += (v - baseline) / 8; }

  // Feed one reading; returns true once, when a gesture completes.
  bool update(uint32_t now, int v) {
    if (baseline < 0) {
      baseline = v;
      edgeMs = now;
      return false;
    }
    if (cooling && (int32_t)(now - cooldownUntil) < 0) {
      follow(v);
      return false;
    }
    cooling = false;

    uint32_t phase = now - edgeMs;
    if (pulse == NONE) {
      Pulse p = startsPulse(v);
      if (p != NONE) {
        // A wrong-length gap, or a different kind of pulse, starts over;
        // this pulse then counts as the first of a new sequence.
        if (count > 0 && (p != kind || phase < GESTURE_MIN_MS || phase > GESTURE_MAX_MS)) {
          count = 0;
        }
        pulse = p;
        edgeMs = now;
        return false;
      }
      follow(v);  // track the room between pulses
      if (count > 0 && phase > GESTURE_MAX_MS) {
        count = 0;  // waited too long for the next pulse
      }
      return false;
    }

    if (phase > GESTURE_MAX_MS) {
      // Not a hand or a flashlight: the lights changed (or something sits in
      // front). Adopt the new level and start over.
      count = 0;
      pulse = NONE;
      baseline = v;
      edgeMs = now;
      return false;
    }
    if (!endsPulse(v)) {
      return false;
    }
    Pulse ended = pulse;
    pulse = NONE;
    edgeMs = now;
    if (phase < GESTURE_MIN_MS) {
      count = 0;  // a flicker, not a pulse
      return false;
    }
    kind = ended;
    if (++count < GESTURE_COUNT) {
      return false;
    }
    count = 0;
    cooling = true;
    cooldownUntil = now + GESTURE_COOLDOWN_MS;
    return true;
  }
};
