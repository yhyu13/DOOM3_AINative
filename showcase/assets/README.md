# Redacted reference capture

`hero-redacted.png` is a **transformed, heavily redacted** frame derived from an official
DOOM 3 promotional screenshot (Steam store app 9050, © id Software / Bethesda Softworks /
ZeniMax). No reuse licence is granted by Steam or the publisher.

The original screenshot is **not** in this repository. It exists only in the operator's temp
directory for a local design preview.

## Transformation

- Crop to 16:9 (the browser viewport).
- Downscale to 48×27 and upscale with nearest-neighbour: deliberate mosaic.
- Ember colour grade, darkening and vignette.
- Full-frame `XX` grid (56×30) over every cell.
- Central `REDACTED` bar and `LICENSING PENDING · NOT A LICENSED RELEASE`.
- Scanlines.

The result preserves the shape of a real game frame while removing readable art. It is not a
licensed release and must be replaced with a properly licensed capture before public
distribution:

```powershell
uv run --python 3.12 showcase/build.py `
  --hero "..\captures\licensed-frame.png" `
  --hero-label "LICENSED CAPTURE" `
  --hero-credit "Capture: your name / used with permission"
```
