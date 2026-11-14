# LENSKID v1.0

Film, camera and glitch looks for DaVinci Resolve (OpenFX, macOS universal: Apple Silicon + Intel),
plus a matching pack of 7 colour-only `.cube` LUTs. Original look designs by koobacorp.

## The 7 looks
| # | Look | What it is |
|---|---|---|
| 1 | Boketto Moody Vintage | Teal-grey crushed shadows, muted greens, faded blacks, cool skin with a little warmth, fine grain, soft vignette |
| 2 | Overexposed Dream | Blown, lifted highlights, warm golden glow, deep glowing greens, red-orange halation, soft grain, mist |
| 3 | Dashcam Junk | Low-res blocky compression, over-sharpened edges, purple-magenta cast, fisheye bulge, orange date stamp |
| 4 | Super 8 | Kodachrome-ish warm colour, heavy grain, gate weave, flicker, rounded dark corners, dust and scratches, 18 fps feel, light leaks |
| 5 | Melt Signal | Rows of pixels sliding sideways, RGB channels tearing, smeared colour bleed; optional beat hits from your BPM |
| 6 | Infrared Ghost | Fake infrared: pink-white foliage, black skies, glowing skin, faint double-exposure ghost |
| 7 | Thermal Heart | Heat-camera palette (blue / purple / red / orange / white) from brightness; skin runs hot, grass and sky run cold |

## What the sliders do in each look (50 = the look's normal amount)
| Look | Wobble / Lens | Distortion / Damage |
|---|---|---|
| Boketto | soft vintage-lens edges + colour fringe | faded, lifted blacks |
| Dream | mist / diffusion | overexposure |
| Dashcam | fisheye bulge | low resolution, block compression, over-sharpening |
| Super 8 | gate weave + flicker | dust and scratches |
| Melt | row slides | RGB tearing + colour bleed |
| Infrared | ghost distance | double-exposure strength |
| Thermal | heat shimmer | low-res sensor + heat banding |

Grain, Glow / Halation, Vignette and Real colour blend work the same way in every look.

## LUTs
`LUTs/LENSKID/LENSKID_0N_<Look>.cube`: 33-point 3D LUTs, Rec.709 display in and out, colour only
(the same colour maths as the plugin at default settings, checked against it pixel for pixel).
Grain, glow, wobble and damage need the plugin.

## Building and tests
- `make test` - engine unit tests (every look x every slider, ranges, NaN safety, determinism,
  odd sizes, 8-bit stability, frame hold, LUT format) and an OpenFX host simulation that loads the real
  plugin binary at 8/16/32-bit, RGB/RGBA, odd sizes and shifted origins, render windows, frames-needed,
  identity, and checks the plugin's pixels equal the engine's.
- `make luts`, `make stills` - LUT pack and before/after stills (`stock/` photos, see `stock/CREDITS.txt`).
- `python3 tools/check_luts.py build/LUTs/LENSKID ./build_render_still sheet.jpg` - applies every LUT with
  Pillow's own 3D-LUT code and compares it with the engine's grade on real photos.
- `./build_mac.sh` - universal, ad-hoc signed `LENSKID.ofx.bundle` and the zip (CI: `.github/workflows/mac.yml`).

OpenFX headers in `include/` are from the OpenFX project (BSD-3, see `include/OPENFX-LICENSE.md`).
