#!/usr/bin/env node
//
// Headless renderer for regolith_craters.html.
//
// Slices the CHAIN block out of the bench and runs it in Node, so the
// PNGs written here are made by the same code the page draws with --
// which is the only way to look at a change without a browser.
//
//   node prototypes/planet_visuals/regolith_craters_render.mjs
//   node ... --region tycho --span 5 --res 400
//   node ... --span 1 --no-craters --out build/off
//   node ... --lat 32.7176 --lon -15.5019 --block anchor.bin --spans 100,25,5
//
// Options:
//   --out DIR        where the PNGs go            (default build/regolith)
//   --region KEY     one of the carried regions   (default the first)
//   --list           print the carried regions and stop
//   --lat/--lon D    an explicit window centre, overriding the region
//   --span KM        the window                   (default 25)
//   --window         2-step ladder (100 km, then the window) rather than stepped
//   --spans A,B,C    an explicit km ladder, overriding --span
//   --res N          pixels per level              (default 320)
//   --no-craters     the same ground with the craters off (A/B)
//   --last-only      carve only the deepest level, not every one
//   --set K=V,...    override any tune or crater lever
//   --block FILE     raw 8-bit block + FILE.json instead of a carried one
//   --dump-lum FILE  write the last level's luminance as raw float32

import fs from "node:fs";
import path from "node:path";
import zlib from "node:zlib";
import { fileURLToPath } from "node:url";

const HERE = path.dirname(fileURLToPath(import.meta.url));
const HTML = path.join(HERE, "regolith_craters.html");

/* --- minimal PNG in and out ---------------------------------------- */

