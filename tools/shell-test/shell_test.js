// Verifies src/minshell.html canvas fitting without a phone.
//
// The web shell has to keep a fixed 1280x720 framebuffer while scaling the
// canvas to fit the viewport -- and it has to win against Emscripten/raylib,
// which write their own inline canvas styles (with 'important' priority) and
// mirror the CSS size back into the framebuffer attributes. That fight is
// what broke the mobile build; this harness reproduces it headlessly.
//
// It builds a page from the REAL shell, substituting a hostile stand-in for
// the wasm runtime, then asserts at phone/tablet viewports that:
//   1. the framebuffer stays 1280x720
//   2. the rendered canvas fits inside the viewport
//
// A second scenario covers the opposite contract: a page that sets
// window.COLONY_CANVAS_FREE (lunar_map) sizes its own framebuffer to the
// viewport, and the shell must leave it alone -- pinning it to 1280x720
// was what drew that page's frame into the wrong-sized buffer.
//
// Usage:
//   node tools/shell-test/shell_test.js            # assert, exit non-zero on failure
//   node tools/shell-test/shell_test.js --shots    # also write PNGs to build/shell-test/
//
// Needs Playwright on the module path. The dev container has it
// preinstalled (NODE_PATH=/opt/node22/lib/node_modules, browser under
// /opt/pw-browsers); elsewhere `npm install playwright && npx playwright
// install chromium` in any directory and point NODE_PATH at its
// node_modules. SHELL_TEST_CHROME overrides the browser executable.
//
// See docs/web-deploy-mobile.md for the full background.

const fs = require('fs');
const path = require('path');
const { chromium } = require('playwright');

const REPO_ROOT = path.resolve(__dirname, '../..');
const SHELL_PATH = path.join(REPO_ROOT, 'src/minshell.html');
const OUT_DIR = process.env.SHELL_TEST_OUT || path.join(REPO_ROOT, 'build/shell-test');

const GAME_W = 1280;
const GAME_H = 720;

// Viewports to check. iPhone 16 portrait/landscape and an iPad.
const VIEWPORTS = [
    { name: 'phone-portrait',  width: 402, height: 714,  dpr: 3 },
    { name: 'phone-landscape', width: 714, height: 402,  dpr: 3 },
    { name: 'tablet',          width: 1024, height: 768, dpr: 2 },
];

// Stands in for the Emscripten/raylib runtime, behaving as badly as the real
// one did on iOS: repaints constantly, and repeatedly mirrors the CSS size
// into the framebuffer attributes while stomping the style with 'important'.
const HOSTILE_RUNTIME = `<script>
var c = document.getElementById('canvas');
c.width = ${GAME_W}; c.height = ${GAME_H};

function paint() {
    var ctx = c.getContext('2d');
    ctx.fillStyle = '#0a0f1c'; ctx.fillRect(0, 0, c.width, c.height);
    ctx.strokeStyle = '#50e1ff'; ctx.lineWidth = 8;
    ctx.strokeRect(4, 4, c.width - 8, c.height - 8);
    ctx.fillStyle = '#50e1ff'; ctx.font = '48px sans-serif';
    ctx.fillText('TL', 30, 70);
    ctx.fillText('BR', c.width - 110, c.height - 30);
    ctx.fillText('FIT TEST', c.width / 2 - 110, c.height / 2);
}
setInterval(paint, 100);

function shrink() {
    var r = c.getBoundingClientRect();
    if (r.width && r.height) {
        c.width = Math.round(r.width);
        c.height = Math.round(r.height);
    }
    c.style.setProperty('width', c.width + 'px', 'important');
    c.style.setProperty('height', c.height + 'px', 'important');
}
shrink();
[400, 1200, 2500].forEach(function(ms) { setTimeout(shrink, ms); });
</script>`;

// Stands in for lunar_map: claims the canvas before its first frame and
// keeps the framebuffer at the viewport size, as SyncWebCanvasToViewport
// does. The shell must not pin it back.
const FREE_RUNTIME = `<script>
window.COLONY_CANVAS_FREE = true;
var c = document.getElementById('canvas');
function sync() {
    var w = document.documentElement.clientWidth, h = document.documentElement.clientHeight;
    if (c.width !== w) c.width = w;
    if (c.height !== h) c.height = h;
    c.style.setProperty('max-width', 'none', 'important');
    c.style.setProperty('max-height', 'none', 'important');
    c.style.setProperty('width', w + 'px', 'important');
    c.style.setProperty('height', h + 'px', 'important');
}
sync();
[400, 1200, 2500].forEach(function(ms) { setTimeout(sync, ms); });
function paint() {
    var ctx = c.getContext('2d');
    ctx.fillStyle = '#0a0f1c'; ctx.fillRect(0, 0, c.width, c.height);
    ctx.strokeStyle = '#ffb050'; ctx.lineWidth = 8;
    ctx.strokeRect(4, 4, c.width - 8, c.height - 8);
    ctx.fillStyle = '#ffb050'; ctx.font = '48px sans-serif';
    ctx.fillText('FREE', 30, 70);
}
setInterval(paint, 100);
</script>`;

