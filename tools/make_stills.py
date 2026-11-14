# Renders before/after stills of every LENSKID look on free stock photos (see stock/CREDITS.txt).
import subprocess, sys, os
from PIL import Image, ImageDraw, ImageFont
R = os.path.dirname(os.path.abspath(__file__)) + "/.."
BIN = sys.argv[1] if len(sys.argv) > 1 else R + "/build_render_still"
OUT = sys.argv[2] if len(sys.argv) > 2 else R + "/dist/stills"
os.makedirs(OUT, exist_ok=True)
NAMES = ["Boketto Moody Vintage", "Overexposed Dream", "Dashcam Junk", "Super 8", "Melt Signal", "Infrared Ghost", "Thermal Heart"]
SLUG = ["01-boketto", "02-overexposed-dream", "03-dashcam-junk", "04-super8", "05-melt-signal", "06-infrared-ghost", "07-thermal-heart"]
# (photo, extra args, label suffix)
PLAN = {0: [("dance_park", [], ""), ("street_dance", [], "")], 1: [("dance_park", [], ""), ("golden_portrait", [], "")],
        2: [("road_spring", [], ""), ("road_windshield", [], "")], 3: [("street_dance", ["leaks=1"], " + light leaks"), ("dance_park", [], "")],
        4: [("street_dance", ["7"], ""), ("dance_park", ["0", "beat=1"], " on a beat hit")], 5: [("green_lawn", [], ""), ("street_dance", [], "")],
        6: [("dance_park", [], ""), ("street_dance", [], ""), ("dance_park", ["real=0.45"], " + 45% real colour")]}
FONT = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 26)
def prep(name, H=720):
    im = Image.open(f"{R}/stock/{name}.jpg").convert("RGB")
    im = im.resize((round(im.width * H / im.height) // 2 * 2 + 1 if name == "golden_portrait" else round(im.width * H / im.height), H), Image.LANCZOS)
    p = f"/tmp/lk_{name}.ppm"; im.save(p); return im, p
def label(im, text):
    d = ImageDraw.Draw(im); d.rectangle([0, 0, d.textlength(text, font=FONT) + 24, 42], fill=(0, 0, 0)); d.text((12, 6), text, font=FONT, fill=(255, 255, 255))
only = [int(a) for a in sys.argv[3:]] if len(sys.argv) > 3 else range(7)
for L in only:
    for j, (name, extra, suffix) in enumerate(PLAN[L]):
        before, p = prep(name)
        o = f"/tmp/lk_out_{L}_{j}.ppm"
        subprocess.run([BIN, p, o, str(L)] + extra, check=True)
        after = Image.open(o)
        c = Image.new("RGB", (before.width * 2 + 8, before.height), (255, 255, 255))
        c.paste(before, (0, 0)); c.paste(after, (before.width + 8, 0))
        b2 = c.crop((0, 0, before.width, before.height)); label(b2, "Before"); c.paste(b2, (0, 0))
        a2 = c.crop((before.width + 8, 0, c.width, c.height)); label(a2, f"LENSKID: {NAMES[L]}{suffix}"); c.paste(a2, (before.width + 8, 0))
        c.save(f"{OUT}/LENSKID-{SLUG[L]}-{j + 1}.jpg", quality=90)
        print(f"{OUT}/LENSKID-{SLUG[L]}-{j + 1}.jpg")
