#!/usr/bin/env python3
"""gapscan -- enumerate every Canvas 2D feature site in a JS graphics source.

Step 3 of the JS graphics port protocol (docs/guides/js-graphics-port.md).
The inventory is the step that makes a port faithful: the failure is never
that the glow could not be written, it is that nobody registered a glow was
there. This turns "grep for the gaps" into a deterministic listing, so two
sessions scanning the same source get the same rows.

    python3 tools/jsport/gapscan.py js/dashboard.html --lines 21-712
    python3 tools/jsport/gapscan.py js/new_panel.js --md > inventory-skeleton.md

What it does, and does not, decide:

  * Counts CODE LINES that use a feature, with teardown excluded
    (shadowBlur = 0, setLineDash([]), globalAlpha = 1, ...). Resets are not
    features; counting them hides what was skipped.
  * Finds glow HELPERS by itself: any `function NAME(` whose line assigns
    shadowBlur is a glow wrapper, and every call to NAME is a glow site.
    Grepping shadowBlur alone missed the entire tool rack once.
  * Maps each feature to its spec section and shim function, and marks it
    COVERED, APPROX (the shim has a documented approximation) or MISSING
    (no shim support: extend c2d first, with a c2dtest assertion, before
    the port may use it).
  * Flags determinism hazards (Math.random, clocks) that the visual-diff
    harness has to pin.

It does NOT replace reading the source. A feature reached through an
indirection it does not know (a helper spread over several lines, a style
string built at runtime) will be missed -- the counts are a floor, and the
inventory must account for every row it prints.

Exit status: 0 clean, 2 if any MISSING feature is present (so a script
cannot quietly proceed past it), 1 on usage error.
"""
import argparse
import re
import sys

