/* Extracted verbatim from dashboard.html lines 21-709 so the visual-diff
 * reference page can load the module on its own. Do not edit here --
 * dashboard.html is canonical (spec: 'canonical source'). Regenerate with
 * the snippet in tools/visdiff/visdiff.sh. */
/* ToolRack — sci-fi tool rack renderer (Canvas 2D)
 * Levels: 1 chassis · 2 plates · 3 slot lights · 4 tool rows · 5 icons · 6 finish
 * Local coordinates: origin at the spine's top-left (reference image: 232,100). Rack ≈ 370×842.
 */
(function (root, factory) {
  if (typeof module === 'object' && module.exports) module.exports = factory();
  else root.ToolRack = factory();
})(typeof self !== 'undefined' ? self : this, function () {
  const TAU = Math.PI * 2;

  // ---------- palette (sampled from the reference) ----------
  const P = {
    bg: '#020e17', divider: '#172c41',
    chan: '#06121c', chanTop: '#0a1a2c', chanEdge: '#304d69', chanLine: '#02141c',
    body: '#263d51', bodyLo: '#1b2f42', bodyEdge: '#0f2231',
    brk: '#1f3445', brkLo: '#182b3a', brkHi: '#4b6c8f', brkTop: '#7ea1c6', brkEdge: '#5b83a8',
    holeIn: '#020a12', holeBevel: '#4c6d90', outline: 'rgba(0,6,14,0.8)',
    stud: '#3a5673', studHi: '#7ea4c7', studTop: '#b7dcf5', studLo: '#0f2335',
    plate: '#1e3447', plateLo: '#162a3b', plateHi: '#34506e', plateBand: '#2a445b', plateEdge: '#5f84a9',
    tab: '#142636', tabLo: '#0e1d2c', tabHi: '#496482', tabBand: '#1f3547',
    bar: '#223a4f', barLo: '#192d3e', barHi: '#4e6e8e', barBand: '#2a445b',
    bar2: '#11212f', bar2Lo: '#0b1927', bar2Band: '#183246', seam: '#050f1b',
    screwHead: '#2b4660', screwRim: '#4c6d8a', screwHi: '#e6f0f8', screwHi2: '#8fb0cc',
    stripeH: '#426d93', stripeF: '#3c678d',
    hdrText: '#5688b4', hdrSub: '#4878a2', footText: '#426b93',
    cyan: '#00fbfe', cyanCore: '#6ffefe', cyanEdge: '#00bdd3', cyanGlow: '#00e5f5', halo: '#00273d',
    boxIn: '#000403', label: '#011625', labelR: '#012030', labelEdge: '#01283f',
    typeOn: '#0574a0', white: '#f7fcfd', bitBlue: '#7dc3fa', bitBlue2: '#58a4f6', ellipse: '#00ecf8',
    boxInOff: '#0b1825', boxEdgeOff: '#3a5470', tickOff: '#8ea9c4', labelOff: '#061420', labelOffR: '#0d1b28', labelEdgeOff: '#0e2131',
    nameOff: '#587ea6', typeOff: '#3c5f80', pipOff: '#36495f', pipOffEdge: '#243547',
    rover: '#4b6984', roverHi: '#6883a5', roverLo: '#2f4b66', roverDk: '#172a3e',
    sock: '#050e17', sockRim: '#587b98', sockLine: '#000206', face: '#4e6e8b', faceHi: '#6c8caa', faceEdge: '#2e475d',
    capA: '#8ac4f4', capB: '#a0d2f8', rule: '#6cb8f9', titleA: '#2c62b5', titleB: '#5bc6f6', sub: '#4577ad',
  };

  // ---------- layout ----------
  const G = {
    w: 370, h: 842, spineBodyX: 20, spineBodyW: 38, spineBottom: 750,
    rowTop0: 140, rowPitch: 122, rowH: 108,
    boxX: 77, boxW: 109, boxH: 103,
    labelX: 194, labelW: 161,
    pillCX: 38,
    studs: [137, 240, 356, 477, 595, 714],
  };
  const DEFAULT_FONT = '"JetBrains Mono","IBM Plex Mono","Roboto Mono",ui-monospace,Menlo,Consolas,monospace';

  // ---------- primitives ----------
  function rpoly(ctx, pts, dx = 0, dy = 0) {           // polygon with optional per-vertex radius [x,y,r]
    ctx.beginPath();
    const n = pts.length;
    for (let i = 0; i < n; i++) {
      const [x, y, r] = pts[i];
      if (!r) { i ? ctx.lineTo(x + dx, y + dy) : ctx.moveTo(x + dx, y + dy); continue; }
      const [px, py] = pts[(i - 1 + n) % n], [nx, ny] = pts[(i + 1) % n];
      const l1 = Math.hypot(px - x, py - y), l2 = Math.hypot(nx - x, ny - y);
      const t1x = x + (px - x) / l1 * r + dx, t1y = y + (py - y) / l1 * r + dy;
      const t2x = x + (nx - x) / l2 * r + dx, t2y = y + (ny - y) / l2 * r + dy;
      i ? ctx.lineTo(t1x, t1y) : ctx.moveTo(t1x, t1y);
      ctx.quadraticCurveTo(x + dx, y + dy, t2x, t2y);
    }
    ctx.closePath();
  }
  function rrect(ctx, x, y, w, h, r) {
    rpoly(ctx, [[x, y, r], [x + w, y, r], [x + w, y + h, r], [x, y + h, r]]);
  }
  function bounds(pts) {
    let x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9;
    for (const [x, y] of pts) { x0 = Math.min(x0, x); y0 = Math.min(y0, y); x1 = Math.max(x1, x); y1 = Math.max(y1, y); }
    return [x0, y0, x1 - x0, y1 - y0];
  }
  function line(ctx, x1, y1, x2, y2, color, w = 1) {
    ctx.beginPath(); ctx.moveTo(x1, y1); ctx.lineTo(x2, y2); ctx.strokeStyle = color; ctx.lineWidth = w; ctx.stroke();
  }
  // Draw a slot in transition: previous state underneath, current state faded in on top.
  function crossfade(ctx, tool, draw) {
    if (!tool || tool.fade == null || tool.fade >= 1 || !tool.prev) { draw(tool); return; }
    draw({ ...tool, ...tool.prev });
    ctx.save(); ctx.globalAlpha = Math.max(0, tool.fade); draw(tool); ctx.restore();
  }
  function glowOn(ctx, color, blur) { ctx.shadowColor = color; ctx.shadowBlur = blur; }
  function glowOff(ctx) { ctx.shadowColor = 'transparent'; ctx.shadowBlur = 0; }

  // Bevelled slab: vertical gradient body, light band on top/left edges, band on bottom/right, grain, outline.
  function slab(ctx, pts, o) {
    const b = bounds(pts);
    const grad = ctx.createLinearGradient(0, b[1], 0, b[1] + b[3]);
    grad.addColorStop(0, o.top); grad.addColorStop(1, o.bottom);
    const hw = o.hiW ?? 2, lw = o.loW ?? 2;
    ctx.save();
    rpoly(ctx, pts); ctx.fillStyle = o.lo; ctx.fill();
    rpoly(ctx, pts); ctx.clip();
    rpoly(ctx, pts, -lw, -lw); ctx.fillStyle = o.hi; ctx.fill();
    rpoly(ctx, pts, -lw, -lw); ctx.clip();
    rpoly(ctx, pts, hw, hw); ctx.fillStyle = grad; ctx.fill();
    if (o.grain) { ctx.globalAlpha = o.grainA ?? 0.09; ctx.fillStyle = o.grain; ctx.fillRect(b[0] - 4, b[1] - 4, b[2] + 8, b[3] + 8); ctx.globalAlpha = 1; }
    ctx.restore();
    rpoly(ctx, pts); ctx.lineWidth = 1; ctx.strokeStyle = o.outline || P.outline; ctx.stroke();
  }
  // Recessed hole: dark fill, dark rim, light bevel on downward-facing edges.
  function hole(ctx, pts) {
    ctx.save();
    rpoly(ctx, pts); ctx.fillStyle = P.holeBevel; ctx.fill();
    rpoly(ctx, pts); ctx.clip();
    rpoly(ctx, pts, 0, -2); ctx.fillStyle = P.holeIn; ctx.fill();
    ctx.restore();
    rpoly(ctx, pts); ctx.strokeStyle = 'rgba(0,3,8,0.85)'; ctx.lineWidth = 1.5; ctx.stroke();
  }
  function makeGrain(makeCanvas) {
    if (!makeCanvas) return null;
    const s = 128, cv = makeCanvas(s, s), g = cv.getContext('2d');
    const img = g.createImageData(s, s), d = img.data;
    for (let i = 0; i < d.length; i += 4) {
      const v = (100 + (Math.random() + Math.random()) * 40) | 0;
      d[i] = v; d[i + 1] = v + 6; d[i + 2] = v + 14; d[i + 3] = 255;
    }
    g.putImageData(img, 0, 0);
    return g.createPattern(cv, 'repeat');
  }
  function text(ctx, str, x, y, o) {
    ctx.font = `${o.weight || 500} ${o.size}px ${o.font}`;
    ctx.fillStyle = o.color; ctx.textBaseline = 'alphabetic'; ctx.textAlign = 'left';
    const sp = o.spacing || 0;
    if (!sp) { ctx.fillText(str, x, y); return; }
    let cx = x;
    for (const ch of str) { ctx.fillText(ch, cx, y); cx += ctx.measureText(ch).width + sp; }
  }
  // Text with horizontal squeeze and optional vertical gradient (top → bottom over cap height)
  function condensed(ctx, str, x, y, o) {
    ctx.save(); ctx.translate(x, y); ctx.scale(o.sx || 1, 1);
    let color = o.color;
    if (o.gradient) {
      const g = ctx.createLinearGradient(0, -o.size * 0.72, 0, 0);
      o.gradient.forEach(([s, c]) => g.addColorStop(s, c)); color = g;
    }
    text(ctx, str, 0, 0, { ...o, color });
    ctx.restore();
  }

  // ---------- level 1: chassis ----------
  function drawSpine(ctx, grain) {
    const bx = G.spineBodyX, bw = G.spineBodyW, top = 4, bot = G.spineBottom;
    // recessed channel with studs
    const cg = ctx.createLinearGradient(0, top, 0, bot);
    cg.addColorStop(0, P.chanTop); cg.addColorStop(0.35, '#03080f'); cg.addColorStop(1, '#071424');
    ctx.fillStyle = cg; ctx.fillRect(0, top, bx, bot - top);
    ctx.fillStyle = P.chanEdge; ctx.fillRect(0, top, 2, bot - top);
    ctx.fillStyle = P.chanLine; ctx.fillRect(bx - 2, top, 2, bot - top);
    // raised body
    slab(ctx, [[bx, top], [bx + bw, top], [bx + bw, bot], [bx, bot]],
      { top: P.body, bottom: P.bodyLo, hi: '#2f4a60', lo: P.bodyEdge, hiW: 1, loW: 1, grain });
    ctx.fillStyle = 'rgba(0,0,0,0.25)'; ctx.fillRect(bx + bw - 2, top, 2, bot - top);
    for (const y of G.studs) {
      ctx.fillStyle = P.stud; ctx.fillRect(0, y, 12, 21);
      ctx.fillStyle = P.studHi; ctx.fillRect(0, y, 2, 21);
      ctx.fillStyle = P.studTop; ctx.fillRect(0, y, 12, 1);
      ctx.fillStyle = '#5c7ea0'; ctx.fillRect(2, y + 1, 10, 1);
      ctx.fillStyle = P.studLo; ctx.fillRect(0, y + 20, 12, 1); ctx.fillRect(11, y, 1, 21);
    }
  }
  const BRACKET = { top: P.brk, bottom: P.brkLo, hi: P.brkHi, lo: '#12222f', hiW: 3, loW: 1 };
  function drawTopBracket(ctx, grain) {
    const pts = [[0, 4, 9], [40, 4], [100, 64], [58, 64], [58, 114], [34, 115], [18, 131], [18, 134], [0, 134]];
    slab(ctx, pts, { ...BRACKET, grain });
    line(ctx, 0.5, 12, 0.5, 134, P.brkEdge, 2);                 // bright rail edge
    line(ctx, 40, 4.5, 100, 64.5, 'rgba(120,160,200,0.55)', 2); // strut edge highlight
    line(ctx, 34, 116, 18, 132, 'rgba(120,160,200,0.45)', 1.5);  // lower diagonal
    ctx.fillStyle = 'rgba(255,255,255,0.9)'; ctx.fillRect(3, 6, 2, 1);
    hole(ctx, [[19, 25, 5], [30, 25, 4], [48, 55, 5], [19, 55, 5]]);
    hole(ctx, [[19, 75, 3], [27, 75, 3], [40, 88, 3], [40, 111, 3], [28, 122, 3], [19, 122, 3]]);
  }
  function drawGusset(ctx, grain) {
    const pts = [[-2, 736], [26, 736], [84, 794, 3], [34, 842], [6, 842], [-2, 834]];
    slab(ctx, pts, { ...BRACKET, grain });
    line(ctx, -1, 736, -1, 836, P.brkEdge, 2);
    line(ctx, 26, 736.5, 84, 794.5, 'rgba(120,160,200,0.5)', 2);
    hole(ctx, [[18, 756, 4], [50, 790, 4], [18, 818, 4]]);
  }
  function drawChassis(ctx, grain) {
    drawSpine(ctx, grain);
    drawTopBracket(ctx, grain);
    drawGusset(ctx, grain);
  }

  // ---------- level 2: plates ----------
  function screw(ctx, x, y, r = 8) {
    ctx.beginPath(); ctx.arc(x, y, r, 0, TAU); ctx.fillStyle = '#000000'; ctx.fill();
    ctx.beginPath(); ctx.arc(x, y, r + 0.5, 0, TAU); ctx.strokeStyle = 'rgba(70,100,130,0.35)'; ctx.lineWidth = 1; ctx.stroke();
    ctx.beginPath(); ctx.arc(x, y, r - 3, 0, TAU); ctx.fillStyle = P.screwHead; ctx.fill();
    ctx.beginPath(); ctx.arc(x, y, r - 3, 0.3, 2.6); ctx.strokeStyle = P.screwRim; ctx.lineWidth = 1.5; ctx.stroke();
    ctx.beginPath(); ctx.arc(x - 1.6, y - 1.6, 2.4, 0, TAU); ctx.fillStyle = P.screwHi; ctx.fill();
    ctx.beginPath(); ctx.arc(x - 0.5, y - 0.5, 3.6, 3.4, 5.2); ctx.strokeStyle = P.screwHi2; ctx.lineWidth = 1; ctx.stroke();
    ctx.fillStyle = 'rgba(0,0,0,0.85)'; ctx.fillRect(x + 0.5, y + 0.5, 2.5, 1.5);
  }
  function stripes(ctx, x0, y, w, h, gap, skew, n, color) {
    ctx.fillStyle = color;
    for (let i = 0; i < n; i++) {
      const x = x0 + i * (w + gap);
      rpoly(ctx, [[x + skew, y], [x + skew + w, y], [x + w, y + h], [x, y + h]]); ctx.fill();
    }
  }
  function drawHeader(ctx, rack, grain, font) {
    // lower tab first (right end), then the main tier over it
    slab(ctx, [[224, 92], [338, 92], [342, 96], [342, 127], [338, 131], [224, 131]],
      { top: P.tab, bottom: P.tabLo, hi: P.tabHi, lo: P.tabBand, grain });
    line(ctx, 341.5, 97, 341.5, 126, P.tabEdge, 1);
    slab(ctx, [[67, 54], [308, 54], [316, 62], [316, 88], [308, 89], [304, 93], [304, 124], [57, 124], [57, 64]],
      { top: P.plate, bottom: P.plateLo, hi: P.plateHi, lo: P.plateBand, grain });
    line(ctx, 315.5, 63, 315.5, 88, P.plateEdge, 1);          // specular right edge
    line(ctx, 67, 54.5, 300, 54.5, 'rgba(140,175,210,0.35)', 1);
    line(ctx, 60, 96, 60, 124, P.bg, 3);                      // dark seam between bracket and plate
    line(ctx, 58, 96, 58, 124, 'rgba(120,160,200,0.35)', 1);
    // screw boss on the bracket at the plate's left end
    ctx.beginPath(); ctx.arc(58, 85, 12, 0, TAU); ctx.fillStyle = P.brk; ctx.fill();
    ctx.strokeStyle = 'rgba(0,6,14,0.7)'; ctx.lineWidth = 1; ctx.stroke();
    screw(ctx, 58, 85); screw(ctx, 306, 68); screw(ctx, 326, 113);
    stripes(ctx, 238, 98, 11, 15, 8, 10, 3, P.stripeH);
    text(ctx, rack.header || 'SURVEY TOOLS', 84, 83, { size: 21, color: P.hdrText, font, weight: 600 });
    text(ctx, rack.unit || 'FIELD UNIT U-77', 84, 109, { size: 15, color: P.hdrSub, font, weight: 500 });
  }
  function drawFooter(ctx, rack, grain, font) {
    slab(ctx, [[40, 750], [342, 750], [346, 754], [346, 789], [342, 793], [40, 793]],
      { top: P.bar, bottom: P.barLo, hi: P.barHi, lo: P.barBand, grain });
    line(ctx, 345.5, 755, 345.5, 788, P.plateEdge, 1);
    slab(ctx, [[98, 796], [344, 796], [346, 798], [346, 835], [342, 840], [100, 840], [73, 812]],
      { top: P.bar2, bottom: P.bar2Lo, hi: '#1c3245', lo: P.bar2Band, hiW: 1, loW: 3, grain });
    line(ctx, 100, 795, 346, 795, P.seam, 2);                  // seam between bars
    screw(ctx, 77, 771); screw(ctx, 327, 769);
    stripes(ctx, 242, 806, 12, 19, 10, 10, 4, P.stripeF);
    text(ctx, rack.footer || 'TOOLS READY', 108, 823, { size: 19, color: P.footText, font, weight: 500 });
  }
  function drawPlates(ctx, rack, grain, font) {
    drawHeader(ctx, rack, grain, font);
    drawFooter(ctx, rack, grain, font);
    drawGusset(ctx, grain);   // strut overlaps the footer bar
  }

  // ---------- level 3: slot lights ----------
  function drawPill(ctx, cy, on, fx) {
    const cx = G.pillCX;
    const hex = [[cx - 5, cy - 37], [cx + 5, cy - 37], [cx + 14, cy - 28], [cx + 14, cy + 28], [cx + 5, cy + 37], [cx - 5, cy + 37], [cx - 14, cy + 28], [cx - 14, cy - 28]];
    if (on && fx) {   // wide bloom across the spine
      const g = ctx.createRadialGradient(cx, cy, 6, cx, cy, 50);
      g.addColorStop(0, 'rgba(0,229,245,0.4)'); g.addColorStop(1, 'rgba(0,229,245,0)');
      ctx.fillStyle = g; ctx.fillRect(cx - 60, cy - 60, 120, 120);
    }
    rpoly(ctx, hex); ctx.fillStyle = on && fx ? '#062a36' : P.sock; ctx.fill();
    ctx.strokeStyle = P.sockLine; ctx.lineWidth = 2; ctx.stroke();
    ctx.beginPath(); ctx.moveTo(cx - 12.5, cy - 27); ctx.lineTo(cx - 4.5, cy - 35); ctx.lineTo(cx + 4.5, cy - 35); ctx.lineTo(cx + 12.5, cy - 27);
    ctx.strokeStyle = P.sockRim; ctx.lineWidth = 1.5; ctx.stroke();
    ctx.beginPath(); ctx.moveTo(cx - 12.5, cy + 27); ctx.lineTo(cx - 4.5, cy + 35); ctx.lineTo(cx + 4.5, cy + 35); ctx.lineTo(cx + 12.5, cy + 27);
    ctx.strokeStyle = 'rgba(88,123,152,0.35)'; ctx.lineWidth = 1; ctx.stroke();
    if (on) {
      if (fx) glowOn(ctx, P.cyanGlow, 16);
      rrect(ctx, cx - 7.5, cy - 22.5, 15, 45, 4.5); ctx.fillStyle = P.cyanEdge; ctx.fill();
      glowOff(ctx);
      rrect(ctx, cx - 6, cy - 21, 12, 42, 3.5); ctx.fillStyle = P.cyanCore; ctx.fill();
      rrect(ctx, cx - 3, cy - 18, 6, 36, 2); ctx.fillStyle = 'rgba(255,255,255,0.35)'; ctx.fill();
    } else {
      rrect(ctx, cx - 8, cy - 23, 16, 46, 5); ctx.fillStyle = P.faceEdge; ctx.fill();
      rrect(ctx, cx - 6, cy - 21, 12, 42, 3.5); ctx.fillStyle = P.face; ctx.fill();
      ctx.fillStyle = P.faceHi; ctx.fillRect(cx - 5, cy - 20, 10, 2);
      ctx.fillStyle = 'rgba(0,0,0,0.25)'; ctx.fillRect(cx - 5, cy + 18, 10, 2);
    }
  }
  function drawLights(ctx, rack, slots, fx) {
    for (let i = 0; i < slots; i++) {
      const cy = G.rowTop0 + i * G.rowPitch + 53;
      crossfade(ctx, rack.tools[i], t => drawPill(ctx, cy, !!(t && t.active), fx));
    }
  }

  // ---------- level 4: tool rows ----------
  function cornerTicks(ctx, x, y, w, h, color) {
    const t = 8, i = 4;
    ctx.strokeStyle = color; ctx.lineWidth = 2; ctx.lineCap = 'butt';
    ctx.beginPath();
    ctx.moveTo(x + i, y + i + t); ctx.lineTo(x + i, y + i); ctx.lineTo(x + i + t, y + i);
    ctx.moveTo(x + w - i - t, y + i); ctx.lineTo(x + w - i, y + i); ctx.lineTo(x + w - i, y + i + t);
    ctx.moveTo(x + i, y + h - i - t); ctx.lineTo(x + i, y + h - i); ctx.lineTo(x + i + t, y + h - i);
    ctx.moveTo(x + w - i - t, y + h - i); ctx.lineTo(x + w - i, y + h - i); ctx.lineTo(x + w - i, y + h - i - t);
    ctx.stroke();
  }
  function drawRow(ctx, tool, i, fx, font) {
    const t = G.rowTop0 + i * G.rowPitch, on = !!tool.active;
    // icon box: dark well with a chamfered outline and corner ticks
    const bx = G.boxX, by = t + 1, bw = G.boxW, bh = G.boxH;
    const box = [[bx, by, 7], [bx + bw, by, 7], [bx + bw, by + bh, 7], [bx, by + bh, 7]];
    rpoly(ctx, box); ctx.strokeStyle = on ? P.halo : 'rgba(0,4,10,0.8)'; ctx.lineWidth = on ? 5 : 4; ctx.stroke();
    rpoly(ctx, box); ctx.fillStyle = on ? P.boxIn : P.boxInOff; ctx.fill();
    if (on && fx) glowOn(ctx, P.cyanGlow, 6);
    rpoly(ctx, box); ctx.strokeStyle = on ? P.cyan : P.boxEdgeOff; ctx.lineWidth = on ? 2 : 2.5; ctx.stroke();
    glowOff(ctx);
    cornerTicks(ctx, bx, by, bw, bh, on ? P.cyan : P.tickOff);

    // label plate with arrow tip
    const lx = G.labelX, lw = G.labelW;
    const lab = [[lx, t], [lx + lw - 14, t], [lx + lw, t + 34], [lx + lw, t + 73], [lx + lw - 14, t + 108], [lx, t + 108]];
    const lg = ctx.createLinearGradient(lx, 0, lx + lw, 0);
    lg.addColorStop(0, on ? P.label : P.labelOff); lg.addColorStop(1, on ? P.labelR : P.labelOffR);
    rpoly(ctx, lab); ctx.fillStyle = lg; ctx.fill();
    ctx.save(); rpoly(ctx, lab); ctx.clip();
    rpoly(ctx, lab); ctx.strokeStyle = on ? P.labelEdge : P.labelEdgeOff; ctx.lineWidth = 6; ctx.stroke();
    if (on && fx) {   // pip light spilling onto the plate
      const g = ctx.createRadialGradient(lx + lw, t + 54, 2, lx + lw, t + 54, 70);
      g.addColorStop(0, 'rgba(0,229,245,0.16)'); g.addColorStop(1, 'rgba(0,229,245,0)');
      ctx.fillStyle = g; ctx.fillRect(lx, t, lw, 108);
    }
    ctx.restore();
    condensed(ctx, tool.name, lx + 17, t + 49, { size: 23, sx: 0.77, color: on ? (tool.accent || P.cyan) : P.nameOff, font, weight: 700 });
    condensed(ctx, tool.type, lx + 17, t + 76, { size: 20, sx: 0.83, color: on ? P.typeOn : P.typeOff, font, weight: 500 });

    // status pip: half-hexagon bar outside the tip
    const px = lx + lw + 1, py = t + 34;
    const pip = [[px, py], [px + 3, py], [px + 8, py + 5], [px + 8, py + 34], [px + 3, py + 39], [px, py + 39]];
    if (on) {
      if (fx) glowOn(ctx, P.cyanGlow, 14);
      rpoly(ctx, pip); ctx.fillStyle = '#03f8f9'; ctx.fill(); glowOff(ctx);
      ctx.fillStyle = 'rgba(255,255,255,0.45)'; ctx.fillRect(px + 2, py + 5, 3, 29);
    } else {
      rpoly(ctx, pip); ctx.fillStyle = P.pipOff; ctx.fill();
      ctx.strokeStyle = P.pipOffEdge; ctx.lineWidth = 1.5; ctx.stroke();
      ctx.fillStyle = 'rgba(140,170,200,0.35)'; ctx.fillRect(px + 1, py + 2, 5, 1.5);
    }
  }
  function drawRows(ctx, rack, fx, font) {
    rack.tools.forEach((tool, i) => { if (tool) crossfade(ctx, tool, t => drawRow(ctx, t, i, fx, font)); });
  }

  // ---------- level 5: icons ----------
  function tri(ctx, x, y, w, h, color) {   // downward triangle, top edge centred at (x,y)
    ctx.beginPath(); ctx.moveTo(x - w / 2, y); ctx.lineTo(x + w / 2, y); ctx.lineTo(x, y + h); ctx.closePath();
    ctx.fillStyle = color; ctx.fill();
  }
  const ICONS = {
    drill(ctx, cx, cy, on, fx) {
      const c = on ? P.cyan : P.nameOff, w = on ? P.white : '#9fb6cc', b = on ? P.bitBlue : '#6a86a6';
      if (on && fx) glowOn(ctx, P.cyanGlow, 6);
      ctx.fillStyle = c; ctx.fillRect(cx - 11, cy - 38, 22, 21);
      glowOff(ctx);
      ctx.fillStyle = on ? P.boxIn : P.boxInOff; ctx.fillRect(cx - 3, cy - 30, 7, 7);
      [[-14, 32, 11], [0, 24, 10], [12, 16, 10]].forEach(([dy, wd, h]) => {
        tri(ctx, cx, cy + dy, wd, h, b);
        ctx.save(); ctx.beginPath(); ctx.rect(cx - wd, cy + dy - 1, wd, h + 2); ctx.clip();
        tri(ctx, cx, cy + dy, wd, h, w); ctx.restore();
        ctx.fillStyle = w; ctx.fillRect(cx - wd / 2, cy + dy, wd, 2);
      });
      if (on && fx) glowOn(ctx, P.cyanGlow, 5);
      ctx.strokeStyle = on ? P.ellipse : P.nameOff; ctx.lineWidth = 3.5; ctx.setLineDash([11, 6]); ctx.lineDashOffset = -3;
      ctx.beginPath(); ctx.ellipse(cx, cy + 23, 36, 14, 0, 0, TAU); ctx.stroke();
      ctx.setLineDash([]); ctx.lineDashOffset = 0; glowOff(ctx);
    },
    seismic(ctx, cx, cy, on, fx) {
      const pts = [[-42, 0], [-28, 0], [-22, -6], [-16, 8], [-10, -40], [-4, 44], [2, -30], [8, 22], [14, -10], [20, 6], [26, 0], [42, 0]];
      const path = () => { ctx.beginPath(); pts.forEach(([x, y], i) => (i ? ctx.lineTo(cx + x, cy + y) : ctx.moveTo(cx + x, cy + y))); };
      ctx.lineJoin = 'miter'; ctx.lineCap = 'round';
      path(); ctx.strokeStyle = on ? 'rgba(30,110,150,0.55)' : 'rgba(70,95,125,0.5)'; ctx.lineWidth = 8; ctx.stroke();
      if (on && fx) glowOn(ctx, P.cyanGlow, 10);
      path(); ctx.strokeStyle = on ? P.white : '#a3b8cc'; ctx.lineWidth = 3; ctx.stroke();
      glowOff(ctx);
    },
    rover(ctx, cx, cy, on, fx) {
      const body = on ? P.cyan : P.rover, hi = on ? P.white : P.roverHi, lo = on ? P.cyanEdge : P.roverLo, dk = on ? '#003a4a' : P.roverDk;
      const outline = 'rgba(0,6,14,0.75)';
      if (on && fx) glowOn(ctx, P.cyanGlow, 6);
      // roll cage
      ctx.beginPath(); ctx.moveTo(cx - 18, cy - 4); ctx.lineTo(cx - 14, cy - 28); ctx.quadraticCurveTo(cx, cy - 34, cx + 14, cy - 28); ctx.lineTo(cx + 18, cy - 4);
      ctx.strokeStyle = outline; ctx.lineWidth = 7; ctx.lineJoin = 'round'; ctx.stroke();
      ctx.strokeStyle = hi; ctx.lineWidth = 4; ctx.stroke();
      ctx.beginPath(); ctx.moveTo(cx - 12, cy - 24); ctx.lineTo(cx + 12, cy - 24); ctx.strokeStyle = lo; ctx.lineWidth = 2; ctx.stroke();
      // body
      rrect(ctx, cx - 31, cy - 8, 62, 22, 4); ctx.fillStyle = outline; ctx.lineWidth = 3; ctx.strokeStyle = outline; ctx.stroke(); ctx.fillStyle = body; ctx.fill();
      ctx.fillStyle = lo; ctx.fillRect(cx - 31, cy + 8, 62, 6);
      ctx.fillStyle = hi; ctx.fillRect(cx - 28, cy - 6, 56, 2);
      ctx.fillStyle = dk; ctx.fillRect(cx - 27, cy - 2, 9, 4); ctx.fillRect(cx + 18, cy - 2, 9, 4); ctx.fillRect(cx - 6, cy - 3, 12, 8);
      ctx.fillStyle = hi; ctx.fillRect(cx - 4, cy - 1, 8, 2);
      // wheels
      [-20, 20].forEach(dx => {
        ctx.beginPath(); ctx.arc(cx + dx, cy + 22, 14, 0, TAU); ctx.fillStyle = outline; ctx.fill();
        ctx.beginPath(); ctx.arc(cx + dx, cy + 22, 12.5, 0, TAU); ctx.fillStyle = lo; ctx.fill();
        ctx.beginPath(); ctx.arc(cx + dx, cy + 22, 12.5, 0, TAU); ctx.strokeStyle = hi; ctx.lineWidth = 2; ctx.stroke();
        ctx.beginPath(); ctx.arc(cx + dx, cy + 22, 6, 0, TAU); ctx.fillStyle = body; ctx.fill();
        ctx.beginPath(); ctx.arc(cx + dx, cy + 22, 2.5, 0, TAU); ctx.fillStyle = dk; ctx.fill();
      });
      glowOff(ctx);
    },
  };
  function drawIcons(ctx, rack, fx) {
    rack.tools.forEach((tool, i) => {
      if (!tool) return; const fn = ICONS[tool.icon]; if (!fn) return;
      crossfade(ctx, tool, t => fn(ctx, G.boxX + G.boxW / 2, G.rowTop0 + i * G.rowPitch + 53, !!t.active, fx));
    });
  }

  // ---------- level 6: caption ----------
  function drawCaption(ctx, rack, font) {
    if (rack.index) condensed(ctx, rack.index, 0, -42, { size: 42, sx: 0.78, font, weight: 700, gradient: [[0, P.capA], [1, P.capB]] });
    ctx.fillStyle = P.rule; ctx.fillRect(-1, -20, 40, 2);
    if (rack.title) condensed(ctx, rack.title, 87, -55, { size: 23, sx: 0.9, font, weight: 700,
      gradient: [[0, P.titleA], [0.55, '#3470c2'], [0.8, '#44a1f0'], [1, P.titleB]] });
    if (rack.subtitle) condensed(ctx, rack.subtitle, 87, -24, { size: 17, sx: 0.83, font, weight: 500, color: P.sub });
  }

  // ---------- public ----------
  function drawRack(ctx, rack, x, y, opts = {}) {
    const level = opts.level ?? 6, fx = level >= 6, font = opts.font || DEFAULT_FONT;
    const grain = level >= 6 ? opts.grain : null, slots = rack.slots || 5;
    ctx.save(); ctx.translate(x, y);
    if (level >= 1) drawChassis(ctx, grain);
    if (level >= 2) drawPlates(ctx, rack, grain, font);
    if (level >= 3) drawLights(ctx, rack, slots, fx);
    if (level >= 4) drawRows(ctx, rack, fx, font);
    if (level >= 5) drawIcons(ctx, rack, fx);
    if (level >= 6) drawCaption(ctx, rack, font);
    ctx.restore();
  }
  function drawScene(ctx, racks, opts = {}) {
    const W = 1536, H = 1024, level = opts.level ?? 6;
    ctx.save();
    ctx.fillStyle = P.bg; ctx.fillRect(0, 0, W, H);
    if (level >= 6) { ctx.fillStyle = P.divider; ctx.fillRect(W / 2 - 1, 0, 2, H); }
    racks.forEach((r, i) => drawRack(ctx, r, 232 + i * 706, 100, opts));
    ctx.restore();
  }


  // =====================================================================
  // Variant B: compact rack v2 (single column, title above, framed slots)
  // Design units = reference px / 2, origin at the bracket's top-left (ref 124,23)
  // =====================================================================
  const PB = {
    bg: '#02111a',
    metal: '#263c50', metalLo: '#1f3345', metalHi: '#5f7c9b', metalBand: '#1a2d3e', edge: '#e0eff8', bodyEdge: '#688aaf',
    railShort: '#2f465c', railShortR: '#1e3247', railShortTop: '#f3fafd', railShortTop2: '#6a86a5',
    railLong: '#1a3649', railLongL: '#243d58', railLongR: '#144458', railEdge: '#7f97b2',
    bar: '#182c3c', barLo: '#122433', barHi: '#4c6781', barBand: '#1e3444', barEdge: '#3f5a74',
    frame: '#2c4258', frameLo: '#1c3144', frameHi: '#7094b2', frameHi2: '#385167', frameBand: '#1e3345',
    boxIn: '#010c13', boxInSel: '#00090f', sel: '#01fbfe', selHalo: 'rgba(1,69,88,0.9)', socket: '#000306', socketEdge: '#08202c',
    label: '#00141f', labelEdge: '#084a6a', labelOff: '#04141e', labelOffEdge: '#203b54',
    name: '#08f8fc', type: '#96cbf9', nameOff: '#587ea6', typeOff: '#3f5f82', iconOff: '#5f7a96', iconOffDk: '#34506b', pip: '#00f5fe', pipEdge: '#05dbf6', pipOff: '#1d3246', pipOffEdge: '#3c5a78',
    hole: '#00080d', holeRim: '#0a2432',
    sock: '#000306', face: '#5a799b', faceHi: '#8aa6c4', faceEdge: '#3f5c7c', core: '#00f8ff', coreEdge: '#03dbe9', glow: '#00e6f6',
    title: '#67b7fa', underline: '#01f8fc',
    head: '#02f8fe', stripe: '#07f7fd', ellipse: '#0df8fd', trace: '#cdfbfc',
    screwHead: '#0e1c2a', screwRim: '#3d5a76', nut: '#243b54', nutHi: '#f4f8fc',
  };
  const GB = {
    w: 395, h: 750, railW: 22, bodyX: 26, bodyW: 35.5, bodyTop: 110, bodyBottom: 700,
    rowTop0: 113.5, rowPitch: 119.25, rowH: 110, boxX: 77.5, boxW: 126, labelX: 212, labelW: 165, pillCX: 44,
    barX: 79, barRight: 366, barTop: 74, barH: 29.5, footTop: 710,
  };
  const METAL_B = { top: PB.metal, bottom: PB.metalLo, hi: PB.metalHi, lo: PB.metalBand, hiW: 2, loW: 1.5 };

  // §4 rail ---------------------------------------------------------------
  function railBlock(ctx, y, h, raised, grain) {
    const w = GB.railW;
    const g = ctx.createLinearGradient(0, 0, w, 0);
    if (raised) { g.addColorStop(0, '#3a516a'); g.addColorStop(0.5, PB.railShort); g.addColorStop(1, PB.railShortR); }
    else { g.addColorStop(0, PB.railLongL); g.addColorStop(0.45, PB.railLong); g.addColorStop(1, PB.railLongR); }
    ctx.fillStyle = g; ctx.fillRect(0, y, w, h);
    if (grain) { ctx.save(); ctx.globalAlpha = 0.08; ctx.fillStyle = grain; ctx.fillRect(0, y, w, h); ctx.restore(); }
    ctx.fillStyle = 'rgba(0,4,10,0.5)'; ctx.fillRect(0, y, 1, h);
    ctx.fillStyle = raised ? PB.edge : PB.railEdge; ctx.fillRect(1, y, 2, h);
    if (raised) {
      ctx.fillStyle = PB.railShortTop; ctx.fillRect(0, y, w, 1.5);
      ctx.fillStyle = PB.railShortTop2; ctx.fillRect(0, y + 1.5, w, 1.5);
      ctx.fillStyle = 'rgba(160,190,220,0.35)'; ctx.fillRect(0, y + h - 1.5, w, 1.5);
    }
    ctx.strokeStyle = 'rgba(0,4,10,0.75)'; ctx.lineWidth = 1; ctx.strokeRect(0.5, y + 0.5, w - 1, h - 1);
  }
  function railSegments(slots) {
    const shorts = [], longs = [];
    for (let i = 0; i < slots; i++) { const t = Math.max(GB.rowTop0 + i * GB.rowPitch - 16, GB.bodyTop + 4); shorts.push([t, t + 29]); }
    for (let i = 0; i < slots; i++) longs.push([shorts[i][1] + 4, i + 1 < slots ? shorts[i + 1][0] - 4 : 685.5]);
    return { shorts, longs };
  }
  // §5 body, §2 top bracket, §8 bottom bracket --------------------------------
  function hexNut(ctx, x, y, r) {
    ctx.beginPath(); ctx.arc(x, y, r, 0, TAU); ctx.fillStyle = '#000205'; ctx.fill();
    const hr = r - 3, hex = [];
    for (let k = 0; k < 6; k++) { const a = k * Math.PI / 3 + Math.PI / 6; hex.push([x + Math.cos(a) * hr, y + Math.sin(a) * hr]); }
    rpoly(ctx, hex); ctx.fillStyle = PB.nut; ctx.fill();
    ctx.strokeStyle = 'rgba(210,228,245,0.85)'; ctx.lineWidth = 1.5; ctx.stroke();
    ctx.beginPath(); ctx.arc(x, y, hr * 0.5, 0, TAU); ctx.fillStyle = '#050606'; ctx.fill();
    ctx.beginPath(); ctx.arc(x - hr * 0.35, y - hr * 0.4, 1.8, 0, TAU); ctx.fillStyle = PB.nutHi; ctx.fill();
  }
  function drawChassisB(ctx, slots, grain) {
    const bx = GB.bodyX, bw = GB.bodyW;
    // body
    slab(ctx, [[bx, GB.bodyTop], [bx + bw, GB.bodyTop], [bx + bw, GB.bodyBottom], [bx, GB.bodyBottom]], { ...METAL_B, hiW: 0.5, grain });
    ctx.fillStyle = PB.bodyEdge; ctx.fillRect(bx, GB.bodyTop, 3, GB.bodyBottom - GB.bodyTop);
    ctx.fillStyle = 'rgba(0,0,0,0.35)'; ctx.fillRect(bx + bw - 2, GB.bodyTop, 2, GB.bodyBottom - GB.bodyTop);
    // rail
    const seg = railSegments(slots);
    seg.longs.forEach(([a, b]) => railBlock(ctx, a, b - a, false, grain));
    seg.shorts.forEach(([a, b]) => railBlock(ctx, a, b - a, true, grain));
    // top bracket with strut and step joint
    const top = [[12, 0], [49, 0], [49, 48.5], [76, 75.5], [76, 96.5], [66.5, 96.5], [60.5, 103], [60.5, 110], [0, 110], [0, 12]];
    slab(ctx, top, { ...METAL_B, hiW: 2.5, grain });
    line(ctx, 1, 12, 1, 110, PB.edge, 2); line(ctx, 12, 1, 49, 1, '#6a86a5', 2); line(ctx, 1, 12, 12, 1, PB.edge, 2);
    line(ctx, 49.5, 48.5, 76.5, 75.5, 'rgba(150,180,215,0.55)', 1.5);
    line(ctx, 21.5, 66, 35, 79, 'rgba(0,6,14,0.85)', 4);                       // groove
    line(ctx, 20.5, 65, 34, 78, 'rgba(160,190,225,0.35)', 1);
    hexNut(ctx, 21.5, 23, 11); screw(ctx, 38, 88, 8);
    // bottom bracket
    const bot = [[0, 690], [23, 690], [23, 697.5], [61.5, 697.5], [61.5, 700.5], [72, 711], [72, 735.5], [63, 735.5], [58, 740], [58, 747.5], [8, 747.5], [0, 739.5]];
    slab(ctx, bot, { ...METAL_B, hiW: 2, grain });
    line(ctx, 1, 690, 1, 739.5, PB.edge, 2);
    line(ctx, 61.5, 700.5, 72.5, 711, 'rgba(150,180,215,0.55)', 1.5);
    hexNut(ctx, 31, 726.5, 11);
  }
  // §3 header bar, §8 footer bar, §1 title -------------------------------------
  function drawBarsB(ctx, rack, grain, font) {
    const BAR = { top: PB.bar, bottom: PB.barLo, hi: PB.barHi, lo: PB.barBand, hiW: 2, loW: 1.5, grain };
    const x1 = GB.barRight, t = GB.barTop, h = GB.barH;
    slab(ctx, [[GB.barX, t], [x1 - 3, t], [x1, t + 3], [x1, t + h - 3], [x1 - 3, t + h], [71, t + h], [71, t + 23], [GB.barX, t + 23]], BAR);
    line(ctx, x1 - 0.5, t + 4, x1 - 0.5, t + h - 4, PB.barEdge, 1);
    screw(ctx, 96, t + 15, 6); screw(ctx, 350, t + 15, 6);
    const f = GB.footTop;
    slab(ctx, [[75.5, f], [x1 - 3, f], [x1, f + 3], [x1, f + h - 3], [x1 - 3, f + h], [66, f + h], [66, f + 25.5], [75.5, f + 25.5]], BAR);
    line(ctx, x1 - 0.5, f + 4, x1 - 0.5, f + h - 4, PB.barEdge, 1);
    screw(ctx, 349, f + 15, 6);
    condensed(ctx, rack.header || 'SURVEY TOOLS', 80.5, 28.5, { size: 29, sx: 0.92, font, weight: 700, gradient: [[0, '#5aa6f2'], [1, '#74c2fb']] });
    ctx.fillStyle = PB.underline; ctx.fillRect(79.5, 42, 48, 6);
  }
  // §5 slot lights ---------------------------------------------------------------
  function drawPillB(ctx, cy, on, fx) {
    const cx = GB.pillCX;
    if (on && fx) {
      const g = ctx.createRadialGradient(cx, cy, 8, cx, cy, 64);
      g.addColorStop(0, 'rgba(0,230,246,0.5)'); g.addColorStop(1, 'rgba(0,230,246,0)');
      ctx.fillStyle = g; ctx.fillRect(cx - 66, cy - 66, 132, 132);
    }
    rrect(ctx, cx - 12, cy - 29, 24, 58, 5); ctx.fillStyle = on && fx ? '#052a36' : PB.sock; ctx.fill();
    ctx.strokeStyle = 'rgba(0,0,0,0.65)'; ctx.lineWidth = 1.5; ctx.stroke();
    if (on) {
      if (fx) glowOn(ctx, PB.glow, 16);
      rrect(ctx, cx - 10.5, cy - 28, 21, 56, 5); ctx.fillStyle = PB.coreEdge; ctx.fill(); glowOff(ctx);
      rrect(ctx, cx - 10, cy - 27.5, 20, 55, 4.5); ctx.fillStyle = PB.core; ctx.fill();
      rrect(ctx, cx - 5, cy - 23, 10, 46, 3); ctx.fillStyle = 'rgba(255,255,255,0.22)'; ctx.fill();
    } else {
      rrect(ctx, cx - 8, cy - 25.5, 16, 51, 4); ctx.fillStyle = PB.face; ctx.fill();
      ctx.fillStyle = PB.faceHi; ctx.fillRect(cx - 6, cy - 24.5, 12, 2);
      ctx.fillStyle = PB.faceEdge; ctx.fillRect(cx - 6, cy + 22, 12, 2);
      rrect(ctx, cx - 8, cy - 25.5, 16, 51, 4); ctx.strokeStyle = 'rgba(0,2,6,0.7)'; ctx.lineWidth = 1; ctx.stroke();
    }
  }
  function drawLightsB(ctx, rack, slots, fx) {
    for (let i = 0; i < slots; i++) { const cy = GB.rowTop0 + i * GB.rowPitch + 55; crossfade(ctx, rack.tools[i], t => drawPillB(ctx, cy, !!(t && t.active), fx)); }
  }
  // §6 boxes, §7 labels ---------------------------------------------------------
  function screwB(ctx, x, y, r, rim, dim) {
    ctx.beginPath(); ctx.arc(x, y, r, 0, TAU); ctx.fillStyle = '#02060b'; ctx.fill();
    ctx.beginPath(); ctx.arc(x, y, r - 1.2, 0, TAU); ctx.fillStyle = PB.screwHead; ctx.fill();
    ctx.beginPath(); ctx.arc(x, y, r - 1, 0, TAU); ctx.strokeStyle = rim || PB.screwRim; ctx.lineWidth = 1.2; ctx.stroke();
    ctx.beginPath(); ctx.arc(x - 1.2, y - 1.3, 1.2, 0, TAU); ctx.fillStyle = dim ? 'rgba(120,190,210,0.35)' : 'rgba(225,238,250,0.9)'; ctx.fill();
  }
  function drawRowB(ctx, tool, i, fx, font) {
    const t = GB.rowTop0 + i * GB.rowPitch, bx = GB.boxX, bw = GB.boxW, bh = GB.rowH;
    const has = !!tool, on = has && !!tool.active, sel = on && !!tool.selected;
    const corners = [[bx + 16.5, t + 17.5], [bx + bw - 16.5, t + 17.5], [bx + 16.5, t + bh - 17.5], [bx + bw - 16.5, t + bh - 17.5]];
    if (sel) {
      const ix = bx + 3.5, iy = t + 3.5, iw = bw - 7, ih = bh - 7;
      rrect(ctx, ix, iy, iw, ih, 7); ctx.fillStyle = PB.boxInSel; ctx.fill();
      rrect(ctx, ix - 3, iy - 3, iw + 6, ih + 6, 9); ctx.strokeStyle = PB.selHalo; ctx.lineWidth = 7; ctx.stroke();
      if (fx) glowOn(ctx, PB.sel, 14);
      rrect(ctx, ix, iy, iw, ih, 7); ctx.strokeStyle = PB.sel; ctx.lineWidth = 2; ctx.stroke();
      glowOff(ctx);
      corners.forEach(([x, y]) => screwB(ctx, x, y, 4.5, '#12505f', true));
    } else {
      slab(ctx, [[bx, t, 8], [bx + bw, t, 8], [bx + bw, t + bh, 8], [bx, t + bh, 8]],
        { top: PB.frame, bottom: PB.frameLo, hi: PB.frameHi2, lo: PB.frameBand, hiW: 2, loW: 2 });
      ctx.save(); rrect(ctx, bx, t, bw, bh, 8); ctx.clip();
      ctx.fillStyle = PB.frameHi; ctx.fillRect(bx, t, 1.5, bh); ctx.fillRect(bx, t, bw, 1.5); ctx.restore();
      rrect(ctx, bx + 6.5, t + 6.5, bw - 13, bh - 13, 3); ctx.fillStyle = PB.boxIn; ctx.fill();
      ctx.strokeStyle = 'rgba(0,3,8,0.85)'; ctx.lineWidth = 1.5; ctx.stroke();
      corners.forEach(([x, y]) => screwB(ctx, x, y, 4.5));
      if (!has) {
        const sx = bx + bw / 2 - 20, sy = t + bh / 2 - 20;
        ctx.fillStyle = PB.socket; ctx.fillRect(sx, sy, 40, 40);
        ctx.strokeStyle = PB.socketEdge; ctx.lineWidth = 1; ctx.strokeRect(sx + 0.5, sy + 0.5, 39, 39);
      }
    }
    // label plate with tip
    const lx = GB.labelX, lw = GB.labelW;
    const lab = [[lx, t], [lx + lw - 18.5, t], [lx + lw, t + 24], [lx + lw, t + 86], [lx + lw - 18.5, t + bh], [lx, t + bh]];
    rpoly(ctx, lab); ctx.fillStyle = on ? PB.label : PB.labelOff; ctx.fill();
    ctx.save(); rpoly(ctx, lab); ctx.clip();
    rpoly(ctx, lab); ctx.strokeStyle = on ? PB.labelEdge : PB.labelOffEdge; ctx.lineWidth = 5; ctx.stroke();
    if (on && fx) {
      const g = ctx.createRadialGradient(lx + lw, t + 55, 2, lx + lw, t + 55, 64);
      g.addColorStop(0, 'rgba(0,230,246,0.2)'); g.addColorStop(1, 'rgba(0,230,246,0)');
      ctx.fillStyle = g; ctx.fillRect(lx, t, lw, bh);
    }
    ctx.restore();
    ctx.beginPath(); ctx.arc(lx + 152.5, t + 56, 6, 0, TAU); ctx.fillStyle = PB.hole; ctx.fill();
    ctx.strokeStyle = PB.holeRim; ctx.lineWidth = 1; ctx.stroke();
    if (has) {
      condensed(ctx, tool.name, lx + 14.5, t + 47, { size: 24, sx: 0.78, color: on ? PB.name : PB.nameOff, font, weight: 700 });
      condensed(ctx, tool.type, lx + 14.5, t + 77, { size: 22, sx: 0.76, color: on ? PB.type : PB.typeOff, font, weight: 500 });
    }
    const px = lx + lw + 1.5, py = t + 37;
    const pip = [[px, py], [px + 9, py], [px + 13, py + 4], [px + 13, py + 32], [px + 9, py + 36], [px, py + 36]];
    if (on) {
      if (fx) glowOn(ctx, PB.glow, 14);
      rpoly(ctx, pip); ctx.fillStyle = PB.pipEdge; ctx.fill(); glowOff(ctx);
      rpoly(ctx, [[px + 1, py + 1], [px + 8.5, py + 1], [px + 12, py + 4.5], [px + 12, py + 31.5], [px + 8.5, py + 35], [px + 1, py + 35]]); ctx.fillStyle = PB.pip; ctx.fill();
      ctx.fillStyle = 'rgba(255,255,255,0.35)'; ctx.fillRect(px + 3, py + 5, 4, 26);
    } else {
      rpoly(ctx, pip); ctx.fillStyle = PB.pipOff; ctx.fill();
      ctx.strokeStyle = PB.pipOffEdge; ctx.lineWidth = 1.5; ctx.stroke();
      ctx.fillStyle = 'rgba(160,190,220,0.3)'; ctx.fillRect(px + 1.5, py + 2, 7, 1.5);
    }
  }
  function drawRowsB(ctx, rack, slots, fx, font) {
    for (let i = 0; i < slots; i++) crossfade(ctx, rack.tools[i], t => drawRowB(ctx, t, i, fx, font));
  }
  // §9 icons -----------------------------------------------------------------------
  ICONS.drillStriped = function (ctx, cx, cy, on, fx) {
    const c = on ? PB.head : PB.iconOff, st = on ? PB.stripe : PB.iconOff, el = on ? PB.ellipse : PB.iconOffDk;
    if (on && fx) glowOn(ctx, PB.glow, 6);
    ctx.fillStyle = c; ctx.fillRect(cx - 13, cy - 37, 26, 26);
    glowOff(ctx);
    ctx.fillStyle = on ? PB.boxInSel : PB.boxIn; ctx.fillRect(cx - 3.5, cy - 27.5, 7, 7);
    const shaft = [[cx - 5.5, cy - 11], [cx + 5.5, cy - 11], [cx + 5.5, cy + 30], [cx, cy + 34], [cx - 5.5, cy + 30]];
    ctx.save(); rpoly(ctx, shaft); ctx.clip();
    ctx.translate(cx, cy); ctx.rotate(-Math.PI * 0.28); ctx.fillStyle = st;
    for (let k = -6; k <= 6; k++) ctx.fillRect(-40, k * 10.5, 80, 5.5);
    ctx.restore();
    if (on && fx) glowOn(ctx, PB.glow, 5);
    ctx.strokeStyle = el; ctx.lineWidth = 2.5; ctx.setLineDash([7, 4]); ctx.lineDashOffset = 3;
    ctx.beginPath(); ctx.ellipse(cx, cy + 21.5, 40.5, 16, 0, 0, TAU); ctx.stroke();
    ctx.setLineDash([]); ctx.lineDashOffset = 0; glowOff(ctx);
  };
  ICONS.seismicWide = function (ctx, cx, cy, on, fx) {
    const pts = [[-43, 0], [-28, 0], [-24, -4], [-20, 6], [-15, -11], [-10, 8], [-5, -37], [0, 36], [5, -21], [10, 13], [15, -6], [20, 8], [24, -2], [29, 0], [43, 0]];
    ctx.beginPath(); pts.forEach(([x, y], i) => (i ? ctx.lineTo(cx + x, cy + y + 3) : ctx.moveTo(cx + x, cy + y + 3)));
    ctx.lineJoin = 'miter'; ctx.lineCap = 'round';
    if (on && fx) glowOn(ctx, PB.glow, 5);
    ctx.strokeStyle = on ? PB.trace : '#8fa6bd'; ctx.lineWidth = 2; ctx.stroke(); glowOff(ctx);
  };
  ICONS.sonar = function (ctx, cx, cy, on, fx) {
    const c = on ? PB.head : PB.iconOff, ox = cx - 14, oy = cy + 12;
    if (on && fx) glowOn(ctx, PB.glow, 6);
    ctx.strokeStyle = c; ctx.lineWidth = 3; ctx.lineCap = 'round';
    [15, 28, 41].forEach(r => { ctx.beginPath(); ctx.arc(ox, oy, r, -Math.PI * 0.44, Math.PI * 0.1); ctx.stroke(); });
    ctx.beginPath(); ctx.arc(ox, oy, 5, 0, TAU); ctx.fillStyle = c; ctx.fill();
    ctx.setLineDash([3, 4]); ctx.lineWidth = 1.5; ctx.beginPath(); ctx.moveTo(ox, oy); ctx.lineTo(ox + 40, oy - 30); ctx.stroke(); ctx.setLineDash([]);
    glowOff(ctx);
  };
  function drawIconsB(ctx, rack, fx) {
    rack.tools.forEach((tool, i) => {
      if (!tool) return; const fn = ICONS[tool.icon]; if (!fn) return;
      crossfade(ctx, tool, t => fn(ctx, GB.boxX + GB.boxW / 2, GB.rowTop0 + i * GB.rowPitch + GB.rowH / 2, !!t.active, fx));
    });
  }
  // Hit tests: (x, y) in rack-local units → slot index or -1. Covers spine light, box and label.
  function hitTestA(x, y, slots = 5) {
    if (x < G.spineBodyX || x > G.labelX + G.labelW + 12) return -1;
    const i = Math.floor((y - G.rowTop0) / G.rowPitch), r = y - G.rowTop0 - i * G.rowPitch;
    return i >= 0 && i < slots && r >= 0 && r <= G.rowH ? i : -1;
  }
  function hitTestB(x, y, slots = 5) {
    if (x < GB.bodyX || x > GB.labelX + GB.labelW + 16) return -1;
    const i = Math.floor((y - GB.rowTop0) / GB.rowPitch), r = y - GB.rowTop0 - i * GB.rowPitch;
    return i >= 0 && i < slots && r >= 0 && r <= GB.rowH ? i : -1;
  }
  // public (B) --------------------------------------------------------------------
  function drawRackB(ctx, rack, x, y, opts = {}) {
    const level = opts.level ?? 6, fx = level >= 6, font = opts.font || DEFAULT_FONT;
    const grain = level >= 6 ? opts.grain : null, slots = rack.slots || 5;
    ctx.save(); ctx.translate(x, y);
    if (level >= 1) drawChassisB(ctx, slots, grain);
    if (level >= 2) drawBarsB(ctx, rack, grain, font);
    if (level >= 3) drawLightsB(ctx, rack, slots, fx);
    if (level >= 4) drawRowsB(ctx, rack, slots, fx, font);
    if (level >= 5) drawIconsB(ctx, rack, fx);
    ctx.restore();
  }
  function drawSceneB(ctx, rack, opts = {}) {   // 512×768 design units; scale 2 → 1024×1536 like the reference
    const s = opts.scale ?? 2;
    ctx.save(); ctx.scale(s, s);
    ctx.fillStyle = PB.bg; ctx.fillRect(0, 0, 512, 768);
    drawRackB(ctx, rack, 62, 11.5, opts);
    ctx.restore();
  }

  return { drawRack, drawScene, drawRackB, drawSceneB, hitTestA, hitTestB, paletteB: PB, geometryB: GB, makeGrain,
    util: { rpoly, rrect, text, condensed, glowOn, glowOff, slab, screw, line }, palette: P, geometry: G, icons: ICONS, DEFAULT_FONT, SCENE: { w: 1536, h: 1024 } };
});
