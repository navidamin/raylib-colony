// Stitch variant renders into one contact sheet.
// Usage: node sheet.js <out.png> <label:prefix> [<label:prefix> ...]
const { chromium } = require('playwright');
const fs = require('fs');
const path = require('path');

const DIR = '/home/user/raylib-colony/build/drill';
const POSES = (process.env.POSES || 'cool,hot,cracked').split(',');

(async () => {
  const out = process.argv[2];
  const cols = process.argv.slice(3).map(a => {
    const i = a.indexOf(':');
    return { label: a.slice(0, i), prefix: a.slice(i + 1) };
  });

  const cells = [];
  for (const c of cols) {
    for (const pose of POSES) {
      const p = path.join(DIR, `${c.prefix}-${pose}.png`);
      cells.push({
        col: c.label, pose,
        data: fs.existsSync(p)
          ? 'data:image/png;base64,' + fs.readFileSync(p).toString('base64')
          : null,
      });
    }
  }

  const html = `<html><body style="margin:0;background:#0d1218;font:13px ui-monospace,monospace;color:#cfe0f5">
  <div style="display:grid;grid-template-columns:repeat(${cols.length},1fr);gap:10px;padding:10px">
    ${cols.map(c => `<div style="text-align:center;padding:6px;background:#18212c;
        border-radius:5px;letter-spacing:2px">${c.label}</div>`).join('')}
    ${POSES.map(pose => cols.map(c => {
      const cell = cells.find(x => x.col === c.label && x.pose === pose);
      return `<div style="background:#141c25;border-radius:5px;padding:5px;text-align:center">
        <div style="opacity:.55;padding:2px 0 5px">${pose}</div>
        ${cell.data ? `<img src="${cell.data}" style="width:100%;display:block">`
                    : `<div style="padding:60px;opacity:.4">missing</div>`}
      </div>`;
    }).join('')).join('')}
  </div></body></html>`;

  const tmp = path.join(DIR, '_sheet.html');
  fs.writeFileSync(tmp, html);

  const b = await chromium.launch({ executablePath: '/opt/pw-browsers/chromium' });
  const p = await b.newPage({ viewport: { width: 460 * cols.length, height: 900 } });
  await p.goto('file://' + tmp);
  await p.waitForTimeout(400);
  await p.screenshot({ path: out, fullPage: true });
  await b.close();
  fs.unlinkSync(tmp);
  console.log('wrote ' + out);
})();
