# Lunar layout editor

A standalone tool for laying out a base by hand from a sprite sheet. The
palette on the left **is** the sheet: hover a piece, click to grab it onto
the cursor, move to the map on the right, click to drop. Nothing about the
sheet is hard-coded — the pieces and their geometry are read off the image.

```
pip install pygame pillow numpy
python3 lunar_layout_editor.py /path/to/your_sheet.png
```

Drop the sheet next to the script as `lunar_sheet.png` and you can run it
with no arguments.

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

With nothing selected, the scale buttons set the size that *new* pieces are
dropped at; with a piece selected they resize that piece.

## What it reads off the sheet

**Where the pieces are.** The sheet is sliced at its background gutters
(an X-Y cut), then components inside each cell are merged across small
gaps. The merge is what keeps a road drawn with a *dashed* centre line one
pickable object rather than a row of loose dashes.

This is the fix for "the small road in the lower left cannot be selected":
the previous version carried a typed table of sprite rectangles and that
one missed the art. Nothing is typed here.

**How far each curve turns.** A filled band is a bad thing to fit a circle
to — over a short arc the band's own thickness swamps the few pixels of
sagitta and an algebraic fit collapses onto the blob's centroid. So the
curve's *centreline* is recovered first (bin the ink along the piece's long
axis, take the mean of each bin, throw away the end bins where the cut
corner biases them), a parabola gives a first circle, and a pattern search
then moves the centre to wherever the ink looks most annular — the same
mean radius at every angle.

A curve's rotation step is its own measured span, so N rotations chain N
copies exactly end to end. A span landing near an exact divisor of 360 is
taken to be that divisor, so a ring closes instead of drifting.

**Where pieces join.** Arcs get a port at each end, straights get one at
each end of the principal axis, and domes get eight around the rim so roads
can run in from any heading. Dropping a piece within ~48 px of a free port
rotates it (in whole steps) so the two headings oppose, then slides the
ports together. Ports that already have something joined to them are
skipped, so a growing chain extends from its open end.

## Checking it against your sheet

```
python3 lunar_layout_editor.py your_sheet.png --atlas atlas.png
```

writes the sheet with every detected box drawn on it, labelled with what it
was taken to be and — for curves — the measured span. If pieces come out
merged or split, `--gutter N` sets how much background has to separate two
pieces (default 22 px). Raise it if separate pieces are being merged; lower
it if one piece is coming apart.

If a curve's span is measured wrongly, that is visible right there in the
atlas, and the number to argue with is printed on launch.

## Proving it, without the real sheet

`make_sample_sheet.py` draws a stand-in sheet in the same layout — two
large domes, two rows of unit domes, a connector, a long road, a short road
in the lower left, five curves — and prints the true arc spans it drew.
`--demo` then drives the editor headlessly and measures them back:

```
python3 make_sample_sheet.py sample_sheet.png
python3 lunar_layout_editor.py sample_sheet.png --demo demo.png
```

Measured against the spans the generator actually drew:

| drawn | measured | used as the rotation step |
|------:|---------:|--------------------------:|
| 15.0 | 15.18 | 15 |
| 30.0 | 31.27 | 30 |
| 22.5 | 23.51 | 24 |
| 50.0 | 50.49 | 51.43 (= 360/7) |
| 60.0 | 59.30 | 60 |

The last two rows are worth reading twice. 50° is not a divisor of 360, so
no whole number of copies can ever close a ring; the step is pulled to the
nearest one that can. And 22.5° vs 24° is the resolution limit — those two
are 1.5° apart and the measurement is good to about 1°, so a curve that
shallow can land on either. Both still chain and close.

The demo also picks the small road out of the palette, snaps four copies
onto a dome's rim, and chains six copies of the 60° curve into a ring whose
two loose ends meet within 0.0 px.
