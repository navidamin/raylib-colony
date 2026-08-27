// Stills of the three drill-dock variants, deterministic mid-drill pose.
const { chromium } = require('playwright');
const F='file:///home/user/raylib-colony/docs/design/prospecting/prototypes/drill-dock.html';
(async () => {
  const b = await chromium.launch({ executablePath: '/opt/pw-browsers/chromium' });
  for (const v of ['a','b','c']){
    const p = await b.newPage({ viewport:{width:640,height:900}, deviceScaleFactor:2 });
    const errs=[]; p.on('pageerror',e=>errs.push(e.message));
    await p.goto(F+'?v='+v); await p.waitForTimeout(700);
    await p.evaluate(()=>{ window.__manual(true);
      window.__setState({auto:false, trace:{c:[2,5],g:[5,2]}, mode:'drill', depth:74, t:3.30});
      window.__step(1/60); });
    await p.locator('.stage').screenshot({ path:`dock4-${v}.png` });
    console.log(v, errs.length?errs.join(' | '):'no errors');
    await p.close();
  }
  await b.close();
})();
