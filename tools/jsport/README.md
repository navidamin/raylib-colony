# tools/jsport

Tooling for the JS graphics port protocol,
[`docs/guides/js-graphics-port.md`](../../docs/guides/js-graphics-port.md).

## gapscan.py

Step 1 of the protocol. Lists every Canvas 2D feature site in a JS source,
maps each one to its `CANVAS2D_PORT_SPEC.md` section and `c2d` function, and
flags what the shim can't do yet.

```bash
python3 tools/jsport/gapscan.py js/dashboard.html --lines 21-712        # summary
python3 tools/jsport/gapscan.py js/new_panel.js --md > skeleton.md      # + inventory rows
```

| Status | Meaning |
|---|---|
| `COVERED` | the shim has the approved implementation |
| `APPROX` | the shim has a documented approximation; check it at this site |
| `FLATTEN` | no shim entry needed; sample the curve into points at the call site |
| `MISSING` | **extend c2d first**, with a c2dtest, before the port uses it |

Exit 0 when clean, 2 when anything is `MISSING`.

When the shim gains a feature, update the row in `FEATURES` so the next scan
reports it `COVERED`.

Checked against the spec's verified §3.5 table on `js/dashboard.html`: the
linear, radial, clip, dash, alpha, ellipse, measureText, drawImage and
pattern rows match per module. For glow it reports 15 `glowOn` calls where
the spec says 16, because the spec's grep also counted the helper's
definition line.
