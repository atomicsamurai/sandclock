// Host test for src/message_queue_logic.h (the same header the firmware uses).
// Built and run by tests/run.sh.
#include <cstdio>
#include <cstring>
#include <string>

#include "../src/message_queue_logic.h"

static int failures = 0;

static void expect(const char *name, bool pass) {
  printf("  %-46s %s\n", name, pass ? "pass" : "FAIL");
  failures += !pass;
}

// Pops everything and joins the texts with '|'.
static std::string drain(MessageQueue &q) {
  std::string out;
  QueuedMessage m;
  while (q.pop(m)) out += (out.empty() ? "" : "|") + std::string(m.text);
  return out;
}

int main() {
  {
    MessageQueue q;
    QueuedMessage m;
    expect("starts empty", q.empty() && !q.pop(m));
  }
  {
    MessageQueue q;
    q.push("A", 50, 1);
    q.push("B", 60, 2);
    expect("first in, first out", drain(q) == "A|B");
  }
  {
    MessageQueue q;
    for (int i = 0; i < MESSAGE_QUEUE_SIZE; i++) q.push("X", 50, 1);
    expect("full after 4", q.full());
    expect("a 5th push is refused", !q.push("Y", 50, 1) && q.count == 4);
  }
  {
    MessageQueue q;  // wrap the ring around several times, never full
    std::string want, got;
    QueuedMessage m;
    for (int i = 0; i < 20; i++) {
      char t[4];
      snprintf(t, sizeof t, "%d,", i);
      want += t;
      q.push(t, 50, 1);
      if (q.count == 3) {
        q.pop(m);
        got += m.text;
      }
    }
    while (q.pop(m)) got += m.text;
    expect("order kept across wrap-around", got == want);
  }
  {
    MessageQueue q;
    q.push("A", 50, 1);
    q.push("B", 50, 1);
    q.pushFront("DIAG", 120, 1, true);
    QueuedMessage m;
    q.pop(m);
    expect("pushFront goes next", strcmp(m.text, "DIAG") == 0 && m.quiet && m.delayMs == 120);
    expect("then the others in order", drain(q) == "A|B");
  }
  {
    MessageQueue q;
    q.push("A", 50, 1);
    q.push("B", 50, 1);
    q.push("C", 50, 1);
    q.push("D", 50, 1);
    q.pushFront("DIAG", 120, 1);
    expect("pushFront when full drops the last", drain(q) == "DIAG|A|B|C");
  }
  {
    MessageQueue q;
    std::string longText(300, 'L');
    q.push(longText.c_str(), 50, 1);
    QueuedMessage m;
    q.pop(m);
    expect("long text cut to 127 bytes", strlen(m.text) == 127);
  }
  {
    MessageQueue q;
    q.push("A", 70, 0);
    QueuedMessage m;
    q.pop(m);
    expect("repeat 0 becomes 1, delay kept", m.repeat == 1 && m.delayMs == 70 && !m.quiet);
  }
  {
    MessageQueue q;
    q.push(nullptr, 50, 1);
    QueuedMessage m;
    q.pop(m);
    expect("null text becomes empty", m.text[0] == 0);
  }

  printf(failures ? "%d FAILED\n" : "all passed\n", failures);
  return failures ? 1 : 0;
}
