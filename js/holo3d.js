/* Holo3D — rotatable, explodable holographic layered block (software 3D on Canvas 2D)
 *   const model = Holo3D.build();                       // geometry (height fields per bed boundary)
 *   Holo3D.render(ctx, model, state, view);             // state: {yaw,pitch,explode,selected,time,fast}; view: {cx,cy,zoom}
 *   Holo3D.hit(model, x, y) → bed index or -1           // uses the polygons recorded by the last render
 */
(function (root, factory) {
  if (typeof module === 'object' && module.exports) module.exports = factory();
  else root.Holo3D = factory();
})(typeof self !== 'undefined' ? self : this, function () {
  const TAU = Math.PI * 2;
  const FONT = '"JetBrains Mono","IBM Plex Mono",ui-monospace,Menlo,Consolas,monospace';

  // ---------- measured edge profiles (u: 0 left-back → 1 front → 2 right-back; d: fraction of depth) ----------
  const U = [0.01, 0.097, 0.2, 0.306, 0.41, 0.514, 0.618, 0.722, 0.826, 0.93, 1, 1.021, 1.128, 1.255, 1.383, 1.51, 1.638, 1.766, 1.894, 1.979];
  const zip = ds => { const out = []; U.forEach((u, i) => { if (i) { const [pu, pd] = out[out.length - 1]; out.push([pu + (u - pu) * 0.55, pd]); } out.push([u, ds[i]]); }); return out; };
  const PROFILES = [
    zip([-0.002, -0.014, -0.008, -0.041, -0.061, -0.064, -0.038, -0.018, -0.02, -0.015, 0, -0.011, -0.003, 0.002, 0.011, 0.015, -0.002, 0, 0, 0]),
    zip([0.165, 0.168, 0.174, 0.132, 0.133, 0.129, 0.152, 0.182, 0.198, 0.204, 0.203, 0.21, 0.208, 0.208, 0.189, 0.192, 0.195, 0.206, 0.206, 0.208]),
    zip([0.344, 0.35, 0.359, 0.368, 0.374, 0.377, 0.347, 0.353, 0.367, 0.391, 0.398, 0.405, 0.405, 0.407, 0.433, 0.426, 0.404, 0.406, 0.427, 0.429]),
    zip([0.508, 0.514, 0.526, 0.536, 0.524, 0.503, 0.538, 0.573, 0.588, 0.595, 0.598, 0.605, 0.6, 0.598, 0.595, 0.612, 0.616, 0.594, 0.583, 0.582]),
    zip([0.695, 0.703, 0.712, 0.721, 0.688, 0.689, 0.729, 0.767, 0.779, 0.786, 0.789, 0.796, 0.792, 0.79, 0.789, 0.761, 0.782, 0.794, 0.786, 0.782]),
    [[0, 1], [2, 1]],
  ];
  const LAYERS = [
    { name: 'SURFACE', neon: '#3f9bd4', mid: '#1d5d90', deep: '#0c2a4a', line: '#cfefff', mesh: '190,235,255', range: '0 – 200 m', tag: '0x1A', value: 0.78 },
    { name: 'SHALLOW', neon: '#35b3c4', mid: '#157383', deep: '#06303c', line: '#bdeff5', mesh: '180,235,240', range: '200 – 600 m', tag: '0x2C', value: 0.62 },
    { name: 'MID', neon: '#7fa8ae', mid: '#4a6d75', deep: '#22383e', line: '#d6ecef', mesh: '210,230,232', range: '600 m – 1.15 km', tag: '0x3E', value: 0.45 },
    { name: 'DEEP', neon: '#7a92b4', mid: '#3a4f6c', deep: '#1b2636', line: '#d3dcec', mesh: '200,215,235', range: '1.15 – 2.00 km', tag: '0x40', value: 0.31 },
    { name: 'BOREHOLE', neon: '#5c7090', mid: '#2a384c', deep: '#0f1620', line: '#c8d2e2', mesh: '190,205,225', range: '> 2.00 km', tag: '0x57', value: 0.12 },
  ];

  function lerpProfile(prof, u) {
    if (u <= prof[0][0]) return prof[0][1];
    for (let i = 1; i < prof.length; i++) if (u <= prof[i][0]) { const [u0, d0] = prof[i - 1], [u1, d1] = prof[i]; return d0 + (d1 - d0) * (u - u0) / (u1 - u0); }
    return prof[prof.length - 1][1];
  }
  const hex2 = c => [1, 3, 5].map(i => parseInt(c.slice(i, i + 2), 16));
  const mix = (a, b, t) => { const A = hex2(a), B = hex2(b); return `rgb(${A.map((v, i) => Math.round(v + (B[i] - v) * t)).join(',')})`; };
  const shade = (c, f) => { const A = hex2(c); return `rgb(${A.map(v => Math.round(Math.min(255, v * f))).join(',')})`; };
  const hash = (a, b, c) => { let h = (a * 73856093) ^ (b * 19349663) ^ (c * 83492791); h = (h ^ (h >>> 13)) * 1274126177; return ((h ^ (h >>> 16)) >>> 0) / 4294967296; };

  // ---------- 1. model: height fields on a grid ----------
  function build(opts = {}) {
    const W = opts.width ?? 740, D = opts.depth ?? 700, NX = opts.nx ?? 16, NZ = opts.nz ?? 13;
    const profs = opts.profiles || PROFILES, layers = opts.layers || LAYERS;
    const fields = profs.map(p => { const c = lerpProfile(p, 1); return (x, z) => lerpProfile(p, x) + lerpProfile(p, 1 + z) - c; });
    // pts[k][i][j] = model point of boundary k at grid (i,j); x runs along the L→F edge, z along F→R
    const pts = fields.map(f => { const g = []; for (let i = 0; i <= NX; i++) { g.push([]); for (let j = 0; j <= NZ; j++) { const x = i / NX, z = j / NZ; g[i].push([(x - 0.5) * W, -f(x, z) * D, (z - 0.5) * W]); } } return g; });
    return { W, D, NX, NZ, pts, fields, layers, hits: [], seed: opts.seed ?? 11 };
  }

  // ---------- 2. camera ----------
  function camera(state, view) {
    const yaw = state.yaw || 0, pitch = state.pitch ?? 0.45, zoom = view.zoom ?? 1;
    const a = Math.PI * 0.75 + yaw;                          // yaw 0 ≈ the reference view (front edge toward the viewer)
    const f = [Math.cos(a), Math.sin(a)], r = [f[1], -f[0]], cp = Math.cos(pitch), sp = Math.sin(pitch);
    const light = [-0.55, 0.65, -0.5];
    return {
      f, cp, sp, zoom, light,
      project(p) { const y = p[1] - (view.centerY ?? 0), h = p[0] * f[0] + p[2] * f[1]; return [(p[0] * r[0] + p[2] * r[1]) * zoom + view.cx, (-y * cp - h * sp) * zoom + view.cy, h * cp - y * sp]; },
      facing(n) { return n[0] * f[0] + n[2] * f[1] < 0; },   // wall normal points toward the camera
    };
  }

  // ---------- 3. render ----------
  function render(ctx, model, state, view) {
    const { W, D, NX, NZ, pts, layers } = model, cam = camera(state, view), t = state.time || 0, fast = !!state.fast;
    const e = state.explode || 0, sel = state.selected ?? -1, gap = D * 0.16 * e;
    if (view.centerY == null) view.centerY = -D / 2;
    const alphaOf = k => (sel < 0 || k === sel ? 1 : 0.3);
    const offY = k => (2 - k) * gap;
    model.hits = [];
    const path = pts2 => { ctx.beginPath(); pts2.forEach(([x, y], i) => (i ? ctx.lineTo(x, y) : ctx.moveTo(x, y))); };
    const proj = (p, dy) => cam.project([p[0], p[1] + dy, p[2]]);
    // walls: [name, normal, top-edge sampler (returns the grid points along the edge)]
    const WALLS = [
      ['A', [0, 0, -1], k => Array.from({ length: NX + 1 }, (_, i) => pts[k][i][0])],
      ['B', [1, 0, 0], k => Array.from({ length: NZ + 1 }, (_, j) => pts[k][NX][j])],
      ['C', [0, 0, 1], k => Array.from({ length: NX + 1 }, (_, i) => pts[k][i][NZ])],
      ['D', [-1, 0, 0], k => Array.from({ length: NZ + 1 }, (_, j) => pts[k][0][j])],
    ];
    const lit = n => 0.62 + 0.38 * Math.max(0, n[0] * cam.light[0] + n[1] * cam.light[1] + n[2] * cam.light[2]);
    const allPts = [];

    const GHOST = 0.3, outer = ctx;
    // paint one bed, fully opaque, into any context (walls, top surface, mesh, edges)
    const paintLayer = (ctx, k) => {
      const path = pts2 => { ctx.beginPath(); pts2.forEach(([x, y], i) => (i ? ctx.lineTo(x, y) : ctx.moveTo(x, y))); };   // bound to THIS context
      const ly = layers[k], dy = offY(k), showTop = k === 0 || e > 0.02;
      ctx.lineJoin = 'round'; ctx.lineCap = 'round';
      // ---- walls ----
      WALLS.forEach(([name, n, edge]) => {
        const top = edge(k).map(p => proj(p, dy)), bot = edge(k + 1).map(p => proj(p, dy));
        top.forEach(p => allPts.push(p)); if (k === layers.length - 1) bot.forEach(p => allPts.push(p));
        if (!cam.facing(n)) return;
        const poly = [...top, ...bot.slice().reverse()], f = lit(n);
        const ys = poly.map(p => p[1]), y0 = Math.min(...ys), y1 = Math.max(...ys);
        const g = ctx.createLinearGradient(0, y0, 0, y1);
        g.addColorStop(0, shade(ly.neon, f)); g.addColorStop(0.2, shade(ly.mid, f)); g.addColorStop(0.75, shade(ly.deep, f)); g.addColorStop(1, shade(ly.deep, f));
        path(poly); ctx.closePath(); ctx.fillStyle = g; ctx.fill();
        model.hits.push({ k, poly });
        // mesh
        ctx.save(); path(poly); ctx.closePath(); ctx.clip();
        ctx.strokeStyle = `rgba(${ly.mesh},${0.22 * f})`; ctx.lineWidth = 1;
        for (let i = 1; i < top.length - 1; i++) { ctx.beginPath(); ctx.moveTo(top[i][0], top[i][1]); ctx.lineTo(bot[i][0], bot[i][1]); ctx.stroke(); }
        const rows = Math.max(2, Math.round((y1 - y0) / 26));
        ctx.strokeStyle = `rgba(${ly.mesh},${0.14 * f})`;
        for (let m = 1; m < rows; m++) { const q = m / rows; ctx.beginPath(); top.forEach((p, i) => { const x = p[0] + (bot[i][0] - p[0]) * q, y = p[1] + (bot[i][1] - p[1]) * q; i ? ctx.lineTo(x, y) : ctx.moveTo(x, y); }); ctx.stroke(); }
        if (!fast) for (let i = 0; i < top.length - 1; i++) for (let m = 0; m < rows; m++) {
          const h = hash(i + 1, m + 7, k * 4 + name.charCodeAt(0)); if (h > 0.18) continue;
          const a0 = (m + 0.35) / rows, a1 = (m + 0.65) / rows, P = (idx, q) => [top[idx][0] + (bot[idx][0] - top[idx][0]) * q, top[idx][1] + (bot[idx][1] - top[idx][1]) * q];
          const l0 = P(i, a0), l1 = P(i + 1, a0), l2 = P(i + 1, a1), l3 = P(i, a1), s0 = 0.3, s1 = 0.7;
          const Q = (u, pa, pb) => [pa[0] + (pb[0] - pa[0]) * u, pa[1] + (pb[1] - pa[1]) * u];
          path([Q(s0, l0, l1), Q(s1, l0, l1), Q(s1, l3, l2), Q(s0, l3, l2)]); ctx.closePath(); ctx.fillStyle = h < 0.14 ? 'rgba(0,8,20,0.32)' : 'rgba(200,235,255,0.14)'; ctx.fill();
        }
        ctx.restore();
      });
      // ---- top surface (slope-shaded cells) ----
      const P = pts[k].map(col => col.map(p => proj(p, dy)));
      if (showTop) {
        for (let i = 0; i < NX; i++) for (let j = 0; j < NZ; j++) {
          const a = pts[k][i][j], b = pts[k][i + 1][j], c = pts[k][i + 1][j + 1], d = pts[k][i][j + 1];
          const ex = [b[0] - a[0], b[1] - a[1], b[2] - a[2]], ez = [d[0] - a[0], d[1] - a[1], d[2] - a[2]];
          const n = [ez[1] * ex[2] - ez[2] * ex[1], ez[2] * ex[0] - ez[0] * ex[2], ez[0] * ex[1] - ez[1] * ex[0]];   // up-facing normal
          const len = Math.hypot(...n) || 1, f = 0.45 + 0.55 * Math.max(0, (n[0] * cam.light[0] + n[1] * cam.light[1] + n[2] * cam.light[2]) / len);
          const shadeK = k === 0 ? mix('#123f6e', '#3f92d0', f) : mix(ly.deep, ly.neon, f * 0.85);
          path([P[i][j], P[i + 1][j], P[i + 1][j + 1], P[i][j + 1]]); ctx.closePath(); ctx.fillStyle = shadeK; ctx.fill();
          if (!fast && hash(i + 3, j + 5, 99 + k) < 0.12) { ctx.fillStyle = 'rgba(0,10,30,0.28)'; const m = (u, pa, pb) => [pa[0] + (pb[0] - pa[0]) * u, pa[1] + (pb[1] - pa[1]) * u]; const q0 = m(0.3, P[i][j], P[i + 1][j]), q1 = m(0.7, P[i][j], P[i + 1][j]), q2 = m(0.7, P[i][j + 1], P[i + 1][j + 1]), q3 = m(0.3, P[i][j + 1], P[i + 1][j + 1]); path([m(0.3, q0, q3), m(0.3, q1, q2), m(0.7, q1, q2), m(0.7, q0, q3)]); ctx.closePath(); ctx.fill(); }
        }
        ctx.strokeStyle = `rgba(${ly.mesh},0.3)`; ctx.lineWidth = 1;
        for (let i = 1; i < NX; i++) { path(P[i]); ctx.stroke(); }
        for (let j = 1; j < NZ; j++) { path(P.map(col => col[j])); ctx.stroke(); }
        const outline = [...P.map(c => c[0]), ...P[NX].slice(1), ...P.map(c => c[NZ]).reverse().slice(1), ...P[0].slice().reverse().slice(1)];
        model.hits.push({ k, poly: outline });
      }
      // ---- edges ----
      const stroke = (pts2, color, w, blur) => { path(pts2); ctx.shadowColor = '#6fd8ff'; ctx.shadowBlur = fast ? 0 : blur; ctx.strokeStyle = color; ctx.lineWidth = w; ctx.stroke(); ctx.shadowBlur = 0; };
      WALLS.forEach(([name, n, edge]) => {
        const top = edge(k).map(p => proj(p, dy)), vis = cam.facing(n);
        if (vis) stroke(top, k === 0 ? '#e6ffff' : ly.line, k === 0 ? 2.2 : 1.5, k === 0 ? 16 : 10);
        else if (showTop) stroke(top, `rgba(${ly.mesh},0.45)`, 1, 0);
        if (k === layers.length - 1 && vis) stroke(edge(k + 1).map(p => proj(p, dy)), 'rgba(160,190,215,0.6)', 1.1, 3);
      });
      // vertical corner edges: bright where two visible walls meet, dim on the silhouette
      const corners = [[0, 0, ['A', 'D']], [NX, 0, ['A', 'B']], [NX, NZ, ['B', 'C']], [0, NZ, ['C', 'D']]];
      corners.forEach(([i, j, walls]) => {
        const v = walls.map(w => cam.facing(WALLS.find(x => x[0] === w)[1]));
        if (!v[0] && !v[1]) return;
        const a = proj(pts[k][i][j], dy), b = proj(pts[k + 1][i][j], dy);
        stroke([a, b], v[0] && v[1] ? '#e6ffff' : 'rgba(180,215,240,0.55)', v[0] && v[1] ? 2.2 : 1.1, v[0] && v[1] ? 14 : 2);
      });
    };

    ctx.save();
    if (sel < 0) {
      for (let k = layers.length - 1; k >= 0; k--) paintLayer(ctx, k);
    } else {
      // ghost groups are rendered opaque off-screen and composited ONCE at ghost alpha, so stacked ghosts never accumulate;
      // beds above the focus stay above it (true 3D order) yet the focus shows through them.
      const below = [], above = [];
      for (let k = layers.length - 1; k > sel; k--) below.push(k);
      for (let k = sel - 1; k >= 0; k--) above.push(k);
      const buf = getBuffer(model, ctx);
      const group = (ks, slot) => {
        if (!ks.length) return;
        if (!buf) { ctx.globalAlpha = GHOST; ks.forEach(k => paintLayer(ctx, k)); ctx.globalAlpha = 1; return; }   // fallback: per-bed alpha
        const g = buf[slot];
        g.save(); g.setTransform(1, 0, 0, 1, 0, 0); g.clearRect(0, 0, g.canvas.width, g.canvas.height); g.restore();
        ks.forEach(k => paintLayer(g, k));
        ctx.save(); ctx.setTransform(1, 0, 0, 1, 0, 0); ctx.globalAlpha = GHOST; ctx.drawImage(g.canvas, 0, 0); ctx.restore();
      };
      group(below, 0);
      paintLayer(ctx, sel);
      group(above, 1);
    }
    ctx.globalAlpha = 1;
    model.bounds = allPts.reduce((b, p) => [Math.min(b[0], p[0]), Math.min(b[1], p[1]), Math.max(b[2], p[0]), Math.max(b[3], p[1])], [1e9, 1e9, -1e9, -1e9]);
    model.hull = hull(allPts);
    model.cam = cam; model.offY = offY; model.alphaOf = alphaOf;
    ctx.restore();
  }

  let makeCanvas = (w, h) => { if (typeof document === 'undefined') return null; const c = document.createElement('canvas'); c.width = w; c.height = h; return c; };
  function getBuffer(model, ctx) {
    const w = ctx.canvas.width, h = ctx.canvas.height;
    if (!model._buf || model._buf[0].canvas.width !== w || model._buf[0].canvas.height !== h) {
      const a = makeCanvas(w, h), b = makeCanvas(w, h); if (!a || !b) return null;
      model._buf = [a.getContext('2d'), b.getContext('2d')];
    }
    const t = ctx.getTransform ? ctx.getTransform() : null;
    model._buf.forEach(g => { if (t) g.setTransform(t.a, t.b, t.c, t.d, t.e, t.f); else g.setTransform(1, 0, 0, 1, 0, 0); });
    return model._buf;
  }
  function hull(points) {   // monotone chain convex hull
    const P = points.slice().sort((a, b) => a[0] - b[0] || a[1] - b[1]);
    const cross = (o, a, b) => (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0]);
    const lower = [], upper = [];
    for (const p of P) { while (lower.length >= 2 && cross(lower[lower.length - 2], lower[lower.length - 1], p) <= 0) lower.pop(); lower.push(p); }
    for (const p of P.reverse()) { while (upper.length >= 2 && cross(upper[upper.length - 2], upper[upper.length - 1], p) <= 0) upper.pop(); upper.push(p); }
    return lower.slice(0, -1).concat(upper.slice(0, -1));
  }

  // ---------- 4. hit test against the last frame's polygons ----------
  function inside(poly, x, y) { let c = false; for (let i = 0, j = poly.length - 1; i < poly.length; j = i++) { const [xi, yi] = poly[i], [xj, yj] = poly[j]; if ((yi > y) !== (yj > y) && x < (xj - xi) * (y - yi) / (yj - yi) + xi) c = !c; } return c; }
  function hit(model, x, y) {
    let any = -1;
    for (let i = model.hits.length - 1; i >= 0; i--) {
      const h = model.hits[i]; if (!inside(h.poly, x, y)) continue;
      if (!model.alphaOf || model.alphaOf(h.k) >= 1) return h.k;   // a solid bed under the pointer wins
      if (any < 0) any = h.k;
    }
    return any;
  }

  // ---------- 5. HUD (projected, so it follows the rotation) ----------
  function drawHud(ctx, model, state, view, hud = {}) {
    const { W, D, NX, NZ, pts, layers, cam, fields } = model; if (!cam) return;
    const t = state.time || 0, s = Math.min(1.4, view.zoom / 0.94), fs = Math.max(9, 15 * s), cyan = '#5ff0ff', small = s < 0.5;
    const path = pts2 => { ctx.beginPath(); pts2.forEach(([x, y], i) => (i ? ctx.lineTo(x, y) : ctx.moveTo(x, y))); };
    const proj = (p, dy = 0) => cam.project([p[0], p[1] + dy, p[2]]);
    const [bx0, by0, bx1, by1] = model.bounds;
    ctx.save(); ctx.lineCap = 'round'; ctx.lineJoin = 'round'; ctx.font = `500 ${fs}px ${FONT}`; ctx.textBaseline = 'alphabetic';
    if (hud.brackets !== false) {
      const m = 26 * s, x0 = bx0 - m, x1 = bx1 + m, y0 = by0 - m, y1 = by1 + m, leg = 42 * s;
      ctx.strokeStyle = cyan; ctx.lineWidth = 2 * s; ctx.shadowColor = cyan; ctx.shadowBlur = 8;
      [[x0, y0, 1, 1], [x1, y0, -1, 1], [x0, y1, 1, -1], [x1, y1, -1, -1]].forEach(([x, y, dx, dy]) => { ctx.beginPath(); ctx.moveTo(x, y + dy * leg); ctx.lineTo(x, y); ctx.lineTo(x + dx * leg, y); ctx.stroke(); });
      ctx.shadowBlur = 0;
    }
    // base ring: a circle on the ground plane under the (possibly exploded) stack
    if (hud.base !== false) {
      const yb = -D + model.offY(layers.length - 1) - 40, r = W * 0.78, ring = a => proj([Math.cos(a) * r, yb, Math.sin(a) * r]);
      ctx.save(); ctx.beginPath(); ctx.rect(-1e5, -1e5, 2e5, 2e5); path(model.hull); ctx.closePath(); ctx.clip('evenodd');
      ctx.strokeStyle = 'rgba(95,240,255,0.35)'; ctx.lineWidth = 1.2; ctx.setLineDash([8 * s, 6 * s]);
      path(Array.from({ length: 73 }, (_, i) => ring(i / 72 * TAU))); ctx.stroke(); ctx.setLineDash([]);
      ctx.strokeStyle = 'rgba(95,240,255,0.5)';
      for (let k = 0; k < 36; k++) { const a = k * TAU / 36, r1 = k % 9 === 0 ? 1.1 : 1.04, p = ring(a), q = proj([Math.cos(a) * r * r1, yb, Math.sin(a) * r * r1]); ctx.beginPath(); ctx.moveTo(p[0], p[1]); ctx.lineTo(q[0], q[1]); ctx.stroke(); }
      ctx.restore();
    }
    // reticle: true circles on the top surface at the drill site
    if (hud.reticle !== false) {
      const dy0 = model.offY(0) + 2, onSurf = (x, z) => proj([(x - 0.5) * W, -fields[0](x, z) * D, (z - 0.5) * W], dy0);
      const rr = 0.16, a0 = t * 0.9, ring = (rad, a1, a2, n = 40) => Array.from({ length: n + 1 }, (_, i) => { const a = a1 + (a2 - a1) * i / n; return onSurf(0.5 + Math.cos(a) * rad, 0.5 + Math.sin(a) * rad); });
      ctx.globalAlpha = model.alphaOf(0);
      ctx.lineWidth = 1.2; ctx.strokeStyle = 'rgba(160,250,255,0.55)'; path(ring(rr, 0, TAU, 60)); ctx.stroke();
      ctx.setLineDash([5 * s, 5 * s]); ctx.strokeStyle = 'rgba(160,250,255,0.4)'; path(ring(rr * 0.66, 0, TAU, 60)); ctx.stroke(); ctx.setLineDash([]);
      ctx.strokeStyle = 'rgba(160,250,255,0.6)';
      for (let k = 0; k < 24; k++) { const a = k * TAU / 24, r1 = k % 6 === 0 ? 1.14 : 1.06, p = onSurf(0.5 + Math.cos(a) * rr, 0.5 + Math.sin(a) * rr), q = onSurf(0.5 + Math.cos(a) * rr * r1, 0.5 + Math.sin(a) * rr * r1); ctx.beginPath(); ctx.moveTo(p[0], p[1]); ctx.lineTo(q[0], q[1]); ctx.stroke(); }
      ctx.strokeStyle = cyan; ctx.lineWidth = 2.5 * s; ctx.shadowColor = cyan; ctx.shadowBlur = 12;
      path(ring(rr, a0, a0 + 1.1, 16)); ctx.stroke(); path(ring(rr * 0.66, a0 + Math.PI, a0 + Math.PI + 0.7, 12)); ctx.stroke();
      ctx.lineWidth = 1; path([onSurf(0.5 - rr * 1.25, 0.5), onSurf(0.5 - rr * 0.3, 0.5)]); ctx.stroke(); path([onSurf(0.5 + rr * 0.3, 0.5), onSurf(0.5 + rr * 1.25, 0.5)]); ctx.stroke();
      path([onSurf(0.5, 0.5 - rr * 1.3), onSurf(0.5, 0.5 - rr * 0.3)]); ctx.stroke(); path([onSurf(0.5, 0.5 + rr * 0.3), onSurf(0.5, 0.5 + rr * 1.3)]); ctx.stroke();
      const c = onSurf(0.5, 0.5), pulse = 0.5 + 0.5 * Math.sin(t * 3);
      ctx.fillStyle = `rgba(255,255,255,${0.6 + 0.4 * pulse})`; ctx.beginPath(); ctx.arc(c[0], c[1], 3.5 * s, 0, TAU); ctx.fill();
      ctx.strokeStyle = `rgba(95,240,255,${0.7 - 0.6 * pulse})`; ctx.lineWidth = 1.5; path(ring(rr * (0.12 + 0.35 * pulse), 0, TAU, 40)); ctx.stroke();
      ctx.shadowBlur = 0;
      if (!small) {
        ctx.strokeStyle = 'rgba(160,250,255,0.6)'; ctx.lineWidth = 1; ctx.beginPath(); ctx.moveTo(c[0], c[1]); ctx.lineTo(c[0] + 70 * s, c[1] - 95 * s); ctx.lineTo(c[0] + 150 * s, c[1] - 95 * s); ctx.stroke();
        ctx.fillStyle = cyan; ctx.font = `600 ${fs * 0.9}px ${FONT}`; ctx.textAlign = 'left'; ctx.fillText('DRILL SITE  ·  LOCK', c[0] + 74 * s, c[1] - 101 * s);
        ctx.fillStyle = 'rgba(160,250,255,0.75)'; ctx.font = `500 ${fs * 0.75}px ${FONT}`; ctx.fillText(`θ ${((a0 * 57.3) % 360).toFixed(1).padStart(5, '0')}°   YAW ${(((state.yaw || 0) * 57.3) % 360).toFixed(0)}°`, c[0] + 74 * s, c[1] - 86 * s);
      }
      ctx.globalAlpha = 1;
    }
    // callouts anchored to the screen-rightmost corner of each bed at mid depth
    if (hud.callouts !== false) {
      const x1 = bx1 + 88 * s, bw = 272 * s, bh = 48 * s;
      layers.forEach((ly, k) => {
        const dy = model.offY(k), cs = [[0, 0], [NX, 0], [NX, NZ], [0, NZ]].map(([i, j]) => { const a = pts[k][i][j], b = pts[k + 1][i][j]; return proj([a[0], (a[1] + b[1]) / 2, a[2]], dy); });
        const p = cs.reduce((m, q) => (q[0] > m[0] ? q : m)), y = p[1];
        ctx.globalAlpha = model.alphaOf(k) < 1 ? 0.35 : 1;
        ctx.strokeStyle = ly.neon; ctx.lineWidth = 1.2; ctx.shadowColor = ly.neon; ctx.shadowBlur = 6;
        ctx.beginPath(); ctx.moveTo(p[0], p[1]); ctx.lineTo(bx1 + 24 * s, p[1]); ctx.lineTo(x1, y); ctx.lineTo(x1 + 10 * s, y); ctx.stroke();
        ctx.fillStyle = ly.neon; ctx.beginPath(); ctx.arc(p[0], p[1], 3.2 * s, 0, TAU); ctx.fill(); ctx.shadowBlur = 0;
        const bx = x1 + 10 * s, by = y - bh / 2;
        ctx.fillStyle = 'rgba(2,10,22,0.86)'; ctx.fillRect(bx, by, bw, bh);
        ctx.strokeStyle = `rgba(${ly.mesh},0.45)`; ctx.lineWidth = 1; ctx.strokeRect(bx + 0.5, by + 0.5, bw - 1, bh - 1);
        ctx.fillStyle = ly.neon; ctx.fillRect(bx, by, 3 * s, bh);
        ctx.strokeStyle = ly.neon; ctx.lineWidth = 1.5; ctx.beginPath(); ctx.moveTo(bx + bw - 12 * s, by); ctx.lineTo(bx + bw, by); ctx.lineTo(bx + bw, by + 12 * s); ctx.stroke();
        ctx.fillStyle = ly.neon; ctx.font = `700 ${fs}px ${FONT}`; ctx.textAlign = 'left'; ctx.fillText(`L${k + 1} · ${ly.name}${k === state.selected ? '  ◆' : ''}`, bx + 12 * s, by + 19 * s);
        ctx.fillStyle = `rgba(${ly.mesh},0.8)`; ctx.font = `500 ${fs * 0.8}px ${FONT}`; ctx.fillText(ly.range, bx + 12 * s, by + 37 * s);
        ctx.textAlign = 'right'; ctx.fillStyle = `rgba(${ly.mesh},0.6)`; ctx.fillText(ly.tag, bx + bw - 10 * s, by + 19 * s);
        const segs = 8, sw = 12 * s, sy = by + 28 * s;
        for (let i = 0; i < segs; i++) { ctx.fillStyle = i < Math.round(ly.value * segs) ? ly.neon : `rgba(${ly.mesh},0.18)`; ctx.fillRect(bx + bw - 10 * s - (segs - i) * (sw + 2 * s), sy, sw, 8 * s); }
        ctx.textAlign = 'left';
      });
      ctx.globalAlpha = 1;
    }
    ctx.restore();
  }

  // ---------- 6. interaction controller (attach to a canvas) ----------
  function attach(canvas, opts) {
    const state = Object.assign({ yaw: -0.1, pitch: 0.42, explode: 0, target: 0, selected: -1, time: 0, fast: false, auto: true }, opts.state || {});
    let down = null, moved = false, last = performance.now();
    const toDesign = e => { const r = canvas.getBoundingClientRect(); return [(e.clientX - r.left) / r.width * opts.width, (e.clientY - r.top) / r.height * opts.height]; };
    const select = k => { if (k >= 0 && k !== state.selected) { state.selected = k; state.target = 1; } else { state.selected = -1; state.target = 0; } };
    canvas.addEventListener('pointerdown', e => {
      const p = toDesign(e); if (opts.inside && !opts.inside(p)) return;
      down = { p, cx: e.clientX, cy: e.clientY, t: performance.now(), yaw: state.yaw, pitch: state.pitch }; moved = false; state.auto = false;
      try { canvas.setPointerCapture(e.pointerId); } catch (_) {}
      e.preventDefault();
    });
    canvas.addEventListener('pointermove', e => {
      if (!down) return; const dx = e.clientX - down.cx, dy = e.clientY - down.cy;   // CSS pixels: same feel on phone and desktop
      if (Math.hypot(dx, dy) > 10) moved = true;
      if (moved) { state.yaw = down.yaw + dx * 0.012; state.pitch = Math.max(0.18, Math.min(1.25, down.pitch + dy * 0.009)); state.fast = true; }
    });
    const finish = e => {
      if (!down) return;
      const tap = !moved && performance.now() - down.t < 600;
      if (tap) select(opts.hit(toDesign(e)));
      down = null; state.fast = false;
    };
    canvas.addEventListener('pointerup', finish);
    canvas.addEventListener('pointercancel', finish);
    canvas.addEventListener('wheel', e => { if (opts.onZoom) { e.preventDefault(); opts.onZoom(Math.exp(-e.deltaY * 0.001)); } }, { passive: false });
    window.addEventListener('keydown', e => { if (e.key >= '1' && e.key <= '5' && opts.keys !== false) select(+e.key - 1); if (e.key === 'Escape') select(-1); });
    function tick(now) {
      const dt = Math.min(0.05, (now - last) / 1000); last = now; state.time = now / 1000;
      state.explode += (state.target - state.explode) * Math.min(1, dt * 7);
      if (Math.abs(state.target - state.explode) < 0.002) state.explode = state.target;
      if (state.auto) state.yaw += dt * 0.12;
    }
    return { state, tick };
  }

  return { build, render, drawHud, hit, attach, camera, LAYERS, PROFILES, setCanvasFactory(f) { makeCanvas = f; } };
});
