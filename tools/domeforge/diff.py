#!/usr/bin/env python3
"""DomeForge diff: percent of pixels whose RGBA differs by more than 24 on any
channel (alpha included -- sprites are transparent outside the dome), plus a
heatmap and the largest single difference. Usage: diff.py ref.png port.png heat.png [gate%]"""
import sys
import numpy as np
from PIL import Image

ref, port, heat = sys.argv[1:4]
gate = float(sys.argv[4]) if len(sys.argv) > 4 else 1.0
a = np.asarray(Image.open(ref).convert('RGBA'), dtype=np.int16)
b = np.asarray(Image.open(port).convert('RGBA'), dtype=np.int16)
if a.shape != b.shape:
    print(f"SIZE MISMATCH ref{a.shape} port{b.shape}"); sys.exit(2)
d = np.abs(a - b).max(axis=2)
bad = d > 24
n = int(bad.sum()); h, w = d.shape
out = np.empty((h, w, 3), dtype=np.uint8)
g = np.clip(20 + d * 4, 0, 255).astype(np.uint8)
out[..., 0] = g; out[..., 1] = g; out[..., 2] = g
out[bad] = (255, 60, 0)
Image.fromarray(out).save(heat)
pct = 100.0 * n / (w * h)
exact = 100.0 * (d == 0).sum() / (w * h)
print(f"differing pixels: {n} / {w*h} = {pct:.3f}%  (exact match {exact:.1f}%, max diff {int(d.max())})  heatmap -> {heat}")
sys.exit(0 if pct < gate else 1)
