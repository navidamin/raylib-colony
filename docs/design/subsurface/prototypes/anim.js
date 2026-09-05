// Capture the drill cycle as an animated GIF: rotating, drilling in, getting red,
// then a peck that trips back off the face and cools -- which is what makes it loop.
// Usage: node anim.js <file.html> <outDir> [scale]
const { chromium } = require('playwright');
const fs = require('fs');

const FPS = 20, DUR = 9.2;            // seconds of simulated time
const DT  = 1 / FPS;

(async () => {
  const [file, outDir, scaleArg] = process.argv.slice(2);
  fs.rmSync(outDir, { recursive: true, force: true });
  fs.mkdirSync(outDir, { recursive: true });

  const b = await chromium.launch({ executablePath: '/opt/pw-browsers/chromium' });
  const p = await b.newPage({ viewport: { width: 1120, height: 1080 },
                              deviceScaleFactor: Number(scaleArg || 2) });
  const errs = [];
  p.on('pageerror', e => errs.push('PAGEERROR: ' + e.message));
  await p.goto('file://' + file);
  await p.waitForTimeout(500);

  await p.evaluate(() => {
    window.__manual(true);
    window.__setState({ depthM: 52, heat: 0.05, rpm: 0.62, feed: 0.55,
                        auto: true, peck: false, cracks: [], t: 0 });
  });

  const box = await p.locator('.stage').boundingBox();
  const n = Math.round(DUR * FPS);
  for (let i = 0; i < n; i++) {
    const t = i * DT;
    // peck window: back off the face, let it cool, then bite again
    const peck = t > 6.0 && t < 8.4;
    await p.evaluate(v => window.__setState({ peck: v }), peck);
    await p.evaluate(dt => window.__step(dt), DT);
    const bitY = await p.evaluate(() => window.__bitY());
    const top = Math.round(Math.max(0, Math.min(880 - 560, bitY - 430)) / 2) * 2;
    await p.screenshot({ path: `${outDir}/f${String(i).padStart(3,'0')}.png`,
      clip: { x: box.x + 214, y: box.y + top, width: 172, height: 560 } });
  }
  console.log(errs.length ? errs.join('\n') : `no errors, ${n} frames`);
  await b.close();
})();
