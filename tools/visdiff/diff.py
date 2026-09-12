#!/usr/bin/env python3
"""Percent differing pixels + a heatmap (spec 5)."""
import sys
import numpy as np
from PIL import Image

ref, port, heat = sys.argv[1], sys.argv[2], sys.argv[3]
a = np.asarray(Image.open(ref).convert('RGB'), dtype=np.int16)
b = np.asarray(Image.open(port).convert('RGB'), dtype=np.int16)
if a.shape != b.shape:
    print(f"SIZE MISMATCH ref{a.shape} port{b.shape}"); sys.exit(2)
d = np.abs(a - b).max(axis=2)
bad = d > 24
n = int(bad.sum())
h, w = d.shape
out = np.empty((h, w, 3), dtype=np.uint8)
grey = np.clip(20 + d, 0, 255).astype(np.uint8)
out[..., 0] = grey; out[..., 1] = grey; out[..., 2] = grey
out[bad, 0] = 255
out[bad, 1] = np.clip(d[bad] * 3, 0, 255).astype(np.uint8)
out[bad, 2] = 0
Image.fromarray(out).save(heat)
pct = 100.0 * n / (w * h)
print(f"differing pixels: {n} / {w*h} = {pct:.2f}%   heatmap -> {heat}")
sys.exit(0 if pct < 2.0 else 1)
