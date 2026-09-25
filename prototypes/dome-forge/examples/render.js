#!/usr/bin/env node
/* Render sprites, the sheet or the whole base to PNG from the command line (no browser needed).
 *
 *   node examples/render.js unit                 -> unit.png
 *   node examples/render.js central              -> central.png
 *   node examples/render.js sheet                -> sheet.png
 *   node examples/render.js base [scale]         -> base.png   (scale 0..1, default 1)
 *   node examples/render.js unit '{"color":"#2f7fd6","hexCells":0.12}'   (any config overrides as JSON)
 *
 * Config JSON copied from the tool's "All parameters (JSON)" panel can be passed the same way.
 */
const fs = require('fs'), zlib = require('zlib'), path = require('path');
const DomeForge = require('../dome-forge-engine.js');
const DomeForgeBase = require('../dome-forge-base.js');

function crc32(buf) { let c, crc = 0xffffffff; for (let n = 0; n < buf.length; n++) { c = (crc ^ buf[n]) & 0xff; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; crc = (crc >>> 8) ^ c; } return (crc ^ 0xffffffff) >>> 0; }
function chunk(t, d) { const l = Buffer.alloc(4); l.writeUInt32BE(d.length); const td = Buffer.concat([Buffer.from(t), d]); const c = Buffer.alloc(4); c.writeUInt32BE(crc32(td)); return Buffer.concat([l, td, c]); }
function png(img) {
  const { width: w, height: h, data } = img, raw = Buffer.alloc((w * 4 + 1) * h);
  for (let y = 0; y < h; y++) { raw[y * (w * 4 + 1)] = 0; Buffer.from(data.buffer, data.byteOffset + y * w * 4, w * 4).copy(raw, y * (w * 4 + 1) + 1); }
  const ihdr = Buffer.alloc(13); ihdr.writeUInt32BE(w, 0); ihdr.writeUInt32BE(h, 4); ihdr[8] = 8; ihdr[9] = 6;
  return Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ihdr), chunk('IDAT', zlib.deflateSync(raw)), chunk('IEND', Buffer.alloc(0))]);
}

const what = process.argv[2] || 'unit';
let overrides = {}, scale = 1;
for (const a of process.argv.slice(3)) { if (a.trim().startsWith('{')) overrides = JSON.parse(a); else scale = parseFloat(a); }
const cfg = Object.assign({}, DomeForge.DEFAULTS, DomeForgeBase.DEFAULTS, overrides);

const t0 = Date.now();
let img;
if (what === 'sheet') img = DomeForge.renderSheet(cfg);
else if (what === 'base') img = DomeForgeBase.renderBase(cfg, DomeForge, { scale });
else img = DomeForge.render(cfg, what);
const out = path.resolve(`${what}.png`);
fs.writeFileSync(out, png(img));
console.log(`${what}: ${img.width}x${img.height} px in ${Date.now() - t0} ms -> ${out}`);
