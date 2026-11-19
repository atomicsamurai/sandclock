// Helper for tests/preview_vs_render.py: prints the web UI preview's frames as
// ASCII (## lit, ". " dark), loading data/js/fonts.js and preview.js in a stub
// browser context.
// Usage: node preview_frames.js <style> <hourFormat> <h> <m> <temp>
//        node preview_frames.js message <text> <pos>
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const root = path.resolve(__dirname, '..');
const ctx = { window: {}, $: () => {}, setInterval: () => {}, Image: function () {}, Date, Math, Array, parseInt, String, Set, TextEncoder };
vm.createContext(ctx);
vm.runInContext(fs.readFileSync(path.join(root, 'data/js/fonts.js'), 'utf8') + '\nthis.CLOCK_FONTS = CLOCK_FONTS;', ctx);
vm.runInContext(fs.readFileSync(path.join(root, 'data/js/preview.js'), 'utf8'), ctx);

const ascii = frame => frame.map(r => r.map(c => (c ? '##' : '. ')).join('')).join('\n');
if (process.argv[2] === 'message') {
  console.log(ascii(ctx.window.ClockPreview.messageFrame(process.argv[3], Number(process.argv[4]), 'M')));
} else {
  const [style, fmt, h, m, t] = process.argv.slice(2).map(Number);
  for (const { frame } of ctx.window.ClockPreview.frames(style, fmt, h, m, t, true, 'C', 'T')) {
    console.log(ascii(frame));
    console.log('--');
  }
}
