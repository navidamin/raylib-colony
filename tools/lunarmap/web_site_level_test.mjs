// The web build's site level must have its ground on it.
//
//   node tools/lunarmap/web_site_level_test.mjs <dir-with-lunar_map.html>
//
// Serves the built page, opens it in headless Chromium at a large desktop
// window, walks Globe -> District -> Site with the mouse, and fails unless
// the site level's terrain synthesis is actually built. Exit code 0 = ok.
//
// Why this exists: on 2026-09-23 the deployed site level came up flat grey
// -- the bare 1.9 km elevation model, no craters, no regolith -- and no
// test noticed. Every instrument rendered at 700-900 px on the desktop;
// the bug only showed in a BROWSER at a BIG window, where the page asked
// the CPU (wasm, one thread) for a 1523 px chain it could not finish. So
// this runs exactly there: WebGL through SwiftShader, as on a CI runner or
// a locked-down laptop, which puts the layer on the CPU path, at the
// reporter's 1656 x 960.
//
// It reads the page's own console, not pixels:
//   "CHAIN: <span> km layer at <res> px -> rung ... built in <ms> ms"
// is printed when a level's synthesis is built, and
//   "CHAIN: layer off -- ..." when it is refused.

import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import { createRequire } from 'node:module';

const dir = process.argv[2];
if (!dir || !fs.existsSync(path.join(dir, 'lunar_map.html')) && !fs.existsSync(path.join(dir, 'index.html'))) {
  console.error('usage: web_site_level_test.mjs <dir containing lunar_map.html or index.html>');
  process.exit(2);
}
const page = fs.existsSync(path.join(dir, 'lunar_map.html')) ? 'lunar_map.html' : 'index.html';

// Playwright from wherever it is: a local install, or the global one.
let chromium;
try { ({ chromium } = await import('playwright')); }
catch {
  const req = createRequire(import.meta.url);
  const globalRoot = process.env.PLAYWRIGHT_MODULE
    || '/opt/node22/lib/node_modules/playwright';
  ({ chromium } = req(globalRoot));
}

const TYPES = { '.html': 'text/html', '.js': 'text/javascript',
                '.wasm': 'application/wasm', '.data': 'application/octet-stream' };
const server = http.createServer((req, res) => {
  const f = path.join(dir, decodeURIComponent(req.url.split('?')[0]));
  if (!f.startsWith(path.resolve(dir)) && !f.startsWith(dir)) { res.writeHead(403); res.end(); return; }
  fs.readFile(f, (err, buf) => {
    if (err) { res.writeHead(404); res.end(); return; }
    res.writeHead(200, { 'Content-Type': TYPES[path.extname(f)] || 'application/octet-stream' });
    res.end(buf);
  });
});
await new Promise(r => server.listen(0, '127.0.0.1', r));
const url = `http://127.0.0.1:${server.address().port}/${page}`;

const W = 1656, H = 960;           // the window the bug was reported at
const browser = await chromium.launch({
  args: ['--use-gl=swiftshader', '--enable-unsafe-swiftshader'] });
const tab = await browser.newPage({ viewport: { width: W, height: H } });
const log = [];
tab.on('console', m => log.push(m.text()));
tab.on('pageerror', e => log.push('PAGEERROR ' + e.message));

const deadline = (ms) => Date.now() + ms;
const refusedRe = /CHAIN: layer off/;
async function waitFor(re, ms, what) {
  const end = deadline(ms);
  while (Date.now() < end) {
    const hit = log.find(l => re.test(l));
    if (hit) return hit;
    // A refusal is final; do not sit out the timeout on it.
    const no = log.find(l => refusedRe.test(l));
    if (no) throw new Error('the page REFUSED the ground while waiting for ' + what + ': ' + no);
    await tab.waitForTimeout(500);
  }
  throw new Error(`timed out after ${ms / 1000}s waiting for ${what}`);
}
async function tap(fx, fy) {
  const c = await tab.$('canvas'); const b = await c.boundingBox();
  const x = b.x + b.width * fx, y = b.y + b.height * fy;
  await tab.mouse.move(x, y); await tab.waitForTimeout(1200);
  await tab.mouse.down(); await tab.waitForTimeout(120); await tab.mouse.up();
}
const built = /CHAIN: ([\d.]+) km layer at (\d+) px .* built in (\d+) ms/;
const refused = refusedRe;

let failed = null;
try {
  await tab.goto(url, { waitUntil: 'load' });
  await waitFor(/CHAIN: mosaic warmed/, 180000, 'the mosaic to load');
  await tab.waitForTimeout(3000);
  await tap(0.46, 0.33);                                  // claim a region
  await waitFor(built, 240000, 'the district level to build its ground');
  const before = log.length;
  await tap(0.50, 0.50);                                  // descend to the site
  const end = deadline(240000);
  let site = null;
  while (!site && Date.now() < end) {
    site = log.slice(before).find(l => built.test(l) && +l.match(built)[1] < 100);
    if (log.some(l => refused.test(l))) break;
    await tab.waitForTimeout(500);
  }
  if (log.some(l => refused.test(l)))
    failed = 'the page REFUSED the site level\'s ground: ' + log.find(l => refused.test(l));
  else if (!site)
    failed = 'the site level never built its ground (no "CHAIN: ... km layer ... built" under 100 km)';
  else {
    const [, span, res, ms] = site.match(built);
    console.log(`site level: ${span} km layer at ${res} px, built in ${ms} ms`);
  }
} catch (e) {
  failed = e.message;
} finally {
  await browser.close();
  server.close();
}

const chain = log.filter(l => /CHAIN|TERRAIN|PAGEERROR/.test(l));
if (failed) {
  console.error('FAIL  ' + failed);
  console.error('--- what the page said ---');
  for (const l of chain.slice(0, 30)) console.error('  ' + l);
  process.exit(1);
}
console.log('PASS  the site level has its ground at ' + W + 'x' + H);
