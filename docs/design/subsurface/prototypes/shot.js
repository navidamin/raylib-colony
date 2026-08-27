// Render a drill variant at three poses. Usage: node shot.js <file.html> <outPrefix>
const { chromium } = require('playwright');
(async () => {
  const b = await chromium.launch({ executablePath: '/opt/pw-browsers/chromium' });
  const p = await b.newPage({ viewport: { width: 1120, height: 1080 } });
  const errs = [];
  p.on('pageerror', e => errs.push('PAGEERROR: ' + e.message));
  await p.goto('file://' + process.argv[2]);
  await p.waitForTimeout(600);
  const poses = [
    { name: 'cool',    s: { depthM: 30, heat: 0.10, auto: false, cracks: [] } },
    { name: 'hot',     s: { depthM: 58, heat: 0.72, auto: false, cracks: [] } },
    { name: 'cracked', s: { depthM: 88, heat: 0.95, auto: false,
                            cracks: [{m:80,seed:0.3},{m:83,seed:0.62},{m:86,seed:0.85}] } },
  ];
  for (const pose of poses) {
    await p.evaluate(s => window.__setState(s), pose.s);
    await p.waitForTimeout(320);
    await p.locator('.stage').screenshot({ path: process.argv[3] + '-' + pose.name + '.png' });
  }
  console.log(errs.length ? errs.join('\n') : 'no errors');
  await b.close();
})();
