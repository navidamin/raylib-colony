// What the block costs, honestly: the FRAME INTERVAL, with the block hidden,
// spinning and still, at two device pixel ratios. Timing individual draw calls
// does not work on Canvas 2D -- rasterisation is deferred, so the numbers land
// on whichever call forces the flush. See the note in survey-dashboard.html.
const { chromium } = require('playwright');
const F='file:///home/user/raylib-colony/docs/design/prospecting/prototypes/survey-dashboard.html';
(async () => {
  const b = await chromium.launch({ executablePath: '/opt/pw-browsers/chromium' });
  for (const dsf of [2, 1.5]) {
    for (const [tag, setup] of [
      ['block off   ', ()=>{ window.__blockOff = true; }],
      ['spin, full  ', ()=>{ window.__blockOff=false; const s=window.__blk(); s.auto=true; s.fast=false; }],
      ['spin, fast  ', ()=>{ window.__blockOff=false; const s=window.__blk(); s.auto=true; s.fast=true; }],
      ['still, full ', ()=>{ window.__blockOff=false; const s=window.__blk(); s.auto=false; s.fast=false; }],
    ]) {
      const p = await b.newPage({ viewport:{width:1600,height:1200}, deviceScaleFactor:dsf });
      await p.goto(F); await p.waitForTimeout(900);
      await p.evaluate(setup); await p.waitForTimeout(300);
      const ms = await p.evaluate(()=>new Promise(res=>{
        const t=[]; let last=performance.now(), n=0;
        const step=()=>{ const now=performance.now(); t.push(now-last); last=now; n++;
          if(n<100) requestAnimationFrame(step); else { t.sort((a,b)=>a-b); res({med:+t[50].toFixed(1),p90:+t[90].toFixed(1)}); } };
        requestAnimationFrame(step); }));
      console.log('dsf', dsf, tag, JSON.stringify(ms));
      await p.close();
    }
  }
  await b.close();
})();
