// Preview of the clock face for the Display Style setting. The layouts mirror
// showTime/showTemperature/showTemperatureSmall/showCombined in src/display.h
// (and tools/render.py); change them together.
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
    if (state.baseMs === null) return { h: 10, m: 8 };
    const d = new Date(state.baseMs + (Date.now() - state.loadedAt));
    return { h: d.getUTCHours(), m: d.getUTCMinutes() };
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

    if (state.shown !== style) {
      $box.empty();
      list.forEach(() => {
        $box.append($('<div class="small text-muted mt-2 preview-label">'))
            .append($('<canvas class="w-100" style="max-width:480px;border-radius:6px" width="' + PANEL.w + '" height="' + PANEL.h + '">'));
      });
      state.shown = style;
    }
    const labels = $box.find(".preview-label"), canvases = $box.find("canvas");
    list.forEach((item, i) => {
      labels.eq(i).text(item.label);
      draw(canvases[i], item.frame);
    });
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
    $("#displayStyle, #hourFormat").on("change", update);
    setInterval(() => { state.dots = !state.dots; update(); }, 1000);
  }

  window.ClockPreview = { update, init, frames };
  $(init);
})();