# (key, label, regex, reset-regex or None, spec section, shim, status, note)
# status: COVERED | APPROX | MISSING | FLATTEN | INFO
FEATURES = [
    ("lingrad", "createLinearGradient", r"createLinearGradient\s*\(", None,
     "2.2", "c2d_gradient_linear / _linear_x + c2d_fill_poly_gradient", "COVERED",
     "axis-aligned only: a DIAGONAL gradient (x0!=x1 AND y0!=y1) is MISSING"),
    ("radgrad", "createRadialGradient", r"createRadialGradient\s*\(", None,
     "2.10", "c2d_fill_radial", "COVERED",
     "concentric only: a two-point (offset centres) radial is MISSING"),
    ("congrad", "createConicGradient", r"createConicGradient\s*\(", None,
     "-", "(none)", "MISSING", "no conic gradient in the shim"),
    ("glowraw", "shadowBlur (raw, non-zero)", r"shadowBlur\s*=", r"shadowBlur\s*=\s*0(\.0*)?\s*[;,)}]",
     "2.3", "c2d_glow_stroke/_polygon/_fill/_line, or c2d_shadow_begin/_end", "COVERED",
     "per-shape glow for an isolated shape; layer blur where shapes share one glow or the path is dashed"),
    ("glowhelper", "glow helper calls", None, None,
     "2.3", "same as shadowBlur", "COVERED",
     "helpers found automatically; see header"),
    ("shadowoff", "shadowOffsetX/Y", r"shadowOffset[XY]\s*=", r"shadowOffset[XY]\s*=\s*0(\.0*)?\s*[;,)}]",
     "-", "(none)", "MISSING", "the shim's shadows are centred; an offset drop shadow needs a new entry point"),
    ("clip", "clip()", r"\.clip\s*\(", None,
     "2.4", "analytic clamp > c2d_clip_segment_* > c2d_clip_poly_begin/_end", "COVERED",
     "check for a preceding beginPath that silently discards an even-odd outer rect"),
    ("dash", "setLineDash (non-reset)", r"setLineDash\s*\(", r"setLineDash\s*\(\s*\[\s*\]\s*\)",
     "2.7", "c2d_dashed_polyline / c2d_glow_dashed_phase", "COVERED",
     "keep the JS scale factor on dash lengths"),
    ("dashoff", "lineDashOffset", r"lineDashOffset\s*=", r"lineDashOffset\s*=\s*0(\.0*)?\s*[;,)}]",
     "2.7", "c2d_dashed_polyline_phase", "COVERED", "Canvas SUBTRACTS the offset"),
    ("alpha", "globalAlpha (non-reset)", r"globalAlpha\s*[*]?=", r"globalAlpha\s*=\s*1(\.0*)?\s*[;,)}]",
     "2.5", "c2d_push_alpha / c2d_pop_alpha (or c2d_save/_restore)", "COVERED", ""),
    ("gco", "globalCompositeOperation", r"globalCompositeOperation\s*=",
     r"globalCompositeOperation\s*=\s*['\"]source-over['\"]",
     "-", "(none)", "MISSING", "blend modes (lighter, destination-out, ...) are not in the shim"),
    ("filter", "ctx.filter", r"\bctx\.filter\s*=", r"filter\s*=\s*['\"]none['\"]",
     "-", "(none)", "MISSING", "CSS filters (blur(), brightness()) are not in the shim"),
    ("ellipse", "ellipse()", r"\.ellipse\s*\(", None,
     "2.11", "c2d_ellipse_pts -> c2d_fill_poly / c2d_polyline", "COVERED",
     "honour partial arcs (a0..a1) and rotation"),
    ("arc", "arc()", r"\.arc\s*\(", None,
     "3", "c2d_disc / c2d_ring (full), c2d_ellipse_pts rx=ry (partial)", "COVERED",
     "screen-space circles only; a projected circle is a polyline"),
    ("arcto", "arcTo()", r"\.arcTo\s*\(", None,
     "-", "sample at the call site", "FLATTEN", "no shim entry; flatten to points"),
    ("quad", "quadraticCurveTo", r"quadraticCurveTo\s*\(", None,
     "-", "c2d_rpoly_pts (rounded corners) or sample at the call site", "FLATTEN", ""),
    ("bezier", "bezierCurveTo", r"bezierCurveTo\s*\(", None,
     "-", "sample at the call site (see dash_chrome.c)", "FLATTEN", ""),
    ("roundrect", "native roundRect()", r"\.roundRect\s*\(", None,
     "-", "c2d_round_rect / c2d_rpoly_pts", "COVERED", ""),
    ("path2d", "Path2D", r"\bPath2D\b", None,
     "-", "(none)", "MISSING", "port the path by hand into a point array"),
    ("measure", "measureText", r"measureText\s*\(", None,
     "2.9", "c2d_measure", "COVERED", "layout depending on width must measure, not hardcode"),
    ("filltext", "fillText", r"fillText\s*\(", None,
     "2.9", "c2d_text / c2d_text_tracked / c2d_text_gradient", "COVERED", "y is the BASELINE"),
    ("stroketext", "strokeText", r"strokeText\s*\(", None,
     "-", "(none)", "MISSING", "outlined glyphs are not in the shim"),
    ("align", "textAlign", r"textAlign\s*=", None,
     "2.9", "C2DAlign", "COVERED", "'start'/'end' map to left/right (ltr)"),
    ("baseline", "textBaseline", r"textBaseline\s*=", None,
     "2.9", "C2DBaseline", "COVERED",
     "alphabetic/middle/top only: bottom/hanging/ideographic are MISSING"),
    ("font", "font =", r"\.font\s*=", None,
     "2.9", "c2d_fonts_load (JetBrains Mono 500/600/700)", "COVERED",
     "any other family or weight is MISSING until loaded; check each"),
    ("drawimage", "drawImage", r"drawImage\s*\(", None,
     "2.6", "c2d_group_composite / c2d_cache_blit", "COVERED",
     "if it draws an actual image FILE rather than an offscreen canvas, that is new"),
    ("pattern", "createPattern", r"createPattern\s*\(", None,
     "2.12", "c2d_grain_init / c2d_grain_draw", "COVERED", "only the grain tile is supported"),
    ("pixels", "getImageData/putImageData", r"(get|put)ImageData\s*\(", None,
     "2.12", "c2d_grain_* if it builds a noise tile", "APPROX",
     "anything else reading/writing pixels is MISSING"),
    ("offscreen", "offscreen canvas", r"createElement\s*\(\s*['\"]canvas['\"]|OffscreenCanvas", None,
     "2.6", "c2d_group_* (composite once) / c2d_cache_* (static chrome)", "COVERED",
     "find out WHY it is offscreen: ghost group, repaint cache, or pattern tile"),
    ("transform", "translate/rotate/scale", r"\.(translate|rotate|scale)\s*\(", None,
     "-", "c2d_translate / c2d_rotate / c2d_scale", "COVERED", ""),
    ("settransform", "setTransform/resetTransform", r"\.(setTransform|resetTransform|transform)\s*\(", None,
     "1", "usually a DPR reset: drop it (design space); else c2d_save/_restore", "INFO", ""),
    ("saverestore", "save/restore", r"\.(save|restore)\s*\(\s*\)", None,
     "-", "c2d_save / c2d_restore (alpha + transform + clip)", "COVERED", ""),
    ("joins", "lineJoin/lineCap not round", r"line(Join|Cap)\s*=", r"line(Join|Cap)\s*=\s*['\"]round['\"]",
     "2.8", "c2d_polyline (round only)", "APPROX",
     "butt/square/miter/bevel draw round; usually sub-gate at 1-2px, check at wide strokes"),
    ("smoothing", "imageSmoothingEnabled", r"imageSmoothing", None,
     "-", "(none)", "MISSING", "texture filter choice; decide explicitly"),
    ("hittest", "isPointInPath/Stroke", r"isPointIn(Path|Stroke)\s*\(", None,
     "1", "point-in-polygon in DESIGN space (c2d_to_design first)", "APPROX", ""),
]