const CRC_TABLE = (() => {
  const t = new Int32Array(256);
  for (let n = 0; n < 256; n++){
    let c = n;
    for (let k = 0; k < 8; k++) c = (c & 1) ? (0xEDB88320 ^ (c >>> 1)) : (c >>> 1);
    t[n] = c;
  }
  return t;
})();
function Crc32(buf){
  let c = 0xFFFFFFFF;
  for (let i = 0; i < buf.length; i++) c = CRC_TABLE[(c ^ buf[i]) & 0xFF] ^ (c >>> 8);
  return (c ^ 0xFFFFFFFF) >>> 0;
}
function Chunk(type, data){
  const out = Buffer.alloc(12 + data.length);
  out.writeUInt32BE(data.length, 0);
  out.write(type, 4, "ascii");
  data.copy(out, 8);
  out.writeUInt32BE(Crc32(out.subarray(4, 8 + data.length)), 8 + data.length);
  return out;
}
// 8-bit RGB, no interlace.
function WritePng(file, rgb, w, h){
  const raw = Buffer.alloc((w * 3 + 1) * h);
  for (let y = 0; y < h; y++){
    raw[y * (w * 3 + 1)] = 0;
    Buffer.from(rgb.buffer, rgb.byteOffset + y * w * 3, w * 3)
          .copy(raw, y * (w * 3 + 1) + 1);
  }
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(w, 0); ihdr.writeUInt32BE(h, 4);
  ihdr[8] = 8; ihdr[9] = 2; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
  fs.writeFileSync(file, Buffer.concat([
    Buffer.from([0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A]),
    Chunk("IHDR", ihdr),
    Chunk("IDAT", zlib.deflateSync(raw, { level: 9 })),
    Chunk("IEND", Buffer.alloc(0))
  ]));
}
// 8-bit grayscale, no interlace -- what the embedded block is.
function ReadGrayPng(buf){
  let p = 8, w = 0, h = 0, bitDepth = 0, colorType = 0;
  const idat = [];
  while (p < buf.length){
    const len = buf.readUInt32BE(p);
    const type = buf.toString("ascii", p + 4, p + 8);
    const data = buf.subarray(p + 8, p + 8 + len);
    if (type === "IHDR"){
      w = data.readUInt32BE(0); h = data.readUInt32BE(4);
      bitDepth = data[8]; colorType = data[9];
    } else if (type === "IDAT") idat.push(data);
    else if (type === "IEND") break;
    p += 12 + len;
  }
  if (bitDepth !== 8 || colorType !== 0)
    throw new Error(`block PNG must be 8-bit grayscale (got depth ${bitDepth} type ${colorType})`);
  const raw = zlib.inflateSync(Buffer.concat(idat));
  const out = new Uint8Array(w * h);
  const stride = w;
  for (let y = 0; y < h; y++){
    const filter = raw[y * (stride + 1)];
    const row = raw.subarray(y * (stride + 1) + 1, y * (stride + 1) + 1 + stride);
    for (let x = 0; x < stride; x++){
      const a = x > 0 ? out[y * w + x - 1] : 0;
      const b = y > 0 ? out[(y - 1) * w + x] : 0;
      const c = (x > 0 && y > 0) ? out[(y - 1) * w + x - 1] : 0;
      let v = row[x];
      if (filter === 1) v += a;
      else if (filter === 2) v += b;
      else if (filter === 3) v += (a + b) >> 1;
      else if (filter === 4){
        const pp = a + b - c, pa = Math.abs(pp - a), pb = Math.abs(pp - b),
              pc = Math.abs(pp - c);
        v += (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
      }
      out[y * w + x] = v & 0xFF;
    }
  }
  return { data: out, w, h };
}

/* --- load the bench ------------------------------------------------- */

const html = fs.readFileSync(HTML, "utf8");
const B = "/* ===== CHAIN BEGIN", E = "/* ===== CHAIN END";
const b0 = html.indexOf(B), e0 = html.indexOf(E);
if (b0 < 0 || e0 < 0) throw new Error("CHAIN markers not found in " + HTML);
const TC = new Function(html.slice(b0, e0) + "\nreturn TerrainChain;")();

const PAYLOAD = JSON.parse(
  html.match(/<script id="wac-blocks"[^>]*>([\s\S]*?)<\/script>/)[1]);

function CarriedBlock(region){
  const png = ReadGrayPng(Buffer.from(region.png, "base64"));
  if (png.w !== PAYLOAD.size || png.h !== PAYLOAD.size)
    throw new Error("block PNG size disagrees with its registration");
  return { data: png.data, w: PAYLOAD.size, h: PAYLOAD.size,
           x0: region.x0, y0: region.y0,
           wacW: PAYLOAD.wacW, wacH: PAYLOAD.wacH };
}

/* --- arguments ------------------------------------------------------ */

const argv = process.argv.slice(2);
const Arg = (k, d) => { const i = argv.indexOf("--" + k); return i < 0 ? d : argv[i + 1]; };
const Flag = k => argv.includes("--" + k);

if (Flag("list")){
  for (const r of PAYLOAD.regions)
    console.log(`${r.key.padEnd(13)} ${r.lat.toFixed(2).padStart(7)} `
      + `${r.lon.toFixed(2).padStart(8)}   ${r.name} — ${r.note}`);
  process.exit(0);
}

const outDir = Arg("out", path.join(HERE, "..", "..", "build", "regolith"));
fs.mkdirSync(outDir, { recursive: true });
const res = parseInt(Arg("res", "320"), 10);

const regionKey = Arg("region", PAYLOAD.regions[0].key);
const region = PAYLOAD.regions.find(r => r.key === regionKey);
if (!region) throw new Error(`no carried region "${regionKey}" (try --list)`);

// The crater population is pinned to the region's own origin.
const originLat = region.lat - region.sKm / TC.MOON_KM_PER_DEG;
const originLon = region.lon + region.eKm /
  (TC.MOON_KM_PER_DEG * Math.cos(originLat * TC.DEG2RAD));

let block;
const blockFile = Arg("block", null);
if (blockFile){
  const meta = JSON.parse(fs.readFileSync(blockFile + ".json", "utf8"));
  block = { data: new Uint8Array(fs.readFileSync(blockFile)), w: meta.w, h: meta.h,
            x0: meta.x0, y0: meta.y0, wacW: meta.wacW, wacH: meta.wacH };
} else {
  block = CarriedBlock(region);
}

const lat = parseFloat(Arg("lat", String(originLat)));
const lon = parseFloat(Arg("lon", String(originLon)));
const span = parseFloat(Arg("span", "25"));
const spans = Arg("spans", null)
  ? Arg("spans").split(",").map(Number)
  : TC.ChainSpans(span, !Flag("window"));

const tune = Object.assign({}, TC.DEFAULT_TUNE);
const cp = Object.assign({}, TC.DEFAULT_CRATER);
if (Flag("no-craters")) cp.on = 0;
if (Flag("last-only")) cp.everyLevel = 0;
for (const kv of (Arg("set", "") || "").split(",").filter(Boolean)){
  const [k, v] = kv.split("=");
  if (k in tune) tune[k] = parseFloat(v);
  else if (k in cp) cp[k] = parseFloat(v);
  else throw new Error("unknown lever: " + k);
}

/* --- render --------------------------------------------------------- */

const t0 = Date.now();
const { levels, report } = TC.GenerateChain({
  block, lat, lon, res, spans, tune,
  craters: TC.CRATERS, craterParams: cp, originLat, originLon
});
const ms = Date.now() - t0;

const files = [];
for (let i = 0; i < levels.length; i++){
  const L = levels[i];
  const rgb = new Uint8Array(res * res * 3);
  for (let p = 0; p < res * res; p++){
    rgb[p * 3] = L.rgba[p * 4]; rgb[p * 3 + 1] = L.rgba[p * 4 + 1];
    rgb[p * 3 + 2] = L.rgba[p * 4 + 2];
  }
  const tag = L.spanKm < 1 ? Math.round(L.spanKm * 1000) + "m"
                           : (+L.spanKm.toFixed(2)) + "km";
  const f = path.join(outDir, `level${i}_${tag}.png`);
  WritePng(f, rgb, res, res);
  files.push({ f, rgb });
  let lo = 1e9, hi = -1e9;
  for (const v of L.height){ if (v < lo) lo = v; if (v > hi) hi = v; }
  console.log(`level ${i}  ${tag.padStart(7)}  `
    + `${(L.kmPerPx * 1000).toFixed(1).padStart(7)} m/px  `
    + `craters ${String(L.craters).padStart(2)}  `
    + `relief ${((hi - lo) * L.heightScaleM).toFixed(0).padStart(6)} m  -> ${path.basename(f)}`);
}

// The mosaic itself, at the same window and the same output size, with
// its texels kept square -- the comparison the bench is built around.
const S = TC.SourceTexels(block, lat, lon, spans[spans.length - 1] / TC.MOON_KM_PER_DEG);
const srcRgb = new Uint8Array(res * res * 3);
{
  const px = new Uint8ClampedArray(4);
  for (let y = 0; y < res; y++){
    const ty = Math.min(S.h - 1, Math.max(0, Math.floor(S.sy + (y + 0.5) / res * S.sh)));
    for (let x = 0; x < res; x++){
      const tx = Math.min(S.w - 1, Math.max(0, Math.floor(S.sx + (x + 0.5) / res * S.sw)));
      TC.RampColor(S.data[ty * S.w + tx] / 255, px, 0);
      const d = (y * res + x) * 3;
      srcRgb[d] = px[0]; srcRgb[d + 1] = px[1]; srcRgb[d + 2] = px[2];
    }
  }
}
const srcFile = path.join(outDir, "source.png");
WritePng(srcFile, srcRgb, res, res);
console.log(`mosaic     ${S.sw.toFixed(1)} x ${S.sh.toFixed(1)} real texels  `
  + `${(S.kmPerTexel * 1000).toFixed(0)} m/texel  -> ${path.basename(srcFile)}`);

// mosaic | synthesis, side by side.
const GAP = 10, sw = res * 2 + GAP;
const sheet = new Uint8Array(sw * res * 3);
const lastRgb = files[files.length - 1].rgb;
for (let y = 0; y < res; y++)
  for (let x = 0; x < res; x++){
    const s = (y * res + x) * 3;
    let d = (y * sw + x) * 3;
    sheet[d] = srcRgb[s]; sheet[d + 1] = srcRgb[s + 1]; sheet[d + 2] = srcRgb[s + 2];
    d = (y * sw + res + GAP + x) * 3;
    sheet[d] = lastRgb[s]; sheet[d + 1] = lastRgb[s + 1]; sheet[d + 2] = lastRgb[s + 2];
  }
const sheetFile = path.join(outDir, "compare.png");
WritePng(sheetFile, sheet, sw, res);

const dump = Arg("dump-lum", null);
if (dump){
  const L = levels[levels.length - 1];
  fs.writeFileSync(dump, Buffer.from(L.lum.buffer, L.lum.byteOffset, L.lum.byteLength));
  console.log("dumped luminance ->", dump);
}

console.log(`${region.name}  ${lat.toFixed(4)}, ${lon.toFixed(4)}  res ${res}  `
  + `ladder ${spans.map(v => +v.toFixed(2)).join(" -> ")} km  `
  + `craters ${cp.on ? (cp.everyLevel ? "every level" : "last level") : "OFF"}  `
  + `${ms} ms${report.escaped ? "  [WINDOW LEFT THE CARRIED IMAGERY]" : ""}`);
console.log("compare ->", sheetFile);
