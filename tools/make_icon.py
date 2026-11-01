# Draws Radiance's icon from the same geometry as ui::DrawMark (src/ui/Icons.cpp): a lit
# centre with eight rays, each ending in a bead, on a 20-unit grid. Supersampled, then
# reduced, so every size is anti-aliased. Small sizes get slightly heavier strokes so the
# rays survive at 16 px.
#   python tools/make_icon.py res [docs]   (docs: also the README logos)
import math
import sys
from PIL import Image, ImageDraw, ImageFilter

out_dir = sys.argv[1]
SIZES = [16, 20, 24, 32, 40, 48, 64, 128, 256]
SS = 8  # supersampling

PERIWINKLE = (171, 189, 255)  # the Radiance theme's accent (dark)
DEEP = (88, 104, 214)          # its light-theme accent, for light taskbars


def mark(size, colour):
    big = size * SS
    img = Image.new('RGBA', (big, big), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    u = big / 20.6  # the mark spans about 20.4 units; a hair of margin
    c = big / 2.0
    heavy = 1.0 if size >= 32 else (1.25 if size >= 24 else 1.45)

    # A soft glow behind the centre at the larger sizes, as the app draws it.
    if size >= 32:
        glow = Image.new('RGBA', (big, big), (0, 0, 0, 0))
        gd = ImageDraw.Draw(glow)
        r = 7.5 * u
        gd.ellipse([c - r, c - r, c + r, c + r], fill=colour + (46,))
        glow = glow.filter(ImageFilter.GaussianBlur(radius=u * 1.6))
        img = Image.alpha_composite(img, glow)
        d = ImageDraw.Draw(img)

    def disc(x, y, r):
        d.ellipse([x - r, y - r, x + r, y + r], fill=colour + (255,))

    disc(c, c, 3.1 * u * min(heavy, 1.15))
    width = 1.7 * u * heavy
    for i in range(8):
        a = math.pi / 4 * i - math.pi / 2
        dx, dy = math.cos(a), math.sin(a)
        x0, y0 = c + dx * 5.6 * u, c + dy * 5.6 * u
        x1, y1 = c + dx * 8.2 * u, c + dy * 8.2 * u
        d.line([x0, y0, x1, y1], fill=colour + (255,), width=max(1, round(width)))
        disc(c + dx * 8.9 * u, c + dy * 8.9 * u, 1.3 * u * heavy)
    return img.resize((size, size), Image.LANCZOS)


for name, colour in (('radiance.ico', PERIWINKLE), ('radiance-light.ico', DEEP)):
    images = [mark(s, colour) for s in SIZES]
    images[-1].save(f'{out_dir}/{name}', format='ICO', sizes=[(s, s) for s in SIZES], append_images=images[:-1])

# A preview on dark and light grounds, every size.
preview = Image.new('RGBA', (sum(SIZES) + 12 * len(SIZES) + 12, 2 * 256 + 36), (0, 0, 0, 255))
pd = ImageDraw.Draw(preview)
pd.rectangle([0, 0, preview.width, 256 + 18], fill=(32, 32, 34, 255))
pd.rectangle([0, 256 + 18, preview.width, preview.height], fill=(238, 238, 238, 255))
x = 12
for s in SIZES:
    preview.alpha_composite(mark(s, PERIWINKLE), (x, 12 + (256 - s) // 2))
    preview.alpha_composite(mark(s, DEEP), (x, 256 + 24 + (256 - s) // 2))
    x += s + 12
preview.save(f'{out_dir}/preview.png')

# The README's logo, one for each GitHub theme.
if len(sys.argv) > 2:
    mark(256, PERIWINKLE).save(f'{sys.argv[2]}/logo.png')
    mark(256, DEEP).save(f'{sys.argv[2]}/logo-light.png')
print('ok')