# Things the harness has to pin, or the diff measures noise.
HAZARDS = [
    (r"Math\.random\s*\(", "Math.random",
     "unseeded: seed it the same on both sides or switch the feature off for the diff (the grain precedent)"),
    (r"performance\.now|Date\.now|requestAnimationFrame", "clock / animation frame",
     "pin time to a fixed value in the reference page and the port driver"),
    (r"devicePixelRatio", "devicePixelRatio", "force dpr = 1 in the reference; the port renders at design size"),
    (r"addEventListener|onpointer|onmouse|ontouch", "input handlers",
     "port hit tests in design space; SurveyDash_* shows the screen->design conversion"),
    (r"fonts\.googleapis|@import\s+url|<link[^>]+font", "web font",
     "the proxy blocks it and Canvas falls back SILENTLY: the reference page must use local @font-face"),
    (r"<img|new Image\s*\(|\.png|\.jpg|\.svg", "image asset",
     "the modules ported so far were pure draw calls; an image asset needs loading on both sides"),
]


def parse_ranges(spec):
    out = []
    for part in spec.split(","):
        a, _, b = part.partition("-")
        out.append((int(a), int(b or a)))
    return out


def in_ranges(n, ranges):
    return not ranges or any(a <= n <= b for a, b in ranges)


def split_args(s):
    """Top-level comma split of the text inside the first (...)."""
    depth, cur, out = 0, "", []
    for ch in s:
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            if depth == 0:
                break
            depth -= 1
        if ch == "," and depth == 0:
            out.append(cur.strip()); cur = ""
        else:
            cur += ch
    if cur.strip():
        out.append(cur.strip())
    return out


def refine(key, line, m_end):
    """Per-site escalations: a covered feature used in a form the shim lacks."""
    args = split_args(line[m_end:])
    if key == "lingrad" and len(args) == 4:
        if args[0] != args[2] and args[1] != args[3]:
            return "MISSING", "diagonal gradient"
    if key == "radgrad" and len(args) == 6:
        if args[0] != args[3] or args[1] != args[4]:
            return "MISSING", "two-point radial (centres differ)"
    if key == "baseline":
        m = re.search(r"textBaseline\s*=\s*['\"](\w+)", line)
        if m and m.group(1) not in ("alphabetic", "middle", "top"):
            return "MISSING", "baseline '%s'" % m.group(1)
    if key == "font":
        weights = re.findall(r"\b([1-9]00)\b", line)
        bad = [w for w in weights if w not in ("500", "600", "700")]
        if bad:
            return "MISSING", "weight %s not loaded" % ",".join(bad)
        fam = re.search(r"px\s+['\"]?([A-Za-z][\w ]+)", line)
        if fam and "JetBrains" not in fam.group(1) and "$" not in line[: fam.start()] \
                and fam.group(1).strip() not in ("monospace",):
            return "APPROX", "family '%s' -- check what the port loads" % fam.group(1).strip()
    if key == "gco":
        m = re.search(r"globalCompositeOperation\s*=\s*['\"]([\w-]+)", line)
        if m:
            return "MISSING", "mode '%s'" % m.group(1)
    return None, ""


