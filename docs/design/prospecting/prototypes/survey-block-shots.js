// Stage-1 harness for survey-dashboard.html: our ground through Holo3D.
//
// Parks the camera before every shot -- the console is still until touched, so
// a pose has to be set rather than waited for -- and sweeps 32 yaw/pitch poses
// checking the block stays inside the region it is drawn into. A clipped block
// at an extreme camera is exactly what a single still frame will not show you.
//
//   NODE_PATH=/opt/node22/lib/node_modules node survey-block-shots.js
const { chromium } = require('playwright');
const F='file:///home/user/raylib-colony/docs/design/prospecting/prototypes/survey-dashboard.html';
const OUT='/tmp/claude-0/-home-user-raylib-colony/187272cf-7754-578f-9233-064508769c6d/scratchpad/shots2/';
(async () => {
  const b = await chromium.launch({ executablePath: '/opt/pw-browsers/chromium' });
  const p = await b.newPage({ viewport:{width:1600,height:1200}, deviceScaleFactor:2 });
  const errs=[]; p.on('pageerror',e=>errs.push(e.message));
  await p.goto(F); await p.waitForTimeout(1200);
  const L = await p.evaluate(()=>window.__L());
  console.log('block', JSON.stringify(L.block));
  // park the camera so the shot is deterministic
  const park = (yaw,pitch) => p.evaluate(([y,pi])=>{ const s=window.__blk(); s.auto=false; s.yaw=y; s.pitch=pi; }, [yaw,pitch]);
  const bx = await p.locator('#cv').boundingBox(), k = bx.width/L.W;
  const crop = (n, r) => p.screenshot({ path: OUT+n, clip:{ x: bx.x+r[0]*k, y: bx.y+r[1]*k, width:(r[2]-r[0])*k, height:(r[3]-r[1])*k } });
  await park(-0.1, 0.42); await p.waitForTimeout(400);
  console.log('bounds', JSON.stringify(await p.evaluate(()=>window.__bounds())));
  await crop('b_home.png', L.block.grab);
  for (const [n,y,pi] of [['b_yaw90',1.57,0.42],['b_yaw180',3.14,0.42],['b_flat',-0.1,0.20],['b_steep',-0.1,1.2]]) {
    await park(y,pi); await p.waitForTimeout(350); await crop(n+'.png', L.block.grab);
  }
  // does the block stay inside the region it is drawn into, at every camera?
  let bad = 0;
  for (const y of [0, 0.8, 1.6, 2.4, 3.1, 3.9, 4.7, 5.5]) for (const pi of [0.18, 0.42, 0.8, 1.25]) {
    await park(y, pi); await p.waitForTimeout(60);
    const f = await p.evaluate(()=>window.__fits());
    if (!f.fits) { bad++; if (bad < 4) console.log('CLIPPED at yaw', y.toFixed(2), 'pitch', pi, JSON.stringify(f)); }
  }
  console.log('camera sweep: 32 poses,', bad, 'clipped');
  console.log(errs.length?('ERRORS '+errs.join(' | ')):'no errors');
  await b.close();
})();
