// Preview of the clock face for the Display Style setting, and of a scrolling
// message as it will run. The layouts mirror showTime/showTemperature/
// showTemperatureSmall/showCombined in src/display.h, and renderScrollFrame
// and showDate in src/scroll.h (and tools/render.py); change them together.
(function () {
  const W = 32, H = 8;
  const COLON = 10, DEGREES = 13, MINUS = 14, UB_MINUS = 11;
  const NO_TEMP = -1000;
  const SAMPLE_TEMP = -15;

  const state = { baseMs: null, loadedAt: 0, temp: NO_TEMP, dots: true, shown: null };

  function newFrame() {
    return Array.from({ length: H }, () => Array(W).fill(null));
  }

  function printAt(f, x, y, idx, font, color) {
    const g = font.glyphs[idx];
    g.rows.forEach((row, j) => {
      for (let i = 0; i < g.w; i++) {
        if (row & (1 << (g.w - 1 - i))) {
          const px = x + i, py = y + j;
          if (px >= 0 && px < W && py >= 0 && py < H) f[py][px] = color;
        }
      }
    });
  }

  function to12(h, fmt) {
    return fmt === 12 ? (h === 0 ? 12 : h > 12 ? h - 12 : h) : h;
  }

  function digits(h, m) {
    return [Math.floor(h / 10), h % 10, Math.floor(m / 10), m % 10];
  }

  function timeFrame(h, m, dots, clock) {
    const f = newFrame(), font = CLOCK_FONTS.UserBold;
    const [h1, h2, m1, m2] = digits(h, m);
    const colon = dots ? clock : null;
    if (h1 > 0) {
      printAt(f, 1, 0, h1, font, clock);
      printAt(f, 8, 0, h2, font, clock);
      printAt(f, 15, 0, COLON, font, colon);
      printAt(f, 18, 0, m1, font, clock);
      printAt(f, 25, 0, m2, font, clock);
    } else {
      printAt(f, 2, 0, h2, font, clock);
      printAt(f, 10, 0, COLON, font, colon);
      printAt(f, 14, 0, m1, font, clock);
      printAt(f, 21, 0, m2, font, clock);
    }
    return f;
  }

  function tempGlyphs(t, minus) {
    if (t === NO_TEMP) return [minus, minus, minus];
    const p = Math.min(Math.abs(t), 99);
    const t1 = Math.floor(p / 10), t2 = p % 10;
    return [...(t < 0 ? [minus] : []), ...(t1 > 0 ? [t1] : []), t2];
  }

  function temperatureFrame(t, color) {
    const f = newFrame(), font = CLOCK_FONTS.UserBold;
    const glyphs = tempGlyphs(t, UB_MINUS);
    const withDegree = t !== NO_TEMP;
    const lastX = withDegree ? 22 : 26;
    glyphs.forEach((g, i) => printAt(f, lastX - 7 * (glyphs.length - 1 - i), 0, g, font, color));
    if (withDegree) printAt(f, 26, 0, DEGREES, font, color);
    return f;
  }

  function smallTemp(f, t, color) {
    const font = CLOCK_FONTS.Temp;
    const glyphs = tempGlyphs(t, MINUS);
    let x = W - (glyphs.reduce((s, g) => s + font.glyphs[g].w, 0) + glyphs.length - 1);
    glyphs.forEach(g => {
      printAt(f, x, 1, g, font, color);
      x += font.glyphs[g].w + 1;
    });
  }

  function combinedFrame(h, m, t, dots, clock, tempColor) {
    const f = newFrame(), font = CLOCK_FONTS.Time;
    const [h1, h2, m1, m2] = digits(h, m);
    if (h1 > 0) printAt(f, 0, 1, h1, font, clock);
    printAt(f, 5, 1, h2, font, clock);
    printAt(f, 10, 1, COLON, font, dots ? clock : null);
    printAt(f, 12, 1, m1, font, clock);
    printAt(f, 17, 1, m2, font, clock);
    smallTemp(f, t, tempColor);
    return f;
  }

  // style 1 = cycle (time screen, then temperature screen), 2 = combined
  function frames(style, fmt, h, m, t, dots, clock, tempColor) {
    const hh = to12(h, fmt);
    return style === 2
      ? [{ label: "Time and temperature together", frame: combinedFrame(hh, m, t, dots, clock, tempColor) }]
      : [{ label: "Time screen (5 s)", frame: timeFrame(hh, m, dots, clock) },
         { label: "Temperature screen (2 s)", frame: temperatureFrame(t, tempColor) }];
  }

  // Scrolling message, as renderScrollFrame draws it: every byte of the
  // UTF-8 text takes 6 columns (5 + a gap), letters show in upper case, and
  // bytes without a glyph (space, punctuation, accents) stay dark.
  const MSG_MAX = 127;  // messageText[128] in vars.h

  function messageBytes(text) {
    return Array.from(new TextEncoder().encode(text));
  }

  function scrollIndex(b) {
    const ch = b < 128 ? String.fromCharCode(b).toUpperCase() : "";
    return ch ? CLOCK_FONTS.Scroll.chars.indexOf(ch) : -1;
  }

  function messageFrame(text, pos, color) {
    const f = newFrame(), cols = CLOCK_FONTS.Scroll.cols;
    messageBytes(text).slice(0, MSG_MAX).forEach((b, k) => {
      const idx = scrollIndex(b);
      if (b === 32 || idx < 0) return;
      for (let col = 0; col < 5; col++) {
        const x = pos + k * 6 + col;
        if (x < 0 || x >= W) continue;
        for (let row = 0; row < 7; row++) {
          if (cols[idx][col] & (1 << row)) f[row][x] = color;
        }
      }
    });
    return f;
  }

  // Date screen, as showDate draws it (src/date_screen_logic.h): "THU 19"
  // centered, 1-column gaps, a 3-column space. wday 0 = Sunday.
  const DAYS = ["SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"];

  function dateFrame(wday, mday, color) {
    const f = newFrame(), cols = CLOCK_FONTS.Scroll.cols;
    const text = `${DAYS[wday]} ${mday}`;
    let x = Math.floor((W - ([...text].reduce((s, ch) => s + (ch === " " ? 3 : 6), 0) - 1)) / 2);
    for (const ch of text) {
      if (ch === " ") { x += 3; continue; }
      const idx = scrollIndex(ch.charCodeAt(0));
      for (let col = 0; col < 5; col++) {
        if (x + col < 0 || x + col >= W) continue;
        for (let row = 0; row < 7; row++) {
          if (cols[idx][col] & (1 << row)) f[row][x + col] = color;
        }
      }
      x += 6;
    }
    return f;
  }

  // img/panel.jpg is a photo of the real wood front panel with the LEDs off
  // (made by tools/make_panel.py). The LED grid position is measured from it.
  const PANEL = { w: 768, h: 195, x0: 17.2, y0: 25.2, dx: 20.77, dy: 20.56 };
  const panel = new Image();
  panel.onload = () => update();
  panel.src = "img/panel.jpg";

  // A lit LED seen through the wood: a soft halo, a solid dot and a pale core.
  function led(ctx, cx, cy, c) {
    const p = PANEL.dx;
    const rgba = a => `rgba(${c.r}, ${c.g}, ${c.b}, ${a})`;
    const w = k => Math.round(255 - (255 - k) * 0.45);  // toward white
    const g = ctx.createRadialGradient(cx, cy, 0, cx, cy, p * 0.75);
    g.addColorStop(0, `rgb(${w(c.r)}, ${w(c.g)}, ${w(c.b)})`);
    g.addColorStop(0.2, rgba(1));
    g.addColorStop(0.45, rgba(0.95));
    g.addColorStop(0.62, rgba(0.3));
    g.addColorStop(1, rgba(0));
    ctx.fillStyle = g;
    ctx.fillRect(cx - p * 0.75, cy - p * 0.75, p * 1.5, p * 1.5);
  }

  function draw(canvas, frame) {
    const ctx = canvas.getContext("2d");
    if (panel.complete && panel.naturalWidth) {
      ctx.drawImage(panel, 0, 0, canvas.width, canvas.height);
    } else {
      ctx.fillStyle = "#d9a07a";  // wood tone until the photo loads
      ctx.fillRect(0, 0, canvas.width, canvas.height);
    }
    for (let y = 0; y < H; y++) {
      for (let x = 0; x < W; x++) {
        if (frame[y][x]) led(ctx, PANEL.x0 + x * PANEL.dx, PANEL.y0 + y * PANEL.dy, frame[y][x]);
      }
    }
  }

  // The LEDs are scaled by the brightness setting anyway, so brighten dim
  // colors (keeping the hue) to keep the digits readable on screen.
  function css(c) {
    const k = Math.max(c.r, c.g, c.b);
    const s = k > 0 && k < 160 ? 160 / k : 1;
    return { r: Math.round(c.r * s), g: Math.round(c.g * s), b: Math.round(c.b * s) };
  }

  // Device time advanced by however long this page has been open.
  function sampleTime() {
    if (state.baseMs === null) {
      const today = new Date();
      return { h: 10, m: 8, wday: today.getDay(), mday: today.getDate() };
    }
    const d = new Date(state.baseMs + (Date.now() - state.loadedAt));
    return { h: d.getUTCHours(), m: d.getUTCMinutes(), wday: d.getUTCDay(), mday: d.getUTCDate() };
  }

  function update() {
    const $box = $("#stylePreview");
    if (!$box.length || !$("#clock_color").data("minicolors-initialized")) return;
    const style = parseInt($("#displayStyle").val(), 10) === 2 ? 2 : 1;
    const fmt = parseInt($("#hourFormat").val(), 10) === 24 ? 24 : 12;
    const clock = css($("#clock_color").minicolors("rgbObject"));
    const tempColor = css($("#temp_color").minicolors("rgbObject"));
    const t = sampleTime();
    // "---" is what the clock shows with no Home Assistant reading (e.g. the
    // first minute after boot); a preview is more useful with a sample value.
    const hasTemp = state.temp !== NO_TEMP;
    const list = frames(style, fmt, t.h, t.m, hasTemp ? state.temp : SAMPLE_TEMP, state.dots, clock, tempColor);
    if (!hasTemp) {
      list.forEach(item => {
        if (/emperature/.test(item.label)) item.label += " - sample value, clock has no reading yet";
      });
    }

    // src/screen_cycle_logic.h: 1 = once a minute, 2 = a 2 s step after
    // the temperature (or after the combined screen)
    const dateMode = parseInt($("#dateMode").val(), 10);
    if (dateMode === 2) {
      if (style === 2) list[0].label += " (5 s)";
      list.push({ label: "Date (2 s, then the cycle starts again)", frame: dateFrame(t.wday, t.mday, messageColor()) });
    } else if (dateMode === 1) {
      list.push({ label: "Date (3 s once a minute, message color)", frame: dateFrame(t.wday, t.mday, messageColor()) });
    }

    const shape = `${style}/${list.length}`;
    if (state.shown !== shape) {
      $box.empty();
      list.forEach(() => {
        $box.append($('<div class="small text-muted mt-2 preview-label">'))
            .append($('<canvas class="w-100" style="max-width:480px;border-radius:6px" width="' + PANEL.w + '" height="' + PANEL.h + '">'));
      });
      state.shown = shape;
    }
    const labels = $box.find(".preview-label"), canvases = $box.find("canvas");
    list.forEach((item, i) => {
      labels.eq(i).text(item.label);
      draw(canvases[i], item.frame);
    });
  }

  // The message card's preview: runs the text across the panel at the chosen
  // scroll delay, the chosen number of passes (up to 3), then a short pause.
  const msg = { timer: null, pos: W, pass: 0 };

  function messageColor() {
    const $c = $("#other_color");
    return css($c.data("minicolors-initialized") ? $c.minicolors("rgbObject") : { r: 255, g: 0, b: 0 });
  }

  function seconds(ms) {
    const s = Math.round(ms / 1000);
    return ms < 60000 ? `${(ms / 1000).toFixed(1)} s` : `${Math.floor(s / 60)} min ${s % 60} s`;
  }

  // Width and timing as on the clock: a pass starts with the text just off
  // the right edge and ends when its last column has left on the left.
  function messageInfo(text, delay, repeat) {
    const bytes = messageBytes(text);
    const shown = bytes.slice(0, MSG_MAX);
    const width = shown.length * 6 - 1;
    const notes = [];
    const blank = new Set();
    for (const ch of text) {
      const b = messageBytes(ch);
      if (ch !== " " && (b.length > 1 || scrollIndex(b[0]) < 0)) blank.add(ch);
    }
    if (blank.size) notes.push(`No glyph, shown as a gap: ${[...blank].join(" ")}`);
    if (bytes.length > MSG_MAX) notes.push(`Too long: only the first ${MSG_MAX} bytes are shown, the rest is cut off`);
    const okDelay = delay >= 10 && delay <= 1000, okRepeat = repeat >= 1 && repeat <= 100;
    if (!okDelay) notes.push("Scroll delay must be 10-1000 ms; the clock will refuse this");
    if (!okRepeat) notes.push("Repeat count must be 1-100; the clock will refuse this");
    const pass = (W + 1 + width) * (okDelay ? delay : 50);
    const timing = okRepeat && repeat > 1
      ? `One pass ${seconds(pass)}, ${repeat} passes ${seconds(pass * repeat)}`
      : `Runs for ${seconds(pass)}`;
    return { width, text: [timing, ...notes].join(". ") + ".", warn: notes.length > 0 };
  }

  function messageSettings() {
    const text = String($("#messageText").val() || "");
    const delay = parseInt($("#scrollDelay").val(), 10);
    const repeat = parseInt($("#repeatCount").val(), 10);
    return { text, delay, repeat, info: messageInfo(text, delay, repeat) };
  }

  function messageStep() {
    const canvas = document.getElementById("messagePreview");
    if (!canvas) return;
    const m = messageSettings();
    const delay = m.delay >= 10 && m.delay <= 1000 ? m.delay : 50;
    if (!m.text) {
      draw(canvas, newFrame());
      msg.timer = null;
      return;
    }
    if (msg.pos < -m.info.width) {  // one pass done
      msg.pass++;
      msg.pos = W;
      if (msg.pass >= Math.min(Math.max(m.repeat, 1) || 1, 3)) {
        msg.pass = 0;
        draw(canvas, newFrame());
        msg.timer = setTimeout(messageStep, 1200);
        return;
      }
    }
    draw(canvas, messageFrame(m.text, msg.pos, messageColor()));
    msg.pos--;
    msg.timer = setTimeout(messageStep, delay);
  }

  function restartMessage() {
    const m = messageSettings();
    $("#messageInfo").text(m.text ? m.info.text : "Type a message to preview it.")
      .toggleClass("text-warning", m.info.warn && !!m.text).toggleClass("text-muted", !(m.info.warn && m.text));
    clearTimeout(msg.timer);
    msg.pos = W;
    msg.pass = 0;
    messageStep();
  }

  function init() {
    $.get("/info", i => {
      const m = /^(\d{4})-(\d\d)-(\d\d) (\d\d):(\d\d):(\d\d)/.exec(i.localTime || "");
      if (m && i.timeSynced) {
        state.baseMs = Date.UTC(+m[1], m[2] - 1, +m[3], +m[4], +m[5], +m[6]);
        state.loadedAt = Date.now();
      }
      if (typeof i.temperature === "number") state.temp = i.temperature;
      update();
    });
    $("#displayStyle, #hourFormat, #dateMode").on("change", update);
    $("#messageText, #scrollDelay, #repeatCount").on("input", restartMessage);
    restartMessage();
    setInterval(() => { state.dots = !state.dots; update(); }, 1000);
  }

  window.ClockPreview = { update, init, frames, messageFrame, messageInfo, dateFrame };
  $(init);
})();
