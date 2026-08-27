// Animated capture of one drill-dock variant: plan -> drill -> done.
const { chromium } = require('playwright');
const F='file:///home/user/raylib-colony/docs/design/prospecting/prototypes/drill-dock.html';
const v = process.argv[2]||'b';
const fs=require('fs');
(async () => {
  const dir=`gif-${v}`; fs.rmSync(dir,{recursive:true,force:true}); fs.mkdirSync(dir);
  const b = await chromium.launch({ executablePath: '/opt/pw-browsers/chromium' });
  const p = await b.newPage({ viewport:{width:640,height:900}, deviceScaleFactor:1 });
  await p.goto(F+'?v='+v); await p.waitForTimeout(700);
  await p.evaluate(()=>{ window.__manual(true);
    window.__setState({auto:false, trace:{c:[2,5],g:[5,2]}, mode:'plan', depth:0, t:0}); });
  const DT=1/16*2.2; let n=0;
  const stage=p.locator('.stage');
  for(let i=0;i<20;i++){ await p.evaluate(d=>window.__step(d),DT);
    await stage.screenshot({path:`${dir}/f${String(n++).padStart(3,'0')}.png`}); }
  await p.evaluate(()=>window.__setState({mode:'drill'}));
  for(let i=0;i<220;i++){
    await p.evaluate(d=>window.__step(d),DT);
    await stage.screenshot({path:`${dir}/f${String(n++).padStart(3,'0')}.png`});
    const g=await p.evaluate(()=>window.__get());
    if (g.mode==='done') break;
  }
  for(let i=0;i<14;i++){ await p.evaluate(d=>window.__step(d),DT);
    await stage.screenshot({path:`${dir}/f${String(n++).padStart(3,'0')}.png`}); }
  console.log(v, n, 'frames');
  await b.close();
})();