const CONTAINER_CHROME = '/opt/pw-browsers/chromium-1194/chrome-linux/chrome';

async function main() {
    const wantShots = process.argv.includes('--shots');

    const shell = fs.readFileSync(SHELL_PATH, 'utf8');
    if (!shell.includes('{{{ SCRIPT }}}')) {
        console.error('FAIL: {{{ SCRIPT }}} placeholder missing from minshell.html');
        process.exit(1);
    }

    fs.mkdirSync(OUT_DIR, { recursive: true });
    const pagePath = path.join(OUT_DIR, 'index.html');
    fs.writeFileSync(pagePath, shell.replace('{{{ SCRIPT }}}', HOSTILE_RUNTIME));
    const freePath = path.join(OUT_DIR, 'free.html');
    fs.writeFileSync(freePath, shell.replace('{{{ SCRIPT }}}', FREE_RUNTIME));

    const chromePath = process.env.SHELL_TEST_CHROME
        || (fs.existsSync(CONTAINER_CHROME) ? CONTAINER_CHROME : undefined);
    const browser = await chromium.launch(chromePath ? { executablePath: chromePath } : {});

    let failures = 0;

    // Scenario 2: the free canvas. The framebuffer must track the
    // viewport, not be pinned.
    for (const vp of VIEWPORTS) {
        const page = await browser.newPage({
            viewport: { width: vp.width, height: vp.height },
            deviceScaleFactor: vp.dpr,
            isMobile: true,
            hasTouch: true,
        });
        await page.goto('file://' + freePath + '?debug=1');
        await page.waitForTimeout(4000);
        const state = await page.evaluate(() => {
            const c = document.getElementById('canvas');
            const r = c.getBoundingClientRect();
            return { fbW: c.width, fbH: c.height,
                     rectW: Math.round(r.width), rectH: Math.round(r.height) };
        });
        const tracks = (state.fbW === vp.width && state.fbH === vp.height);
        const fits = (state.rectW <= vp.width + 1 && state.rectH <= vp.height + 1);
        const ok = tracks && fits;
        if (!ok) failures++;
        console.log(`${ok ? 'PASS' : 'FAIL'}  free ${vp.name.padEnd(16)} ` +
                    `fb=${state.fbW}x${state.fbH} rect=${state.rectW}x${state.rectH} ` +
                    `viewport=${vp.width}x${vp.height}`);
        if (!tracks) console.log(`      framebuffer pinned or stale (want ${vp.width}x${vp.height})`);
        if (wantShots) {
            await page.screenshot({ path: path.join(OUT_DIR, `free-${vp.name}.png`) });
        }
        await page.close();
    }

    for (const vp of VIEWPORTS) {
        const page = await browser.newPage({
            viewport: { width: vp.width, height: vp.height },
            deviceScaleFactor: vp.dpr,
            isMobile: true,
            hasTouch: true,
        });

        // ?debug=1 turns on the shell's geometry badge, so a failure report
        // includes the live numbers.
        await page.goto('file://' + pagePath + '?debug=1');
        // Outlast the last hostile stomp (2500ms) plus the shell's poll
        await page.waitForTimeout(4000);

        const state = await page.evaluate(() => {
            const c = document.getElementById('canvas');
            const r = c.getBoundingClientRect();
            const badge = document.getElementById('shellDebug');
            return {
                fbW: c.width, fbH: c.height,
                rectW: Math.round(r.width), rectH: Math.round(r.height),
                badge: badge ? badge.textContent : '(no badge)',
            };
        });

        const framebufferPinned = (state.fbW === GAME_W && state.fbH === GAME_H);
        const fits = (state.rectW <= vp.width + 1 && state.rectH <= vp.height + 1);
        const ok = framebufferPinned && fits;
        if (!ok) failures++;

        console.log(`${ok ? 'PASS' : 'FAIL'}  ${vp.name.padEnd(16)} ` +
                    `fb=${state.fbW}x${state.fbH} rect=${state.rectW}x${state.rectH} ` +
                    `viewport=${vp.width}x${vp.height}`);
        if (!ok) {
            if (!framebufferPinned) console.log(`      framebuffer not pinned (want ${GAME_W}x${GAME_H})`);
            if (!fits) console.log('      canvas overflows the viewport');
            console.log(`      badge: ${state.badge}`);
        }

        if (wantShots) {
            await page.screenshot({ path: path.join(OUT_DIR, `${vp.name}.png`) });
        }
        await page.close();
    }

    await browser.close();

    console.log(failures === 0
        ? '\nAll viewports OK.'
        : `\n${failures} viewport(s) failed.`);
    process.exit(failures === 0 ? 0 : 1);
}

main().catch((err) => {
    console.error(err);
    process.exit(1);
});
