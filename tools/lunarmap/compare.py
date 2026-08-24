#!/usr/bin/env python3
"""Composite lunar_map renders into one side-by-side comparison image.

The container running this project has neither PIL nor numpy, so PNG
decode and encode are both hand-rolled here (zlib + per-scanline
filters + CRC32 chunks). Kept in the repo rather than a scratch dir
because comparison sheets are how this tool's visual work gets judged.

Each panel is tagged with a colour swatch in its top-left corner, in
order: cyan, orange, green, magenta, yellow — so the panels stay
identifiable without relying on a caption.

Usage:
    python3 tools/lunarmap/compare.py PANEL1.png PANEL2.png [...] OUT.png
"""

import struct
import sys
import zlib

SWATCHES = [(60, 210, 230), (245, 150, 45), (110, 220, 120),
            (230, 110, 220), (240, 230, 90)]


def DecodeRgb(path):
    """Decode a PNG to (rgb_bytes, width, height)."""
    data = open(path, 'rb').read()
    assert data[:8] == b'\x89PNG\r\n\x1a\n', f'{path}: not a PNG'
    pos = 8
    idat = bytearray()
    w = h = bitdepth = colortype = None
    while pos < len(data):
        length = struct.unpack('>I', data[pos:pos + 4])[0]
        ctype = data[pos + 4:pos + 8]
        body = data[pos + 8:pos + 8 + length]
        if ctype == b'IHDR':
            w, h, bitdepth, colortype = struct.unpack('>IIBB', body[:10])
        elif ctype == b'IDAT':
            idat += body
        elif ctype == b'IEND':
            break
        pos += 12 + length
    assert bitdepth == 8, f'{path}: bitdepth {bitdepth} unsupported'
    channels = {0: 1, 2: 3, 4: 2, 6: 4}[colortype]
    raw = zlib.decompress(bytes(idat))

    stride = w * channels
    out = bytearray(w * h * 3)
    prev = bytearray(stride)
    p = 0
    for y in range(h):
        ft = raw[p]
        p += 1
        line = bytearray(raw[p:p + stride])
        p += stride
        if ft == 1:
            for i in range(channels, stride):
                line[i] = (line[i] + line[i - channels]) & 0xFF
        elif ft == 2:
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 0xFF
        elif ft == 3:
            for i in range(stride):
                left = line[i - channels] if i >= channels else 0
                line[i] = (line[i] + ((left + prev[i]) >> 1)) & 0xFF
        elif ft == 4:
            for i in range(stride):
                a = line[i - channels] if i >= channels else 0
                b = prev[i]
                c = prev[i - channels] if i >= channels else 0
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pr) & 0xFF
        base = y * w * 3
        if channels >= 3:
            for x in range(w):
                i = x * channels
                out[base + x * 3] = line[i]
                out[base + x * 3 + 1] = line[i + 1]
                out[base + x * 3 + 2] = line[i + 2]
        else:
            for x in range(w):
                v = line[x * channels]
                out[base + x * 3] = v
                out[base + x * 3 + 1] = v
                out[base + x * 3 + 2] = v
        prev = line
    return out, w, h


def Chunk(ctype, body):
    return (struct.pack('>I', len(body)) + ctype + body +
            struct.pack('>I', zlib.crc32(ctype + body) & 0xFFFFFFFF))


def EncodeRgb(path, rows, w, h):
    raw = bytearray()
    stride = w * 3
    for y in range(h):
        raw.append(0)                      # filter type 0 (none)
        raw += rows[y * stride:(y + 1) * stride]
    png = b'\x89PNG\r\n\x1a\n'
    png += Chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
    png += Chunk(b'IDAT', zlib.compress(bytes(raw), 6))
    png += Chunk(b'IEND', b'')
    open(path, 'wb').write(png)


def Swatch(rows, w, x0, y0, size, rgb):
    for y in range(y0, y0 + size):
        base = y * w * 3
        for x in range(x0, x0 + size):
            i = base + x * 3
            rows[i], rows[i + 1], rows[i + 2] = rgb


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        return 1
    paths, opath = sys.argv[1:-1], sys.argv[-1]
    panels = [DecodeRgb(p) for p in paths]
    h = panels[0][2]
    assert all(p[2] == h for p in panels), 'panels must share height'

    gap = 8
    w = sum(p[1] for p in panels) + gap * (len(panels) - 1)
    out = bytearray(w * h * 3)
    ostride = w * 3

    xoff = 0
    for idx, (rows, pw, _) in enumerate(panels):
        pstride = pw * 3
        for y in range(h):
            o = y * ostride + xoff * 3
            out[o:o + pstride] = rows[y * pstride:(y + 1) * pstride]
        Swatch(out, w, xoff + 14, 60, 26, SWATCHES[idx % len(SWATCHES)])
        xoff += pw
        if idx != len(panels) - 1:
            for y in range(h):
                o = y * ostride
                for x in range(xoff, xoff + gap):
                    i = o + x * 3
                    out[i] = out[i + 1] = out[i + 2] = 90
            xoff += gap

    EncodeRgb(opath, out, w, h)
    print(f'wrote {opath}  ({w} x {h}, {len(panels)} panels)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
