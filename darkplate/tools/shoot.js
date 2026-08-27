/* Render the gfx stage headlessly and write PNGs. §10 capture craft:
   deterministic stepping, zoom to judge, never claim a visual result you have
   not looked at.

     node darkplate/tools/shoot.js <page.html> <outDir> [poses]

   Needs playwright:  npm i playwright   (or set NODE_PATH to an install)   */
const path = require("path");
const fs = require("fs");
let chromium;
try { ({ chromium } = require("playwright")); }
catch (e) { console.error("playwright not found — npm i playwright, or set NODE_PATH"); process.exit(2); }

const PAGE = path.resolve(process.argv[2] || "darkplate/assembler.html");
const OUT  = path.resolve(process.argv[3] || "build/darkplate");

const POSES = [
    { name: "idle",    s: { work: 0.00, heat: 0.00, t: 0.00, run: false } },
    { name: "working", s: { work: 0.75, heat: 0.26, t: 0.909, run: false } },
    { name: "hot",     s: { work: 1.00, heat: 0.88, t: 2.727, run: false } },
];
const PRESETS = ["studio34", "iso21", "shallow"];

(async () => {
    fs.mkdirSync(OUT, { recursive: true });
    const b = await chromium.launch({ executablePath: "/opt/pw-browsers/chromium" });
    const p = await b.newPage({ viewport: { width: 1240, height: 1400 }, deviceScaleFactor: 2 });
    const errs = [];
    p.on("pageerror", e => errs.push("PAGEERROR: " + e.message));
    p.on("console", m => { if (m.type() === "error") errs.push("CONSOLE: " + m.text()); });
    await p.goto("file://" + PAGE);
    await p.waitForTimeout(700);

    /* fixed dt stepping, so the animated parts land in the same place every run */
    await p.evaluate(() => window.__manual(true));

    for (const pose of POSES) {
        await p.evaluate(s => window.__setState(s), pose.s);
        await p.waitForTimeout(140);
        await p.locator(".stage").screenshot({ path: path.join(OUT, `assembler-${pose.name}.png`) });
    }
    /* the icon strip: does it survive at 32 px */
    await p.evaluate(() => window.__setState({ work: 0.75, heat: 0.22, t: 0.909 }));
    await p.waitForTimeout(140);
    await p.locator(".strip").screenshot({ path: path.join(OUT, "assembler-icons.png") });

    /* projection comparison */
    for (const pr of PRESETS) {
        await p.evaluate(pp => window.__setState({ preset: pp, work: 0.5, heat: 0.15, t: 1.1 }), pr);
        await p.waitForTimeout(140);
        await p.locator(".stage").screenshot({ path: path.join(OUT, `assembler-proj-${pr}.png`) });
    }
    await p.evaluate(() => window.__setState({ preset: "studio34" }));

    /* §10 zoom to judge — at full frame the door reveal is four pixels */
    const box = await p.locator(".stage").boundingBox();
    const crops = [
        { name: "z-door",  x: 0.20, y: 0.46, w: 0.62, h: 0.42, s: { work: 0.85, heat: 0.26, t: 0.909 } },
        { name: "z-cap",   x: 0.22, y: 0.05, w: 0.58, h: 0.38, s: { work: 0.3, heat: 0.1, t: 0.9 } },
        { name: "z-chamf", x: 0.05, y: 0.18, w: 0.50, h: 0.42, s: { work: 0.3, heat: 0.1, t: 0.9 } },
    ];
    for (const c of crops) {
        await p.evaluate(s => window.__setState(s), c.s);
        await p.waitForTimeout(140);
        await p.screenshot({ path: path.join(OUT, `assembler-${c.name}.png`), clip: {
            x: box.x + box.width * c.x, y: box.y + box.height * c.y,
            width: box.width * c.w, height: box.height * c.h } });
    }
    console.log(errs.length ? errs.join("\n") : "no errors");
    console.log("wrote " + OUT);
    await b.close();
})();
