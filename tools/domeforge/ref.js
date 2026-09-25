#!/usr/bin/env node
// The reference half of the DomeForge diff: renders with the prototype's own JS.
// Same flags as domeforge_render (the port half), so both sides get one config.
//   node tools/domeforge/ref.js --kind unit|central|roads|ground|base [--scale S]
//        [--size N] [--color #rrggbb] [--socket-start DEG] [--socket-count N] --out file.png
const fs = require('fs'), zlib = require('zlib'), path = require('path');
const P = path.resolve(__dirname, '../../prototypes/dome-forge');
const DomeForge = require(P + '/dome-forge-engine.js');
const DomeForgeBase = require(P + '/dome-forge-base.js');

function crc32(buf) { let c, crc = 0xffffffff; for (let n = 0; n < buf.length; n++) { c = (crc ^ buf[n]) & 0xff; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; crc = (crc >>> 8) ^ c; } return (crc ^ 0xffffffff) >>> 0; }
function chunk(t, d) { const l = Buffer.alloc(4); l.writeUInt32BE(d.length); const td = Buffer.concat([Buffer.from(t), d]); const c = Buffer.alloc(4); c.writeUInt32BE(crc32(td)); return Buffer.concat([l, td, c]); }
function png(img) {
  const { width: w, height: h, data } = img, raw = Buffer.alloc((w * 4 + 1) * h);
  for (let y = 0; y < h; y++) { raw[y * (w * 4 + 1)] = 0; Buffer.from(data.buffer, data.byteOffset + y * w * 4, w * 4).copy(raw, y * (w * 4 + 1) + 1); }
  const ihdr = Buffer.alloc(13); ihdr.writeUInt32BE(w, 0); ihdr.writeUInt32BE(h, 4); ihdr[8] = 8; ihdr[9] = 6;
  return Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ihdr), chunk('IDAT', zlib.deflateSync(raw)), chunk('IEND', Buffer.alloc(0))]);
}

const a = {}; for (let i = 2; i < process.argv.length; i += 2) a[process.argv[i].replace(/^--/, '')] = process.argv[i + 1];
const kind = a.kind || 'unit', scale = a.scale ? parseFloat(a.scale) : 1;
const cfg = Object.assign({}, DomeForge.DEFAULTS, DomeForgeBase.DEFAULTS);
if (a.color) cfg.color = a.color;
if (kind === 'unit' || kind === 'central') {
  cfg[kind] = Object.assign({}, cfg[kind]);
  if (a.size) cfg[kind].size = parseInt(a.size);
  if (a['socket-start']) cfg[kind].socketStart = parseFloat(a['socket-start']);
  if (a['socket-count']) cfg[kind].socketCount = parseInt(a['socket-count']);
}
const t0 = Date.now();
let img;
if (kind === 'unit' || kind === 'central') img = DomeForge.render(cfg, kind);
else if (kind === 'base') img = DomeForgeBase.renderBase(cfg, DomeForge, { scale });
else {
  const lay = DomeForgeBase.layout(cfg, scale), W = Math.round(lay.A);
  img = kind === 'roads' ? DomeForgeBase.renderRoads(cfg, W, W, lay.prims, scale) : DomeForgeBase.renderGround(cfg, W, W, scale);
}
fs.writeFileSync(a.out || 'ref.png', png(img));
console.log(`ref ${kind}: ${img.width}x${img.height} in ${Date.now() - t0} ms -> ${a.out || 'ref.png'}`);
