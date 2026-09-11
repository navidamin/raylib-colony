// Stage-3 harness for survey-dashboard.html: fog, re-expressed.
//
// The console draws only what it has measured, so the thing to look at is the
// progression 0 -> 3 -> 9 holes at two yaws and two pitches. Zero holes is the
// case that matters most: the first thing a new player sees is a wire cage,
// and it has to read as unmeasured ground rather than as a page that failed
// to load.
//
//   NODE_PATH=/opt/node22/lib/node_modules node survey-fog-shots.js
const { chromium } = require('playwright');
const F = 'file:///home/user/raylib-colony/docs/design/prospecting/prototypes/survey-dashboard.html';
const OUT = '/tmp/claude-0/-home-user-raylib-colony/187272cf-7754-578f-9233-064508769c6d/scratchpad/fog/';
const GRID3 = [[6, 6], [21, 9], [12, 22]];
const GRID9 = [4, 14, 24].flatMap(i => [4, 14, 24].map(j => [i, j]));

(async () => {
  const b = await chromium.launch({ executablePath: '/opt/pw-browsers/chromium' });
  const p = await b.newPage({ viewport: { width: 1600, height: 1200 }, deviceScaleFactor: 2 });
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.goto(F); await p.waitForTimeout(1200);
  const L = await p.evaluate(() => window.__L());
  const bx = await p.locator('#cv').boundingBox(), k = bx.width / L.W;
  const crop = (n, r) => p.screenshot({ path: OUT + n, clip: { x: bx.x + r[0] * k, y: bx.y + r[1] * k, width: (r[2] - r[0]) * k, height: (r[3] - r[1]) * k } });
  const park = (yaw, pitch) => p.evaluate(([y, pi]) => { const s = window.__blk(); s.auto = false; s.yaw = y; s.pitch = pi; }, [yaw, pitch]);
  const set = holes => p.evaluate(hs => { window.__reset(); hs.forEach(([i, j]) => window.__reveal(i, j)); return window.__delin(); }, holes);

  for (const [tag, holes] of [['0', []], ['3', GRID3], ['9', GRID9]]) {
    const d = await set(holes);
    console.log('holes', tag, JSON.stringify(d));
    for (const [n, y, pi] of [['home', -0.1, 0.42], ['yaw90', 1.57, 0.42], ['flat', -0.1, 0.20], ['steep', -0.1, 1.2]]) {
      await park(y, pi); await p.waitForTimeout(420);
      await crop(`h${tag}_${n}.png`, L.block.grab);
    }
  }

  // isolate with fog still on it: the focus must show through, the ghosts must
  // be fogged the same way the solid block is
  await set(GRID3); await park(-0.1, 0.42);
  await p.evaluate(() => { const s = window.__blk(); s.selected = 2; s.target = 1; s.explode = 1; });
  await p.waitForTimeout(500); await crop('iso3.png', L.block.grab);
  await p.evaluate(() => { const s = window.__blk(); s.selected = -1; s.target = 0; s.explode = 0; });

  // the knowledge field itself, read where the renderer reads it
  console.log('know at hole / 4 cells / far corner:',
    JSON.stringify(await p.evaluate(() => [window.__know(6, 6, 900), window.__know(10, 6, 900),
                                           window.__know(26, 26, 900), window.__know(6, 6, 1900)])));

  /* FRAME COST. Only the frame interval means anything here (the note in the
     prototype says why), and a still console skips its frame entirely, so the
     number to compare is the one taken while the camera is being dragged.
     Three holes is the worst case for fog and not nine: nothing is uniform
     enough to take the single-polygon path, so every wall is drawn in strips. */
  const drag = async (label, holes) => {
    await set(holes); await park(-0.1, 0.42); await p.waitForTimeout(300);
    await p.evaluate(() => { let a = 0; const spin = () => { const s = window.__blk(); s.yaw = a += 0.02; if (a < 100) requestAnimationFrame(spin); }; spin(); });
    const ms = await p.evaluate(() => new Promise(res => {
      const t = []; let n = 0, last = performance.now();
      const step = () => { const now = performance.now(); t.push(now - last); last = now;
        if (++n < 70) requestAnimationFrame(step); else res(t.slice(10).sort((a, b) => a - b)[Math.floor((t.length - 10) / 2)]); };
      requestAnimationFrame(step);
    }));
    console.log(label, ms.toFixed(1), 'ms');
    await p.evaluate(() => { const s = window.__blk(); s.yaw = -0.1; });
  };
  await set([]); await park(-0.1, 0.42); await p.waitForTimeout(400);
  console.log('still, 0 holes (cage crawling)',
    (await p.evaluate(() => new Promise(res => { const t = []; let n = 0, last = performance.now();
      const step = () => { const now = performance.now(); t.push(now - last); last = now;
        if (++n < 70) requestAnimationFrame(step); else res(t.slice(10).sort((a, b) => a - b)[Math.floor((t.length - 10) / 2)]); };
      requestAnimationFrame(step); }))).toFixed(1), 'ms');
  await drag('dragging, 0 holes           ', []);
  await drag('dragging, 1 hole            ', [[8, 8]]);
  await drag('dragging, 3 holes           ', GRID3);
  await drag('dragging, 9 holes           ', GRID9);

  console.log(errs.length ? ('ERRORS ' + errs.join(' | ')) : 'no errors');
  await b.close();
})();
