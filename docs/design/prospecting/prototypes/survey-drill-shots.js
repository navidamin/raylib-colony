// Stage-2 harness for survey-dashboard.html: the drill on a camera that pitches.
//
// The acceptance test for the stage the plan is arranged around. Two parts:
//
//   1. PICK ROUND-TRIP. Click a screen point over the cap, read back which
//      lattice node the rig would stand on, project that node and compare.
//      12 camera poses x 3 points. The error must stay under one lattice cell
//      or the rig does not land where the pointer was.
//
//   2. THE FULL CYCLE at pitch 0.20, 0.45 and 1.20 -- aim, spud, await, cut,
//      trip out, bore marker -- with stills at each step. World-vertical
//      projects to screen-vertical at every camera, so the rig should stand
//      upright in all of them; the stills are how that is checked.
//
// The cap is found by projecting interface 0's four corners, not assumed:
// at shallow pitch the cap is a thin diamond near the top of the block and a
// point that looks central is on the front wall.
//
//   NODE_PATH=/opt/node22/lib/node_modules node survey-drill-shots.js
const { chromium } = require('playwright');
const F='file:///home/user/raylib-colony/docs/design/prospecting/prototypes/survey-dashboard.html';
const OUT='/tmp/claude-0/-home-user-raylib-colony/187272cf-7754-578f-9233-064508769c6d/scratchpad/shots2/';
(async () => {
  const b = await chromium.launch({ executablePath: '/opt/pw-browsers/chromium' });
  const p = await b.newPage({ viewport:{width:1600,height:1200}, deviceScaleFactor:2 });
  const errs=[]; p.on('pageerror',e=>errs.push(e.message));
  await p.goto(F); await p.waitForTimeout(1200);
  const L = await p.evaluate(()=>window.__L());
  const bx = await p.locator('#cv').boundingBox(), k = bx.width/L.W;
  const pt = (x,y) => ({ x: bx.x + x*k, y: bx.y + y*k });
  const park = async (yaw,pi) => { await p.evaluate(([y,q])=>{ const s=window.__blk(); s.auto=false; s.yaw=y; s.pitch=q; }, [yaw,pi]); await p.waitForTimeout(120); };
  const capCentre = () => p.evaluate(()=>{ const m=window.__model();
    const c=[[0,0],[m.NX,0],[m.NX,m.NZ],[0,m.NZ]].map(([i,j])=>m.cam.project(
      [(i/m.NX-0.5)*m.W, -(window.__surf(i,j)/window.__col())*m.D + m.offY(0), (j/m.NZ-0.5)*m.W]));
    return [c.reduce((a,q)=>a+q[0],0)/4, c.reduce((a,q)=>a+q[1],0)/4]; });
  const crop = n => p.screenshot({ path: OUT+n, clip:{ x:bx.x+L.block.grab[0]*k, y:bx.y+L.block.grab[1]*k,
      width:(L.block.grab[2]-L.block.grab[0])*k, height:(L.block.grab[3]-L.block.grab[1])*k } });

  console.log('--- pick round-trip, 12 camera poses x 3 points on the cap ---');
  let worst = 0, n = 0;
  for (const yaw of [0, 1.6, 3.1, 4.7]) for (const pi of [0.20, 0.45, 1.20]) {
    await park(yaw, pi);
    const [cx,cy] = await capCentre();
    for (const [ox,oy] of [[0,0],[-70,0],[60,0]]) {
      const q = await p.evaluate(([x,y])=>window.__pick(x,y), [cx+ox, cy+oy]);
      if (!q.on) continue;
      const back = await p.evaluate(([i,j])=>{ const m=window.__model();
        const r=m.cam.project([(i/m.NX-0.5)*m.W, -(window.__surf(i,j)/window.__col())*m.D + m.offY(0), (j/m.NZ-0.5)*m.W]);
        return [r[0], r[1]]; }, [q.i, q.j]);
      worst = Math.max(worst, Math.hypot(back[0]-(cx+ox), back[1]-(cy+oy))); n++;
    }
  }
  console.log(`  ${n} picks, worst round-trip ${worst.toFixed(1)} px (one lattice cell ~ ${(740*0.36/28).toFixed(1)} px)`);

  console.log('--- the full cycle, three pitches ---');
  for (const [name,pi] of [['flat',0.20],['home',0.45],['steep',1.20]]) {
    await p.evaluate(()=>{ window.__reset(); }); await p.waitForTimeout(120); await park(-0.1, pi);
    await p.evaluate(()=>window.__arm(1));
    const [cx,cy] = await capCentre();
    await p.mouse.move(pt(cx,cy).x, pt(cx,cy).y); await p.waitForTimeout(220);
    await crop(`d_${name}_aim.png`);
    await p.mouse.down(); await p.mouse.up(); await p.waitForTimeout(420);
    await crop(`d_${name}_spud.png`);
    await p.waitForTimeout(600);
    const a1 = await p.evaluate(()=>window.__drill());
    await p.mouse.down(); await p.mouse.up(); await p.waitForTimeout(1100);
    const a2 = await p.evaluate(()=>window.__drill());
    await crop(`d_${name}_cut.png`);
    await p.waitForTimeout(3600);
    const a3 = await p.evaluate(()=>window.__drill());
    await crop(`d_${name}_done.png`);
    console.log(` ${name.padEnd(6)} await=${a1.mode}/${a1.curM}m  cut=${a2.mode}/${a2.curM}m sink=${a2.sink} spatter=${a2.spatter}  end=${a3.mode} bores=${a3.bores} pxPerM=${a3.pxPerM}`);
  }
  console.log(errs.length?('ERRORS '+errs.join(' | ')):'no errors');
  await b.close();
})();
