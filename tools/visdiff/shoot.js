// Screenshots the reference canvas at design size. NODE_PATH=/opt/node22/lib/node_modules
const { chromium } = require('playwright');
const path = require('path');
(async () => {
  const args = Object.fromEntries(process.argv.slice(2).map(a => a.split('=')));
  const out = args.out || '/tmp/ref.png';
  const qs = Object.entries(args).filter(([k]) => k !== 'out')
                   .map(([k, v]) => `${k}=${v}`).join('&');
  const url = 'file://' + path.resolve(__dirname, 'ref.html') + (qs ? '?' + qs : '');
  const b = await chromium.launch({ executablePath: '/opt/pw-browsers/chromium' });
  const p = await b.newPage({ viewport: { width: 1600, height: 1300 }, deviceScaleFactor: 1 });
  const errs = []; p.on('pageerror', e => errs.push(e.message));
  await p.goto(url);
  await p.waitForFunction(() => window.__ready === true || window.__fontError,
                          { timeout: 15000 });
  const fontErr = await p.evaluate(() => window.__fontError || null);
  if (fontErr) { console.error('FONT ERROR: ' + fontErr); await b.close(); process.exit(3); }
  await p.waitForTimeout(150);
  await p.locator('#c').screenshot({ path: out });
  if (errs.length) console.error('PAGE ERRORS: ' + errs.join(' | '));
  await b.close();
  console.log('ref -> ' + out);
})();
