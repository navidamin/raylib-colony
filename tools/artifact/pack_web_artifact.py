#!/usr/bin/env python3
"""Pack one Emscripten web build as a claude.ai Artifact.

An Artifact serves only web file types (.html .js .wasm, images, ...),
at most 15 MB per binary file and 64 MB per version, and it wraps the
page in its own <!doctype>/<head>/<body>. Emscripten's output breaks all
three: a full document, and a .data package of every preloaded asset
(53 MB, the LOLA DEM alone 33 MB) that no Artifact will serve.

So this writes, into OUT_DIR:

  index.html        the shell's <style> and <body>, with a loader in
                    place of Emscripten's <script src=TARGET.js>
  TARGET.js         unchanged
  TARGET.wasm       unchanged
  TARGET.data.NN.wasm
                    the .data package, cut into 14 MB pieces. The name
                    is only so the Artifact serves them: the bytes are
                    the package's, and the loader never compiles them.

The loader fetches the pieces, joins them, hands the buffer to the
package loader through Module.getPreloadedPackage (Emscripten's own
hook for this), and only then starts TARGET.js. Nothing in the game
changes; a GitHub Pages deploy of the same build is unaffected.

Usage: pack_web_artifact.py BUILD_DIR TARGET OUT_DIR [--title TITLE]
  e.g. pack_web_artifact.py build-web/src colony_viewtest artifact/viewtest
"""

import argparse
import html
import os
import re
import shutil
import sys

PIECE_BYTES = 14*1024*1024     # under the Artifact's 15 MB per binary file
VERSION_LIMIT = 64*1024*1024   # an Artifact version, all files together


def Fail(msg):
    sys.stderr.write("pack_web_artifact: " + msg + "\n")
    sys.exit(1)


