// Frame time for survey-dashboard.html at both breakpoints, measured with a
// sheet held mid-slide so the live layer is doing its most work.
// Standing budget: 16.7 ms median.
const { chromium } = require('playwright');
const F='file:///home/user/raylib-colony/docs/design/prospecting/prototypes/survey-dashboard.html';
(async () => {
  const b = await chromium.launch({ executablePath: '/opt/pw-browsers/chromium' });
  for (const [name, vp, dsf] of [['desk',{width:1600,height:1200},1.5], ['phone',{width:390,height:844},3]]) {
    const p = await b.newPage({ viewport: vp, deviceScaleFactor: dsf });
    const errs=[]; p.on('pageerror',e=>errs.push(e.message));
    await p.goto(F); await p.waitForTimeout(1000);
    // hold a sheet mid-open so the live layer is doing its most work
    await p.evaluate(()=>{ const l=window.__L(); if(l.tabs.length){ const tb=l.tabs[0], r=document.getElementById('cv').getBoundingClientRect();
      document.getElementById('cv').dispatchEvent(new PointerEvent('pointerdown',{clientX:r.left+(tb.x+tb.w/2)/l.W*r.width,clientY:r.top+(tb.y+tb.h/2)/l.H*r.height,bubbles:true})); } });
    const ms = await p.evaluate(()=>new Promise(res=>{
      const t=[]; let last=performance.now(), n=0;
      const step=()=>{ const now=performance.now(); t.push(now-last); last=now; n++;
        if(n<90) requestAnimationFrame(step); else { t.sort((a,b)=>a-b); res({med:+t[45].toFixed(1), p90:+t[81].toFixed(1)}); } };
      requestAnimationFrame(step); }));
    console.log(name, JSON.stringify(ms), errs.length?('ERRORS '+errs.join('|')):'no errors');
    await p.close();
  }
  await b.close();
})();
