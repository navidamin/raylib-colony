// The web build's site level must have its ground on it.
//
//   node tools/lunarmap/web_site_level_test.mjs <build dir>                    lunar_map
//   node tools/lunarmap/web_site_level_test.mjs <build dir> colony_game        the game
//   node tools/lunarmap/web_site_level_test.mjs <build dir> colony_game gpu    the game on
//                                                                              the GPU path
//   node tools/lunarmap/web_site_level_test.mjs <build dir> lunar_map gpu      lunar_map on it
//
// Serves the built page, opens it in headless Chromium at a large desktop
// window, walks Globe -> District -> Site with the mouse, and fails unless
// the site level's terrain synthesis is actually built -- with the regolith
// in it. Exit code 0 = ok.
//
// Why this exists: on 2026-09-23 the deployed site level came up flat grey
// -- the bare 1.9 km elevation model, no craters, no regolith -- and no
// test noticed. Every instrument rendered at 700-900 px on the desktop;
// the bug only showed in a BROWSER at a BIG window, where the page asked
// the CPU (wasm, one thread) for a 1523 px chain it could not finish. So
// this runs exactly there: WebGL through SwiftShader, as on a CI runner or
// a locked-down laptop, at the reporter's 1656 x 960.
//
// The same day, once the ladder was wired into the game, the game's site
// level showed the other face of the same mistake: on a device with a real
// GPU (the iPad) the chain was built by WebGL1 shaders that cannot run the
// regolith, and came up smooth and craterless. SwiftShader takes the CPU
// path and would never see that, so `gpu` loads the page with
// ?terrain=gpu, which puts it on the GPU path a real device takes. Since
// the web build moved to WebGL2 the GPU draws the regolith itself, so the
// game's `gpu` run also requires the WebGL2 context and a site level built
// on the GPU at 1024 px or more -- at 512 it was drawn 3x stretched.
//
// It reads the page's own console, not pixels:
//   lunar_map:    "CHAIN: <span> km layer at <res> px -> rung ... built in <ms> ms"
//                 when a level's synthesis is built, "CHAIN: layer off -- ..."
//                 when it is refused;
//   colony_game:  "TERRAIN: <span> km window at <res> px, CPU|GPU, regolith on|OFF,
//                 <ms> ms" when a level's window is built -- "real relief" in
//                 place of the regolith when the district is drawn from the
//                 moon's measured heights, which the game must reach too.
//
// The relief tiles are served at /relief/ from data/relief, as the deploy
// serves them beside the pages.

import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';

const RELIEF_DIR = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../data/relief');

const [dir, target = 'lunar_map', mode = ''] = process.argv.slice(2);
const game = target === 'colony_game';
if (!dir || !['lunar_map', 'colony_game'].includes(target)
    || !fs.existsSync(path.join(dir, target + '.html'))) {
  console.error('usage: web_site_level_test.mjs <dir containing lunar_map.html '
                + 'and/or colony_game.html> [lunar_map|colony_game] [gpu]');
  process.exit(2);
}
const page = target + '.html' + (mode === 'gpu' ? '?terrain=gpu' : '');
const label = target + (mode === 'gpu' ? ' (GPU path)' : '');

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
                '.wasm': 'application/wasm', '.data': 'application/octet-stream',
                '.jpg': 'image/jpeg' };
