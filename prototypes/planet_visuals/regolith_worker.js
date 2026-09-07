"use strict";
/* ===== REGOLITH WORKER ================================================
   The chain, off the main thread.

   Why this file exists: the picture people like is the one drawn close
   to one tile pixel per screen pixel. There is no cheap way to make it
   -- the chain's cost is very nearly linear in pixels and spread across
   a dozen stages, so there is no single stage to cut -- but there is no
   need for it to be cheap if it is not in the way. Generated here, a
   five second tile costs nothing anybody can feel: the stretched tile
   carries the view until the fine one lands.

   regolith_chain.js has no DOM in it by design, which is what makes
   this possible at all. The imagery block is decoded by the page (that
   part does need a canvas) and handed over once.
   ===================================================================== */

importScripts("regolith_chain.js");
const TC = TerrainChain;

let block = null;
let place = null;                       // lat, lon, tune, origin
let chains = new Map();                 // res -> live chain, built on demand

// The cached rungs stop getting finer here. Past it they cost real
// seconds -- most of the wait after a zoom to a new level -- and buy a
// difference that does not survive the crop. See MakeLiveChain.
const RUNG_RES_CAP = 1200;

function ChainFor(res){
  let lc = chains.get(res);
  if (!lc){
    lc = TC.MakeLiveChain({
      block, lat: place.lat, lon: place.lon, res,
      rungRes: Math.min(res, RUNG_RES_CAP),
      tune: place.tune, craters: TC.CRATERS,
      craterParams: TC.DEFAULT_CRATER,
      originLat: place.originLat, originLon: place.originLon
    });
    chains.set(res, lc);
  }
  return lc;
}

self.onmessage = (e) => {
  const m = e.data;

  if (m.cmd === "block"){
    block = m.block;
    self.postMessage({ cmd: "block" });
    return;
  }

  // A new place, or a new look: every cached chain is stale either way.
  if (m.cmd === "place"){
    place = { lat: m.lat, lon: m.lon, tune: m.tune,
              originLat: m.originLat, originLon: m.originLon };
    chains = new Map();
    self.postMessage({ cmd: "place", epoch: m.epoch });
    return;
  }

  // The widest rung, for the page's locator inset. Sent as luminance so
  // the page can apply its own lift to it.
  if (m.cmd === "mini"){
    const lc = ChainFor(m.res);
    let best = lc.rungs[0];
    for (const r of lc.rungs)
      if (Math.abs(r.spanKm - m.spanKm) < Math.abs(best.spanKm - m.spanKm)) best = r;
    const lum = Float32Array.from(best.lum);
    self.postMessage({ cmd: "mini", epoch: m.epoch, res: lc.res,
                       spanKm: best.spanKm, lum }, [lum.buffer]);
    return;
  }

  // One cached rung, whole. The GPU path takes this instead of a freshly
  // cropped macro: the crop is a bilinear resample and an unsharp mask,
  // which is a millisecond of shader and was most of a second of
  // JavaScript at tile size. Fetched once per LEVEL rather than once per
  // view, so panning and zooming inside a rung cost no worker time at
  // all.
  //
  // Always at 1200, the same resolution the rungs are capped to, so the
  // cached rung does not change when the supersample ratio does.
  if (m.cmd === "rung"){
    if (!place || !block){ self.postMessage({ cmd: "rung", id: m.id, stale: true }); return; }
    const t0 = performance.now();
    const lc = ChainFor(RUNG_RES_CAP);
    lc.EnsureRung(m.base);
    // A copy: the cache keeps its own, and this one is given away.
    const lum = Float32Array.from(lc.rungs[m.base].lum);
    self.postMessage({ cmd: "rung", id: m.id, epoch: m.epoch, base: m.base,
                       res: RUNG_RES_CAP, spanKm: lc.rungs[m.base].spanKm,
                       ms: performance.now() - t0, lum }, [lum.buffer]);
    return;
  }

  // The macro alone: the rung ladder and the crop, stopping where the
  // GPU path takes over. This is the cheap half -- tens of milliseconds
  // against seconds -- and it is still done here because the rungs live
  // here.
  if (m.cmd === "macro"){
    if (!place || !block){ self.postMessage({ cmd: "macro", id: m.id, stale: true }); return; }
    const t0 = performance.now();
    const lc = ChainFor(m.res);
    const pre = lc.Prepare(m.spanKm, m.offX, m.offY);
    self.postMessage({ cmd: "macro", id: m.id, epoch: m.epoch, fine: !!m.fine, gen: m.gen,
                       res: m.res, margin: m.margin, spanKm: m.spanKm,
                       cxKm: m.cxKm, cyKm: m.cyKm, frame: pre.frame,
                       kmPerPx: pre.kmPerPx, ms: performance.now() - t0,
                       lum: pre.lum }, [pre.lum.buffer]);
    return;
  }

  if (m.cmd === "view"){
    if (!place || !block){ self.postMessage({ cmd: "view", id: m.id, stale: true }); return; }

    // A piece of a bigger tile. Same ground sampling as the whole would
    // have, offset to its own cell, and built `pad` pixels wider all
    // round so the parts of the chain that read their neighbours -- the
    // blurs, and the shadow march, which walks up to 22*k pixels toward
    // the sun -- have real ground to read instead of a clamped edge.
    // That padding is cropped off before the piece is blitted.
    //
    // The level is forced to the WHOLE tile's, because it seeds the
    // noise and scales the amplitude: a piece that chose its own would
    // not join up with its neighbours.
    if (m.part){
      const { i, j, cell, pad } = m.part;
      const subRes = cell + 2 * pad;
      const kmPerPx = m.spanKm / m.res;
      const cx = ((i + 0.5) * cell - m.res / 2) * kmPerPx;
      const cy = ((j + 0.5) * cell - m.res / 2) * kmPerPx;
      const lc = ChainFor(subRes);
      const v = lc.View(subRes * kmPerPx, m.offX + cx, m.offY + cy,
                        lc.PickLevel(m.spanKm, m.offX, m.offY), m.tuneOver);
      self.postMessage({ cmd: "view", id: m.id, epoch: m.epoch, fine: !!m.fine, gen: m.gen,
                         margin: m.margin, part: m.part, subRes,
                         res: m.res, spanKm: m.spanKm, kmPerPx,
                         cxKm: m.cxKm, cyKm: m.cyKm,
                         craters: v.craters, popCraters: v.popCraters, ms: v.ms,
                         rgba: v.rgba }, [v.rgba.buffer]);
      return;
    }

    const lc = ChainFor(m.res);
    const v = lc.View(m.spanKm, m.offX, m.offY, null, m.tuneOver);
    // rgba is freshly allocated per view, so it can be given away.
    self.postMessage({ cmd: "view", id: m.id, epoch: m.epoch, fine: !!m.fine, gen: m.gen,
                       margin: m.margin,
                       res: v.res, spanKm: v.spanKm, kmPerPx: v.kmPerPx,
                       cxKm: m.cxKm, cyKm: m.cyKm,
                       craters: v.craters, popCraters: v.popCraters, ms: v.ms,
                       rgba: v.rgba }, [v.rgba.buffer]);
    return;
  }
};
