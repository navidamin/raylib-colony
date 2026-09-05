// Zoomed crops of a drill variant so the thread geometry can actually be seen.
// Usage: node zoomB.js <file.html> <outPrefix>
const { chromium } = require('playwright');
(async () => {
  const b = await chromium.launch({ executablePath: '/opt/pw-browsers/chromium' });
  const p = await b.newPage({ viewport: { width: 1120, height: 1080 }, deviceScaleFactor: 3 });
  const errs = [];
  p.on('pageerror', e => errs.push('PAGEERROR: ' + e.message));
  await p.goto('file://' + process.argv[2]);
  await p.waitForTimeout(600);
  const poses = [
    { name: 'z-cool',    s: { depthM: 30, heat: 0.10, auto: false, cracks: [] }, y0: 40,  y1: 340 },
    { name: 'z-hot',     s: { depthM: 58, heat: 0.72, auto: false, cracks: [] }, y0: 240, y1: 500 },
    { name: 'z-cracked', s: { depthM: 88, heat: 0.95, auto: false,
                              cracks: [{m:80,seed:0.3},{m:83,seed:0.62}] },      y0: 420, y1: 700 },
    // geometry-only pose: rpm 0 so no chips/debris sit on top of the thread
    { name: 'z-clean',   s: { depthM: 46, heat: 0.0, rpm: 0, feed: 0,
                              auto: false, cracks: [] },                         y0: 90,  y1: 420 },
  ];
  const box = await p.locator('.stage').boundingBox();
  for (const pose of poses) {
    await p.evaluate(s => window.__setState(s), pose.s);
    await p.waitForTimeout(320);
    await p.screenshot({ path: process.argv[3] + '-' + pose.name + '.png',
      clip: { x: box.x + 228, y: box.y + pose.y0, width: 144, height: pose.y1 - pose.y0 } });
  }
  console.log(errs.length ? errs.join('\n') : 'no errors');
  await b.close();
})();