const server = http.createServer((req, res) => {
  const rel = decodeURIComponent(req.url.split('?')[0]);
  const root = rel.startsWith('/relief/') ? RELIEF_DIR : dir;
  const f = path.join(root, rel.startsWith('/relief/') ? rel.slice('/relief/'.length) : rel);
  if (!f.startsWith(path.resolve(root)) && !f.startsWith(root)) { res.writeHead(403); res.end(); return; }
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

// What each page says when it builds a level's ground, and when it refuses.
const built = game
  ? /TERRAIN: ([\d.]+) km window at (\d+) px, (CPU|GPU), (regolith on|regolith OFF|real relief), (\d+) ms/
  : /CHAIN: ([\d.]+) km layer at (\d+) px .* built in (\d+) ms/;
const refused = game ? /TERRAIN: [\d.]+ km window .* regolith OFF/ : /CHAIN: layer off/;
const ready = game ? /TERRAIN: WAC loaded/ : /CHAIN: mosaic warmed/;

const deadline = (ms) => Date.now() + ms;
async function waitFor(re, ms, what, from = 0) {
  const end = deadline(ms);
  while (Date.now() < end) {
    const hit = log.slice(from).find(l => re.test(l));
    if (hit) return hit;
    // A refusal is final; do not sit out the timeout on it.
    const no = log.find(l => refused.test(l));
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
const spanOf = (l) => +l.match(built)[1];

let failed = null;
try {
  await tab.goto(url, { waitUntil: 'load' });
  await waitFor(ready, 180000, 'the mosaic to load');
  await tab.waitForTimeout(3000);
  if (game) {
    await tap(0.50, 0.60);                                // past the title screen
    await tab.waitForTimeout(3000);
  }
  const atGlobe = log.length;
  await tap(0.46, game ? 0.40 : 0.33);                    // claim a region
  const district = await (async () => {
    const end = deadline(240000);
    while (Date.now() < end) {
      const hit = log.slice(atGlobe).find(l => built.test(l) && spanOf(l) >= 100);
      if (hit) return hit;
      if (log.some(l => refused.test(l))) break;
      await tab.waitForTimeout(500);
    }
    return null;
  })();
  if (!district && !log.some(l => refused.test(l)))
    throw new Error('the district level never built its ground (no level over 100 km)');
  // The game draws the district from the moon's real relief: built first
  // without it while the tiles arrive, then again with it.
  if (game) {
    const relief = /TERRAIN: ([\d.]+) km window at (\d+) px, (CPU|GPU), real relief, (\d+) ms/;
    const hit = await waitFor(relief, 120000, 'the district to be drawn from real relief', atGlobe);
    const [, span, res, by, ms] = hit.match(relief);
    console.log(`district: ${span} km window at ${res} px on the ${by}, real relief, ${ms} ms`);
  }
  const before = log.length;
  await tap(0.50, 0.50);                                  // descend to the site
  const end = deadline(240000);
  let site = null;
  while (!site && Date.now() < end) {
    site = log.slice(before).find(l => built.test(l) && spanOf(l) < 100);
    if (log.some(l => refused.test(l))) break;
    await tab.waitForTimeout(500);
  }
  if (log.some(l => refused.test(l)))
    failed = 'the page built a level WITHOUT its ground: ' + log.find(l => refused.test(l));
  else if (!site)
    failed = 'the site level never built its ground (no level under 100 km)';
  else if (game) {
    const [, span, res, by, , ms] = site.match(built);
    console.log(`site level: ${span} km window at ${res} px on the ${by}, regolith on, ${ms} ms`);
    // On the GPU path the page must be on WebGL2 and build the site level
    // at the screen's width. On WebGL1 it came up at 512 px -- stretched
    // 3x, visibly blurred (2026-09-24) -- or built small on the CPU.
    if (mode === 'gpu') {
      if (!log.some(l => /TERRAIN: WebGL2 context/.test(l)))
        failed = 'the page did not get a WebGL2 context';
      else if (by !== 'GPU')
        failed = `the site level was built on the ${by}, not the GPU`;
      else if (+res < 1024)
        failed = `the site level was built at ${res} px -- under 1024, it is drawn stretched and blurred`;
    }
  } else {
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
  console.error(`FAIL  ${label}: ${failed}`);
  console.error('--- what the page said ---');
  for (const l of chain.slice(0, 30)) console.error('  ' + l);
  process.exit(1);
}
console.log(`PASS  ${label}: the site level has its ground at ${W}x${H}`);
