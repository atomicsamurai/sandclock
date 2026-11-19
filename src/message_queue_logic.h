#pragma once

// Messages waiting to scroll. /showmessage adds to the back, so messages
// that arrive while one is scrolling (two Home Assistant notifications in a
// row) each get their turn instead of replacing each other. System
// announcements (diagnostics) go to the front. loop() takes the next one
// whenever no scroll is running. Free of Arduino types so tests/ can compile
// it on a host.

#include <stdint.h>
#include <string.h>

constexpr uint8_t MESSAGE_QUEUE_SIZE = 4;
constexpr size_t MESSAGE_TEXT_SIZE = 128;  // 127 bytes + NUL, as messageText

struct QueuedMessage {
  char text[MESSAGE_TEXT_SIZE];
  uint16_t delayMs;
  uint16_t repeat;
  bool quiet;  // no start/end beeps (system announcements)
};

struct MessageQueue {
  QueuedMessage items[MESSAGE_QUEUE_SIZE];
  uint8_t head = 0;   // index of the next message to show
  uint8_t count = 0;

  bool empty() const { return count == 0; }
  bool full() const { return count == MESSAGE_QUEUE_SIZE; }

  static void fill(QueuedMessage &m, const char *text, uint16_t delayMs, uint16_t repeat, bool quiet) {
    strncpy(m.text, text ? text : "", MESSAGE_TEXT_SIZE - 1);
    m.text[MESSAGE_TEXT_SIZE - 1] = 0;  // longer text is cut, as before
    m.delayMs = delayMs;
    m.repeat = repeat > 0 ? repeat : 1;
    m.quiet = quiet;
  }

  // Adds to the back; false (and nothing changes) when full.
  bool push(const char *text, uint16_t delayMs, uint16_t repeat, bool quiet = false) {
    if (full()) {
      return false;
    }
    fill(items[(head + count) % MESSAGE_QUEUE_SIZE], text, delayMs, repeat, quiet);
    count++;
    return true;
  }

  // Adds to the front, to be shown next. When full, the last message in the
  // queue is dropped to make room: an announcement someone asked for right
  // now matters more than the oldest-queued notification's tail.
  void pushFront(const char *text, uint16_t delayMs, uint16_t repeat, bool quiet = false) {
    if (full()) {
      count--;
    }
    head = (head + MESSAGE_QUEUE_SIZE - 1) % MESSAGE_QUEUE_SIZE;
    fill(items[head], text, delayMs, repeat, quiet);
    count++;
  }

  // Takes the next message; false when there is none.
  bool pop(QueuedMessage &out) {
    if (empty()) {
      return false;
    }
    out = items[head];
    head = (head + 1) % MESSAGE_QUEUE_SIZE;
    count--;
    return true;
  }
};
