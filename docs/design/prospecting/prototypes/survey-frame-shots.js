// Stage-0 harness for survey-dashboard.html: the frame, at both breakpoints.
//
// Proves what stage 0 exists to prove -- that the arrangement comes out of
// Layout(containerWidth) and the phone is a different one rather than a
// narrower one: desktop rects against DASH.layout, a module tab switching,
// the breakpoint crossing, and both transient panels opening, closing and
// excluding each other. Geometry is read from window.__L(), never guessed.
//
//   NODE_PATH=/opt/node22/lib/node_modules node survey-frame-shots.js
const { chromium } = require('playwright');
const F='file:///home/user/raylib-colony/docs/design/prospecting/prototypes/survey-dashboard.html';
const OUT='/tmp/claude-0/-home-user-raylib-colony/187272cf-7754-578f-9233-064508769c6d/scratchpad/shots2/';
(async () => {
  const b = await chromium.launch({ executablePath: '/opt/pw-browsers/chromium' });
  const errs=[];
  // ---- desktop ----
  const p = await b.newPage({ viewport:{width:1600,height:1200}, deviceScaleFactor:1.5 });
  p.on('pageerror',e=>errs.push('desk: '+e.message));
  await p.goto(F); await p.waitForTimeout(1200);
  console.log('desk', JSON.stringify(await p.evaluate(()=>{const l=window.__L();return {mode:l.mode,W:l.W,H:l.H,rack:l.rack,sheet:l.sheet};})));
  console.log('desk rects', JSON.stringify(await p.evaluate(()=>window.__L().rects)));
  await p.waitForTimeout(1500);
  await p.locator('#cv').screenshot({ path: OUT+'f_desk.png' });
  // module tab switch
  await p.evaluate(()=>{ const l=window.__L(), b=l.rects.bar; const r=document.getElementById('cv').getBoundingClientRect();
    const x=r.left+(b.x+b.w*0.5)/l.W*r.width, y=r.top+(b.y+b.h*0.5)/l.H*r.height;
    document.getElementById('cv').dispatchEvent(new PointerEvent('pointerdown',{clientX:x,clientY:y,bubbles:true})); });
  await p.waitForTimeout(300);
  console.log('after tab click, module =', await p.evaluate(()=>window.__L().module));
  await p.locator('#cv').screenshot({ path: OUT+'f_desk_tab.png' });
  await p.close();
  // ---- phone ----
  const q = await b.newPage({ viewport:{width:390,height:844}, deviceScaleFactor:3, isMobile:true, hasTouch:true });
  q.on('pageerror',e=>errs.push('phone: '+e.message));
  await q.goto(F); await q.waitForTimeout(1200);
  const L = await q.evaluate(()=>window.__L());
  console.log('phone', JSON.stringify({mode:L.mode,W:L.W,H:L.H,rack:L.rack,sheet:L.sheet,tabs:L.tabs.map(t=>t.name)}));
  await q.waitForTimeout(1500);   // many frames in: the dirty-rect blit must not have eaten the rest
  await q.locator('#cv').screenshot({ path: OUT+'f_phone.png' });
  const tap = async (name) => { const l=await q.evaluate(()=>window.__L());
    const tb=l.tabs.find(t=>t.name===name); const r=await q.locator('#cv').boundingBox();
    await q.mouse.click(r.x+(tb.x+tb.w/2)/l.W*r.width, r.y+(tb.y+tb.h/2)/l.H*r.height); };
  await tap('TOOLS'); await q.waitForTimeout(90);
  await q.locator('#cv').screenshot({ path: OUT+'f_phone_mid.png' });
  await q.waitForTimeout(500);
  console.log('after TOOLS', JSON.stringify(await q.evaluate(()=>({r:window.__L().rack,s:window.__L().sheet}))));
  await q.locator('#cv').screenshot({ path: OUT+'f_phone_rack.png' });
  await tap('DRILL'); await q.waitForTimeout(600);
  console.log('after DRILL', JSON.stringify(await q.evaluate(()=>({r:window.__L().rack,s:window.__L().sheet}))));
  await q.locator('#cv').screenshot({ path: OUT+'f_phone_drill.png' });
  console.log(errs.length?('ERRORS '+errs.join(' | ')):'no errors');
  await b.close();
})();
