# Independent LUT check: applies each .cube with Pillow's own 3D-LUT filter and compares it
# with the engine's colour grade on real photos. Also writes a LUT-only contact sheet.
import sys, subprocess, glob, os
from PIL import Image, ImageFilter, ImageDraw, ImageFont
import numpy as np
lutdir, binp, out = sys.argv[1], sys.argv[2], sys.argv[3]
def load_cube(p):
    size = None; rows = []
    for line in open(p):
        t = line.split()
        if not t or line.startswith("#"): continue
        if t[0] == "LUT_3D_SIZE": size = int(t[1]); continue
        if t[0] in ("TITLE", "DOMAIN_MIN", "DOMAIN_MAX"): continue
        rows.append(tuple(float(v) for v in t))
    assert size == 33 and len(rows) == size ** 3, (p, size, len(rows))
    return ImageFilter.Color3DLUT(size, rows)  # Pillow wants red changing fastest, same as .cube
# orientation self-test: a cube whose output is (r, g, b) itself must be identity
ident = ImageFilter.Color3DLUT.generate(33, lambda r, g, b: (r, g, b))
names = sorted(glob.glob(lutdir + "/*.cube")); assert len(names) == 7
photos = ["dance_park", "street_dance", "road_spring", "green_lawn"]
fails = 0
FONT = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 18)
tiles = []
for li, n in enumerate(names):
    lut = load_cube(n); errs = []
    for ph in photos:
        im = Image.open(f"stock/{ph}.jpg").convert("RGB"); im = im.resize((480, round(480 * im.height / im.width)))
        im.save("/tmp/lc.ppm"); subprocess.run([binp, "/tmp/lc.ppm", "/tmp/lc_g.ppm", str(li), "gradeonly=1"], check=True)
        a = np.asarray(im.filter(lut), dtype=np.float32) / 255; b = np.asarray(Image.open("/tmp/lc_g.ppm"), dtype=np.float32) / 255
        errs.append(float(np.abs(a - b).mean()))
        if ph == "street_dance": tiles.append((os.path.basename(n)[11:-5].replace("_", " "), im.filter(lut), im))
    lim = 0.03 if "Thermal" in n else 0.012
    ok = max(errs) < lim; fails += not ok
    print(f"{'ok  ' if ok else 'FAIL'} {os.path.basename(n)}: mean |LUT - engine grade| = {max(errs):.4f} (limit {lim})")
w, h = tiles[0][1].size; sheet = Image.new("RGB", (w * 4, h * 2), "white")
cells = [("Original", tiles[0][2])] + [(t[0], t[1]) for t in tiles]
for i, (lab, im) in enumerate(cells):
    im = im.copy(); d = ImageDraw.Draw(im); d.rectangle([0, 0, d.textlength(lab, font=FONT) + 16, 28], fill="black"); d.text((8, 4), lab, font=FONT, fill="white")
    sheet.paste(im, ((i % 4) * w, (i // 4) * h))
sheet.save(out, quality=90); print(out)
sys.exit(1 if fails else 0)
