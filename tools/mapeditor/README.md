# Lunar layout editor

A standalone tool for laying out a base by hand from the sprite sheet. The
palette on the left **is** the sheet (`lunar_sheet.png`, shipped here):
hover a piece, click to grab it onto the cursor, move to the map on the
right, click to drop. Nothing about the sheet is hard-coded — the pieces
and their geometry are read off the image.

```
pip install pygame pillow numpy
python3 lunar_layout_editor.py            # uses lunar_sheet.png beside it
python3 lunar_layout_editor.py other.png  # or any other sheet
```

## Controls

| | |
|---|---|
| click palette | pick a piece up onto the cursor |
| click map | drop it |
| Q / E, or the wheel | rotate by the piece's own step |
| `[` / `]`, or the **scale 0.5x / 2x** buttons | halve or double the size |
| drag a placed piece | move it (it re-snaps on release) |
| G, or **snap** | snapping on/off |
| P, or **ports** | show the connection points |
| Del | remove the selected piece |
| Esc / right-click | drop what you are carrying, deselect |
| **save layout** | writes `layout.json` |

With nothing selected the scale buttons set the size *new* pieces are
dropped at; with a piece selected they resize that piece.

## What it reads off the sheet

**Where the background ends.** This sheet is drawn dark on dark: the
inside of a road sits about six levels above the background and no more.
A threshold picked by eye either splits every road into its two bright
rails or swallows the gutters between pieces, so the background level is
measured — the most common brightness in the image — and the ink threshold
put just above it. Override with `--threshold N` if a different sheet
needs it.

**Where the pieces are.** The sheet is sliced at its background gutters
(an X-Y cut), then components inside each cell are merged across small
gaps. On this sheet that finds all 18 pieces with tight boxes: 10 domes,
3 straight roads, 5 curves.

This is the fix for "the small road in the lower left cannot be selected":
the previous version carried a typed table of sprite rectangles and that
one missed the art. Nothing is typed here.

**How far each curve turns.** A filled band is a bad thing to fit a circle
to — over a short arc the band's own thickness swamps the few pixels of
sagitta, and an algebraic fit collapses onto the blob's centroid (an early
attempt read a 15° curve as 106°). So the curve's *centreline* is
recovered first: bin the ink along the piece's long axis, take the mean of
each bin, and throw away the end bins, where the cut corner drags the mean
several pixels off — more than the whole signal on a shallow curve.

That gives a parabola, and from it a first circle. Then the centre is
swept along the normal — the centre has to lie on it, so the search is
one-dimensional — keeping whichever distance makes the ink look most like
part of a ring: the same mean radius at every angle. Both starting points
are refined by pattern search and the more annular one wins, because
neither is reliable alone: the parabola's radius is good while the curve
is shallow and useless once it turns far enough to bias the binning, and
the sweep is the other way round.

A curve's rotation step is its own measured span, so N rotations chain N
copies end to end. A span landing near an exact divisor of 360 is taken to
be that divisor, so a ring closes instead of drifting.

**Where pieces join.** The sockets are found, not assumed. On a dome,
sweep the angles and take the furthest ink in each: anything reaching
past the rim by six percent is a lug. The big domes have eight, each unit
dome has exactly one — so a road snapped to a unit dome turns it until
that socket faces the road. (The rim circle is fitted rather than taken
from the box centre: a single lug on one side drags the centroid toward
it, and then the opposite side of the rim reads as another lug.)

The two sides of a joint are deliberately not symmetric, because the art
is drawn to overlap:

* a dome's port sits on its **rim**, not on the outer face of its lug;
* a road's port sits where its **rails end** — inboard of the socket
  bracket, which stands proud of the deck and is found by walking in
  from the end of the piece.

Put those together and a road brought up to a dome lands its rail ends on
the rim, with its own socket lying over the dome's lug. Butting the two
outer faces instead leaves the sockets nose to nose and the road stopping
short of the dome.

Dropping a piece within ~48 px of a free port rotates it so the two
headings oppose, then slides the ports together. Curves rotate in whole
steps of their own span, so a chain of them stays true; everything else
takes whatever angle the joint needs. Ports that already have something
joined to them are skipped, so a growing chain extends from its open end.

## What it measures on this sheet

```
python3 lunar_layout_editor.py lunar_sheet.png --demo demo.png
```

drives the editor headlessly — palette click, map click, the same path a
player takes — and chains each curve into a ring:

| sprite | radius | measured | rotation step | ring |
|--------|-------:|---------:|--------------:|------|
| 12 | 489 px | 43.14° | 45° | ×8, closes to 0.0 px |
| 13 | 204 px | 49.69° | 51.43° (360/7) | ×7, closes to 0.0 px |
| 15 | 327 px | 41.32° | 40° | ×9, closes to 0.0 px |
| 16 | 233 px | 52.70° | 51.43° (360/7) | ×7, closes to 0.0 px |
| 17 | 157 px | 41.24° | 40° | ×9, closes to 0.0 px |

It also picks the small road out of the palette and builds a base — big
dome, a road out of every second rim port, a unit dome snapped on each far
end.

**How accurate is that?** About ±1.5°, checked against known angles:
`make_sample_sheet.py` draws a sheet in the same layout with spans it
chooses, and the editor measures them back — 15.0 → 15.18, 30.0 → 31.27,
50.0 → 50.49, 60.0 → 59.30, and the hardest case 22.5 → 25.03.

That resolves the coarse divisors of 360 (45 / 40 / 36 / 30 are four to
six degrees apart) but not the fine ones: below about 25°, 24 / 22.5 / 20
/ 18 sit closer together than the measurement error. Note also that a span
like 50° is not a divisor of 360 at all, so no whole number of copies can
ever close a ring with it; the step is pulled to the nearest one that can.

If you know a piece's true angle and disagree with the measurement, write
it down — a file named after the sheet, `lunar_sheet.spans.json`, holding
`{"12": 45, "16": 45}`, overrides those sprites by index.

## Does the connector fit the roads?

```
python3 lunar_layout_editor.py lunar_sheet.png --fit fit.png
```

builds three chains — unit dome / connector / unit dome, big dome /
connector / unit dome, and unit dome / plain road / unit dome — and
writes a close-up of the first joint alongside. Measured off the sheet:

| | |
|---|---|
| connector deck | 76 px |
| long road deck | 78 px |
| short road deck | 80 px |
| big dome socket | 56 px |
| unit dome socket | 42 px |

So yes, they fit: the three straights agree on deck width to within 5%,
which is a step of a pixel or two at the joint, and they butt cleanly.
The domes' sockets are narrower than the deck, which is what the socket
bracket on the connector is for — it covers the dome's lug, and the rails
carry on from the rim.

## Checking it against a different sheet

```
python3 lunar_layout_editor.py your_sheet.png --atlas atlas.png
```

writes the sheet with every detected box drawn on it, labelled with what
it was taken to be and, for curves, the measured span. If pieces come out
merged or split, `--gutter N` sets how much background has to separate two
pieces (default 22 px): raise it if separate pieces are being merged,
lower it if one piece is coming apart.
