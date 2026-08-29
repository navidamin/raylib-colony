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
// Usage:
//   node tools/shell-test/shell_test.js            # assert, exit non-zero on failure
//   node tools/shell-test/shell_test.js --shots    # also write PNGs to build/shell-test/
//
// Requires NODE_PATH to find the preinstalled Playwright:
//   NODE_PATH=/opt/node22/lib/node_modules node tools/shell-test/shell_test.js
//
// See docs/web-deploy-mobile.md for the full background.

const fs = require('fs');
const path = require('path');
const { chromium } = require('playwright');

const REPO_ROOT = path.resolve(__dirname, '../..');
const SHELL_PATH = path.join(REPO_ROOT, 'src/minshell.html');
const OUT_DIR = path.join(REPO_ROOT, 'build/shell-test');

const GAME_W = 1280;
const GAME_H = 720;

// Viewports to check. iPhone 16 portrait/landscape and an iPad.
const VIEWPORTS = [
    { name: 'phone-portrait',  width: 402, height: 714,  dpr: 3 },
    { name: 'phone-landscape', width: 714, height: 402,  dpr: 3 },
    { name: 'tablet',          width: 1024, height: 768, dpr: 2 },
];

// Desktop viewports for the CSS-size check below. Upscales are INTEGER-ONLY
// (a fractional upscale of the fixed 1280x720 buffer renders visibly soft --
// Firefox on Windows at 1920x1200 was the report), so the first two sit at
// native 1280 and the 4K case exercises the 2x step. The stripped-style
// check still matters at every size: a collapse to the natural 1280 cancels
// the pointer division instead of merely shifting it.
const DESKTOP_VIEWPORTS = [
    { name: 'desktop-1920',  width: 1920, height: 919 },
    { name: 'desktop-1680',  width: 1680, height: 1050 },
    { name: 'desktop-4k',    width: 3840, height: 2010 },
];

// The shell's fit law: continuous downscale, whole-step upscale.
function fitScale(vw, vh) {
    let scale = Math.min(vw / GAME_W, vh / GAME_H);
    if (scale > 1.0) scale = Math.max(1.0, Math.floor(scale));
    return scale;
}

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

    const browser = await chromium.launch({
        executablePath: '/opt/pw-browsers/chromium-1194/chrome-linux/chrome',
    });

    let failures = 0;

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

    // -------------------------------------------------------------------
    // The canvas must keep its fitted CSS size even while the inline style
    // is stripped.
    //
    // raylib converts a pointer event with
    //   position.x = (targetX / cssWidth) * screen.width
    // reading cssWidth from getBoundingClientRect INSIDE the handler
    // (rcore_web_emscripten.c). Emscripten's updateCanvasDimensions calls
    // style.removeProperty('width'), which strips even an important inline
    // style; with no inline width the canvas collapses to its natural 1280,
    // the division cancels, and the game silently receives unscaled
    // coordinates -- an error growing with distance from the origin.
    //
    // Firefox on Windows hit this in the wild while every after-the-fact
    // reading looked healthy, because the MutationObserver repairs the style
    // before anything else can measure it. So measure the way raylib does:
    // strip and read in ONE synchronous block, leaving no room for repair.
    // A stylesheet rule is what survives this; an inline style cannot.
    // -------------------------------------------------------------------
    for (const vp of DESKTOP_VIEWPORTS) {
        const page = await browser.newPage({ viewport: { width: vp.width, height: vp.height } });
        await page.goto('file://' + pagePath);
        await page.waitForTimeout(4000);

        const measured = await page.evaluate(() => {
            const c = document.getElementById('canvas');
            c.style.removeProperty('width');
            c.style.removeProperty('height');
            return Math.round(c.getBoundingClientRect().width);
        });

        const wantFit = Math.floor(GAME_W * fitScale(vp.width, vp.height));
        const ok = Math.abs(measured - wantFit) <= 2;
        if (!ok) failures++;

        console.log(`${ok ? 'PASS' : 'FAIL'}  ${vp.name.padEnd(16)} ` +
                    `css-width-while-stripped=${measured} (want ${wantFit})`);
        if (!ok) {
            console.log(`      canvas collapsed toward its natural ${GAME_W}px;`);
            console.log('      pointer coordinates will reach the game unscaled');
        }
        await page.close();
    }

    await browser.close();

    console.log(failures === 0
        ? '\nAll viewports OK.'
        : `\n${failures} check(s) failed.`);
    process.exit(failures === 0 ? 0 : 1);
}

main().catch((err) => {
    console.error(err);
    process.exit(1);
});
