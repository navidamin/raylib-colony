// Capture the diamond-drill sheet. Poses are reached by RUNNING the sim at a
// fixed dt in manual mode, never by teleporting depth -- a jumped depth leaves
// the core strip empty and the still lies about what the run produced.
//
// Usage: NODE_PATH=/opt/node22/lib/node_modules node shot.js <file.html> <outPrefix>
const { chromium } = require('playwright');

const drive = () => (o) => {
  window.__cmd(o.wob, o.rpm, true);
  const i = o.rig === undefined ? 1 : o.rig;
  for (let k = 0; k < 6000; k++) {
    window.__step(1 / 30);
    if (window.__rigs[i].depthM >= o.untilDepth) break;
  }
};

(async () => {
  const b = await chromium.launch({ executablePath: '/opt/pw-browsers/chromium' });
  const p = await b.newPage({ viewport: { width: 900, height: 1400 }, deviceScaleFactor: 2 });
  const errs = [];
  p.on('pageerror', e => errs.push('PAGEERROR: ' + e.message));
  await p.goto('file://' + process.argv[2]);
  await p.waitForTimeout(700);
  await p.evaluate(() => window.__manual(true));

  const shot = async (name, sel) =>
    p.locator(sel).screenshot({ path: `${process.argv[3]}-${name}.png` });

  // --- one continuous run at a sane technique, sampled three times ---
  for (const [name, wob, rpm, depth] of [
    ['a-soft',      0.80, 0.70, 26],
    ['b-fractured', 0.80, 0.70, 58],
    ['c-basalt',    0.80, 0.62, 116],
  ]) {
    await p.evaluate(drive(), { wob, rpm, untilDepth: depth });
    await p.waitForTimeout(150);
    await shot(name, '.sheet');
  }

  // --- the money shot: the compact glazing in basalt while the heavy cuts ---
  await p.evaluate(() => { document.getElementById('reset').click(); window.__step(0.001); });
  await p.evaluate(drive(), { wob: 0.80, rpm: 0.70, untilDepth: 71, rig: 0 });
  await p.waitForTimeout(150);
  await shot('c2-polishing', '.sheet');

  // --- the crown, close enough to judge ---
  const box = await p.locator('.variant canvas').nth(1).boundingBox();
  await p.screenshot({ path: `${process.argv[3]}-crown.png`,
    clip: { x: box.x + 4, y: box.y + box.height * 0.845, width: 132, height: 132 } });

  // --- a second run driven badly: heavy over-weighted and spun hard ---
  await p.evaluate(() => { document.getElementById('reset').click(); window.__step(0.001); });
  await p.evaluate(drive(), { wob: 1.0, rpm: 1.0, untilDepth: 96 });
  await p.waitForTimeout(150);
  await shot('d-hard-driven', '.sheet');

  // --- and the whole page, as published ---
  await p.evaluate(() => { document.getElementById('reset').click(); window.__step(0.001); });
  await p.evaluate(drive(), { wob: 0.80, rpm: 0.70, untilDepth: 44 });
  await p.waitForTimeout(150);
  await shot('e-page', '.wrap');

  console.log(errs.length ? errs.join('\n') : 'no errors');
  await b.close();
})();