def scan(path, ranges):
    lines = open(path, encoding="utf-8", errors="replace").read().split("\n")

    # Pass 1: glow helpers, defined anywhere in the file (a helper is often
    # defined outside the range that calls it).
    # A helper that only ever assigns shadowBlur = 0 (glowOff) is teardown.
    helpers = set()
    for ln in lines:
        blurs = re.findall(r"shadowBlur\s*=\s*([^;,)}]+)", ln)
        if not blurs or all(re.fullmatch(r"0(\.0*)?", b.strip()) for b in blurs):
            continue
        names = [m.group(1) for m in re.finditer(r"function\s+(\w+)\s*\(", ln)]
        names += re.findall(r"(?:const|let|var)\s+(\w+)\s*=\s*\([^)]*\)\s*=>", ln)
        helpers.update(names)
    # (?<![.\w]) so a helper named `stroke` does not match ctx.stroke()
    helper_re = re.compile(r"(?<![.\w])(%s)\s*\(" % "|".join(sorted(helpers))) if helpers else None
    def_re = re.compile(r"(function\s+|(?:const|let|var)\s+)(%s)\b" % "|".join(sorted(helpers))) if helpers else None

    sites = {f[0]: [] for f in FEATURES}
    hazards = []
    for i, ln in enumerate(lines, 1):
        if not in_ranges(i, ranges):
            continue
        code = ln.split("//", 1)[0] if "://" not in ln else ln
        is_helper_def = bool(def_re and def_re.search(code))
        for key, label, rx, reset, sec, shim, status, note in FEATURES:
            if key == "glowhelper":
                if helper_re:
                    for m in helper_re.finditer(code):
                        if is_helper_def and def_re.search(code[max(0, m.start() - 12):m.end()]):
                            continue
                        a = split_args(code[m.end():])
                        sites[key].append((i, m.group(1) + "(" + ", ".join(a) + ")", status, ""))
                continue
            # A helper's own shadowBlur assignment is its mechanism, not a
            # site: the sites are its calls, each of which chooses a blur.
            if key == "glowraw" and is_helper_def:
                continue
            for m in re.finditer(rx, code):
                tail = code[m.start():]
                if reset and re.match(reset, tail):
                    continue
                st, why = refine(key, code, m.end())
                sites[key].append((i, tail[:70].strip(), st or status, why))
        for rx, label, note in HAZARDS:
            if re.search(rx, code):
                hazards.append((i, label, note))
    return lines, helpers, sites, hazards


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("file")
    ap.add_argument("--lines", default="", help="e.g. 21-712 or 21-712,965-1290 (1-based, inclusive)")
    ap.add_argument("--md", action="store_true", help="also emit the per-site inventory skeleton")
    a = ap.parse_args()
    ranges = parse_ranges(a.lines) if a.lines else []
    try:
        lines, helpers, sites, hazards = scan(a.file, ranges)
    except OSError as e:
        print(e, file=sys.stderr); return 1

    where = a.file + (" lines " + a.lines if a.lines else "")
    print("## Gap scan: `%s`\n" % where)
    if helpers:
        print("Glow helpers found (their calls are glow sites): %s\n"
              % ", ".join("`%s`" % h for h in sorted(helpers)))

    print("| Feature | Sites | Spec § | Shim | Status | Note |")
    print("|---|---:|---|---|---|---|")
    for key, label, rx, reset, sec, shim, status, note in FEATURES:
        s = sites[key]
        if not s:
            continue
        worst = "MISSING" if any(st == "MISSING" for _, _, st, _ in s) else status
        if key == "glowhelper":
            per = {}
            for _, txt, _, _ in s:
                per[txt.split("(")[0]] = per.get(txt.split("(")[0], 0) + 1
            label = "glow helper calls (%s)" % ", ".join("%s %d" % kv for kv in sorted(per.items()))
        print("| %s | %d | %s | %s | %s | %s |" % (label, len(s), sec, shim, worst, note))

    fixed = [(k, s) for k, s in sites.items() if s]
    missing_sites = [(key, ln, txt, why)
                     for key, ss in fixed for (ln, txt, st, why) in ss if st == "MISSING"]
    if missing_sites:
        print("\n### MISSING from the shim -- extend c2d (with a c2dtest) before porting these\n")
        for key, ln, txt, why in sorted(missing_sites, key=lambda r: r[1]):
            print("- line %d: `%s`%s" % (ln, txt, (" -- " + why) if why else ""))

    if hazards:
        print("\n### Determinism / harness hazards\n")
        seen = set()
        for ln, label, note in hazards:
            if label in seen:
                continue
            seen.add(label)
            lns = [str(h[0]) for h in hazards if h[1] == label]
            print("- **%s** (lines %s): %s" % (label, ", ".join(lns[:8]) + (" ..." if len(lns) > 8 else ""), note))

    if a.md:
        print("\n## Inventory skeleton -- fill every Notes cell, delete nothing\n")
        print("| JS line | Canvas call | Spec § | Shim function | Status | Notes |")
        print("|---|---|---|---|---|---|")
        rows = []
        for key, label, rx, reset, sec, shim, status, note in FEATURES:
            for ln, txt, st, why in sites[key]:
                rows.append((ln, txt, sec, shim, st, why))
        for ln, txt, sec, shim, st, why in sorted(rows):
            print("| %d | `%s` | %s | %s | %s | %s |" % (ln, txt.replace("|", "\\|"), sec, shim, st, why))

    total = sum(len(s) for s in sites.values())
    print("\n%d feature sites; %d MISSING. Counts are a floor, not a ceiling: "
          "read the source for indirections this scan cannot see." % (total, len(missing_sites)))
    return 2 if missing_sites else 0


if __name__ == "__main__":
    sys.exit(main())
