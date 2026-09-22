// Mouse-units regression test for the browser build.
//
// Emscripten 3.1.64's library_glfw.js hands raylib the mouse in CSS pixels
// (its calculateMouseCoords scales by clientWidth / rect.width, which is
// 1), while the frame is 1280x720 framebuffer pixels that minshell.html
// CSS-fits to the viewport. InputManager::FixWebPointerUnits() puts the
// framebuffer / CSS-box scaling back. This runs the exact JS the game
// injects -- extracted from src/Engine/inputmanager.cpp, so the test and
// the game cannot drift -- in real Chromium against 3.1.64's function,
// for a 1600 px box (1.25x past the cursor before the fix), a 1024 px box
// (0.8x short) and a 1280 px box (lunar_map: unchanged).
//
//   NODE_PATH=/opt/node22/lib/node_modules node tools/shell-test/mouse_units_test.js
//
// Exits non-zero on failure. Run it after touching FixWebPointerUnits or
// bumping emsdk. SHELL_TEST_CHROME overrides the browser executable, as
// for shell_test.js.
const { chromium } = require('playwright');
const fs = require('fs');
const path = require('path');
const src = fs.readFileSync(path.resolve(__dirname, '../../src/Engine/inputmanager.cpp'), 'utf8');
const m = src.match(/EM_ASM\(\{([\s\S]*?)\n    \}\);/);
if (!m) { console.error('EM_ASM body not found'); process.exit(2); }
const body = m[1];
// EM_ASM is a variadic C macro: a comma outside parentheses splits the JS
// into extra macro arguments and the web build fails to compile (braces
// do not protect it). Refuse such a body here, before CI does.
{
  let depth = 0, inStr = null;
  for (let i = 0; i < body.length; i++) {
    const ch = body[i];
    if (inStr) { if (ch === inStr && body[i - 1] !== '\\') inStr = null; continue; }
    if (ch === "'" || ch === '"') { inStr = ch; continue; }
    if (ch === '(') depth++;
    else if (ch === ')') depth--;
    else if (ch === ',' && depth === 0) {
      console.log('FAIL EM_ASM body has a comma outside parentheses at offset ' + i + ': ' + body.slice(Math.max(0, i - 30), i + 10).replace(/\n/g, ' '));
      process.exit(1);
    }
  }
  console.log('PASS EM_ASM body has no top-level comma');
}
const glfw364 = `function(pageX, pageY) {
  var rect = Module["canvas"].getBoundingClientRect();
  var cw = Module["canvas"].clientWidth; var ch = Module["canvas"].clientHeight;
  var scrollX = ((typeof window.scrollX != 'undefined') ? window.scrollX : window.pageXOffset);
  var scrollY = ((typeof window.scrollY != 'undefined') ? window.scrollY : window.pageYOffset);
  var adjustedX = pageX - (scrollX + rect.left); var adjustedY = pageY - (scrollY + rect.top);
  adjustedX = adjustedX * (cw / rect.width); adjustedY = adjustedY * (ch / rect.height);
  return { x: adjustedX, y: adjustedY }; }`;
function page(cssW, cssH) {
  return `<!doctype html><html><body style="margin:0">
<canvas id="canvas" width="1280" height="720" style="display:block;width:${cssW}px;height:${cssH}px"></canvas>
<script>
var Module = { canvas: document.getElementById('canvas') };
var Browser = { calculateMouseCoords: ${glfw364} };
window.before = [Browser.calculateMouseCoords(800, 450), Browser.calculateMouseCoords(55, 340)];
(function(){ ${body} })();
window.after = [Browser.calculateMouseCoords(800, 450), Browser.calculateMouseCoords(55, 340)];
</script></body></html>`;
}
(async () => {
  const browser = await chromium.launch(process.env.SHELL_TEST_CHROME ? { executablePath: process.env.SHELL_TEST_CHROME } : {});
  const pg = await browser.newPage({ viewport: { width: 1700, height: 1000 } });
  let fails = 0;
  const check = (name, got, exp) => {
    const ok = Math.abs(got.x - exp.x) < 0.01 && Math.abs(got.y - exp.y) < 0.01;
    console.log((ok ? 'PASS ' : 'FAIL ') + name + '  got ' + got.x.toFixed(2) + ',' + got.y.toFixed(2) + '  expected ' + exp.x + ',' + exp.y);
    if (!ok) fails++;
  };
  // 1) the deployed game: 1280x720 frame, CSS-fitted to 1600x900 (the 1.25x case)
  await pg.setContent(page(1600, 900));
  let r = await pg.evaluate(() => ({ before: window.before, after: window.after }));
  check('3.1.64 as shipped, 1600 px box: mouse in CSS px (the bug)', r.before[0], { x: 800, y: 450 });
  check('with the fix, 1600 px box: 800,450 -> frame', r.after[0], { x: 640, y: 360 });
  check('with the fix, 1600 px box: 55,340 -> frame', r.after[1], { x: 44, y: 272 });
  // 2) a small window: 1280x720 frame CSS-shrunk to 1024x576
  await pg.setContent(page(1024, 576));
  r = await pg.evaluate(() => ({ before: window.before, after: window.after }));
  check('with the fix, 1024 px box: 800,450 -> frame', r.after[0], { x: 1000, y: 562.5 });
  // 3) lunar_map: framebuffer equals the CSS box -> unchanged
  await pg.setContent(page(1280, 720));
  r = await pg.evaluate(() => ({ before: window.before, after: window.after }));
  check('frame == CSS box: unchanged', r.after[0], r.before[0]);
  await browser.close();
  process.exit(fails ? 1 : 0);
})();
