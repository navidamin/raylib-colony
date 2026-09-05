// Stills of the layer block: the stacked body, then each layer peeled.
// Deterministic poses -- the harness parks the clock, the bit and the pointer
// before the frame reads them, so output does not depend on headless timing.
//
//   npm i -D playwright   (or NODE_PATH=<global>/node_modules)
//   node block-shots.js
const { chromium } = require('playwright');
const F = 'file://' + require('path').resolve(__dirname, 'layer-block.html');

const SHOTS = [
  { name: 'block-stacked', state: { sel: -1, drilling: true,  depth: 45 } },
  { name: 'block-peel-0',  state: { sel:  0, drilling: false, depth: 74 } },
  { name: 'block-peel-1',  state: { sel:  1, drilling: false, depth: 74 } },
  { name: 'block-peel-2',  state: { sel:  2, drilling: false, depth: 74 } },
  { name: 'block-peel-3',  state: { sel:  3, drilling: false, depth: 74 } },
  // the twin cursors: a cell on the surface and its depth in the bar
  { name: 'block-hover',   state: { sel: -1, drilling: false, depth: 45, mouse: [337, 178] } },
  // the four generated strata, to check the port against
  // `tools/preview/preview.sh --module strata`
  { name: 'block-strata',  query: '?probe=tex' },
];

(async () => {
  const b = await chromium.launch();
  for (const s of SHOTS) {
    const p = await b.newPage({ viewport: { width: 880, height: 1100 }, deviceScaleFactor: 2 });
    const errs = [];
    p.on('pageerror', e => errs.push(e.message));
    await p.goto(F + (s.query || ''));
    await p.waitForTimeout(900);                       // the rock is generated on load
    if (s.state) {
      await p.evaluate(st => {
        window.__manual(true);
        window.__setState(Object.assign({ phase: 3.3, t: 3.3, mouse: [-1, -1] }, st));
        window.__step(1 / 60); window.__step(1 / 60);  // pick, then light, then draw
      }, s.state);
      await p.waitForTimeout(120);
    }
    await p.locator('.stage').screenshot({ path: s.name + '.png' });
    console.log(s.name, errs.length ? errs.join(' | ') : 'ok');
    await p.close();
  }
  await b.close();
})();