LOADER = """<div id="artifactLoad" style="position:fixed;left:50%;top:50%;transform:translate(-50%,-50%);
     font:14px/1.5 ui-monospace,monospace;color:#c9ced8;text-align:center;z-index:10;">
  <div id="artifactLoadText">Loading assets…</div>
  <div style="width:220px;height:4px;background:#2a2f3a;margin-top:8px;border-radius:2px;overflow:hidden;">
    <div id="artifactLoadBar" style="width:0;height:100%;background:#8fb3ff;"></div>
  </div>
</div>
<script>
// Artifact loader (tools/artifact/pack_web_artifact.py): join the .data
// pieces, give them to Emscripten's package loader, then start the game.
(function()
{
    var PIECES = __PIECES__;
    var TOTAL = __TOTAL__;
    var SCRIPT = "__SCRIPT__";
    var text = document.getElementById('artifactLoadText');
    var bar = document.getElementById('artifactLoadBar');
    var done = 0;
    var loaded = new Array(PIECES.length);

    function Show(msg) { text.textContent = msg; }

    function FetchPiece(i)
    {
        return fetch(PIECES[i]).then(function(r)
        {
            if (!r.ok) throw new Error(PIECES[i] + ': HTTP ' + r.status);
            return r.arrayBuffer();
        }).then(function(buf)
        {
            loaded[i] = new Uint8Array(buf);
            done += buf.byteLength;
            bar.style.width = Math.round(100*done/TOTAL) + '%';
            Show('Loading assets… ' + (done/1048576).toFixed(0) + ' / ' + (TOTAL/1048576).toFixed(0) + ' MB');
        });
    }

    Promise.all(PIECES.map(function(_, i) { return FetchPiece(i); })).then(function()
    {
        var all = new Uint8Array(TOTAL);
        var at = 0;
        for (var i = 0; i < loaded.length; i++) { all.set(loaded[i], at); at += loaded[i].length; }
        loaded = null;
        if (at !== TOTAL) throw new Error('assets: got ' + at + ' bytes, expected ' + TOTAL);
        Module.getPreloadedPackage = function(name, size)
        {
            return (size === TOTAL) ? all.buffer : null;
        };
        Show('Starting…');
        var s = document.createElement('script');
        s.src = SCRIPT;
        s.onerror = function() { Show('Could not load ' + SCRIPT); };
        document.body.appendChild(s);
        var hide = function()
        {
            var el = document.getElementById('artifactLoad');
            if (el) el.remove();
        };
        Module.onRuntimeInitialized = hide;
    }).catch(function(e) { Show('Load failed: ' + (e && e.message || e)); });
})();
</script>
"""


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("build_dir")
    ap.add_argument("target")
    ap.add_argument("out_dir")
    ap.add_argument("--title", default=None)
    args = ap.parse_args()

    src = lambda ext: os.path.join(args.build_dir, args.target + ext)
    for ext in (".html", ".js", ".wasm", ".data"):
        if not os.path.isfile(src(ext)):
            Fail("missing " + src(ext) + " -- build the web target first")

    page = open(src(".html"), encoding="utf-8").read()
    script = open(src(".js"), encoding="utf-8").read()
    if "getPreloadedPackage" not in script:
        Fail(args.target + ".js has no getPreloadedPackage hook; this Emscripten "
             "packs .data differently and the loader will not work")

    # The page: the shell's styles and body. The Artifact supplies the
    # doctype, head and body; the viewer blocks every script host but a
    # few CDNs, so the FileSaver <script> from cdn.jsdelivr.net/gh goes
    # (inline head scripts stay; saving a file is blocked there anyway).
    head = re.search(r"<head[^>]*>(.*)</head>", page, re.S)
    head = head.group(1) if head else ""
    styles = "\n".join(re.findall(r"<style>.*?</style>", head, re.S))
    inline = "\n".join(re.findall(r"<script>.*?</script>", head, re.S))
    body = re.search(r"<body[^>]*>(.*)</body>", page, re.S)
    if not body:
        Fail("no <body> in " + src(".html"))
    body = body.group(1)
    tag = re.compile(r"<script[^>]*\bsrc=[\"']?" + re.escape(args.target) + r"\.js[\"']?[^>]*>\s*</script>")
    if not tag.search(body):
        Fail("no <script src=%s.js> in the page body" % args.target)

    if os.path.isdir(args.out_dir):
        shutil.rmtree(args.out_dir)
    os.makedirs(args.out_dir)

    data = open(src(".data"), "rb").read()
    pieces = []
    for i in range(0, len(data), PIECE_BYTES):
        name = "%s.data.%02d.wasm" % (args.target, len(pieces))
        with open(os.path.join(args.out_dir, name), "wb") as f:
            f.write(data[i:i + PIECE_BYTES])
        pieces.append(name)

    loader = (LOADER.replace("__PIECES__", repr(pieces).replace("'", '"'))
                    .replace("__TOTAL__", str(len(data)))
                    .replace("__SCRIPT__", args.target + ".js"))
    body = tag.sub(lambda m: loader, body)

    title = args.title or args.target
    out = "<title>%s</title>\n%s\n%s\n%s" % (html.escape(title), styles, inline, body.strip() + "\n")
    with open(os.path.join(args.out_dir, "index.html"), "w", encoding="utf-8") as f:
        f.write(out)
    shutil.copy(src(".js"), args.out_dir)
    shutil.copy(src(".wasm"), args.out_dir)

    total = 0
    for name in sorted(os.listdir(args.out_dir)):
        size = os.path.getsize(os.path.join(args.out_dir, name))
        total += size
        print("  %-34s %6.1f MB" % (name, size/1048576.0))
        if not name.endswith(".html") and size > 15*1024*1024:
            Fail(name + " is over the Artifact's 15 MB per binary file")
    print("  %-34s %6.1f MB (limit 64)" % ("total", total/1048576.0))
    if total > VERSION_LIMIT:
        Fail("over the Artifact's 64 MB per version")


if __name__ == "__main__":
    main()
