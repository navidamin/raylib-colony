# Playtests as claude.ai Artifacts

`pack_web_artifact.py` turns one Emscripten web build (`colony_viewtest`,
`colony_game`, `lunar_map`, ...) into files a claude.ai Artifact will
serve. Each build becomes its own private link, so any branch or feature
can have a playtest up at the same time as the others, without GitHub
Pages and without a public repo.

```bash
source ~/emsdk/emsdk_env.sh            # Emscripten 3.1.64, as CI uses
emcmake cmake -B build-web -DPLATFORM=Web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web --target colony_viewtest
python3 tools/artifact/pack_web_artifact.py build-web/src colony_viewtest out/viewtest \
        --title "Colony Viewtest"
```

Then have Claude Code publish `out/viewtest/index.html` with every other
file in `out/viewtest/` as a supporting file (the Artifact tool's
`files`). Republishing the same page keeps the same link.

## Why it is needed

An Artifact serves only web file types, at most 15 MB per binary file
and 64 MB per version, and supplies its own doctype/head/body. The
game's `.data` package (every preloaded asset, 51 MB, the LOLA DEM alone
33 MB) is none of those. The packer cuts it into 14 MB pieces named
`*.data.NN.wasm` (the name only gets them served; they are never
compiled), and a loader in the page joins them and hands them to
Emscripten through `Module.getPreloadedPackage` before starting the
game. The game's code and the Pages deploy are unchanged.

Checked on 2026-09-24: WebAssembly, WebGL 1 and 2 all run in the Claude
iOS app's Artifact viewer; a piece read back from a published Artifact
matched its sha256; the packed viewtest ran in headless Chromium.

## Limits

- 64 MB per Artifact version, everything included: the packer fails
  past it. `colony_viewtest` is 52.5 MB.
- An Artifact gets no query string, so `?terrain=cpu|gpu` and
  `?debug=1` do nothing; the terrain path picks itself by timing.
- Private by default: share from the page's Share menu.
