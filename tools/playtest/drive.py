#!/usr/bin/env python3
"""drive.py -- play colony_playtest with a REAL pointer, headlessly.

preview.sh and --shot render frames with no pointer at all, so anything that
reacts to hover, a click sequence or a cursor shape could never be looked at
before it shipped. This runs the playtest on its own virtual X display, moves
and clicks a real X pointer with xdotool, and grabs the screen between steps.

    tools/playtest/drive.py out_dir  "move 620 170" "click" "wait 0.5" \\
                                    "move 640 260" "shot stretch" ...

Steps:
    move X Y        pointer to window coordinates (1280x720 game space)
    click           left press + release
    down / up       left press, left release (for drags)
    wait S          seconds
    shot NAME       write out_dir/NAME.png of the whole window
    key K           an xdotool key name (Escape, t, r ...)

Needs Xvfb, xdotool and python-xlib + Pillow. Software GL, as preview.sh.
"""
import os, subprocess, sys, time

from PIL import Image
from Xlib import display as xdisplay, X

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
BIN = os.path.join(ROOT, "build", "src", "colony_playtest")
DISP = ":97"
HOLD = 1.5          # > two software-GL frames, see "click"


def grab(path):
    """The screen, WITH the pointer. An X screen grab leaves the cursor out,
    and the cursor's shape (hidden, arrow, hand) is part of what is being
    tested -- so it is fetched from XFixes and composited where X says it
    is. A hidden cursor comes back as a fully transparent image."""
    d = xdisplay.Display(DISP)
    r = d.screen().root
    g = r.get_geometry()
    raw = r.get_image(0, 0, g.width, g.height, X.ZPixmap, 0xFFFFFFFF)
    im = Image.frombytes("RGB", (g.width, g.height), raw.data, "raw", "BGRX")
    try:
        d.xfixes_query_version()
        c = d.xfixes_get_cursor_image(r)
        px = bytearray()
        for argb in c.cursor_image:              # premultiplied ARGB words
            px += bytes(((argb >> 16) & 255, (argb >> 8) & 255, argb & 255, (argb >> 24) & 255))
        cur = Image.frombytes("RGBA", (c.width, c.height), bytes(px))
        im.paste(cur, (c.x - c.xhot, c.y - c.yhot), cur)
    except Exception as e:                       # no XFixes: the grab still stands
        print("no cursor image:", e)
    im.save(path)


def xdo(*args):
    subprocess.run(["xdotool", *args], env={**os.environ, "DISPLAY": DISP}, check=True)


def main():
    out = sys.argv[1]
    steps = sys.argv[2:]
    os.makedirs(out, exist_ok=True)

    xvfb = subprocess.Popen(["Xvfb", DISP, "-screen", "0", "1280x720x24", "-nolisten", "tcp"],
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(1.0)
    env = {**os.environ, "DISPLAY": DISP, "LIBGL_ALWAYS_SOFTWARE": "1", "GALLIUM_DRIVER": "llvmpipe"}
    log = open(os.path.join(out, "game.log"), "w")
    game = subprocess.Popen([BIN], cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT)
    try:
        time.sleep(6.0)                     # window, fonts, ground
        for step in steps:
            a = step.split()
            if a[0] == "move":
                # Two events, not one. When the game changes the cursor's
                # shape (hidden drill -> arrow -> hand), the next single
                # motion event can be dropped, and xdotool's jump IS a single
                # event. A real mouse sends dozens; this sends two.
                xdo("mousemove", str(int(a[1]) - 2), a[2]); time.sleep(HOLD * 0.5)
                xdo("mousemove", a[1], a[2]); time.sleep(HOLD)
            elif a[0] == "click":
                # Software GL draws this screen at ~0.6 s a frame, and raylib
                # only sees a button that is still down when a frame polls. A
                # 1 ms xdotool click lands between frames and is lost; hold it.
                xdo("mousedown", "1"); time.sleep(HOLD); xdo("mouseup", "1"); time.sleep(HOLD)
            elif a[0] == "down":  xdo("mousedown", "1")
            elif a[0] == "up":    xdo("mouseup", "1")
            elif a[0] == "key":   xdo("key", a[1])
            elif a[0] == "wait":  time.sleep(float(a[1]))
            elif a[0] == "shot":
                time.sleep(HOLD)            # let a frame or two land
                grab(os.path.join(out, a[1] + ".png"))
                print("shot", a[1])
            time.sleep(0.15)
    finally:
        game.terminate(); xvfb.terminate()


if __name__ == "__main__":
    main()
