#!/usr/bin/env python3
"""Prepare the user-selected official GM320 product image; run on Box.

The photograph remains Ant Esports material, outside the project's MIT grant.
No mouse geometry or branding is generated or replaced.
"""
import argparse
import hashlib
import pathlib
import urllib.request
from PIL import Image

SOURCE = "https://antesports.com/cdn/shop/files/PNG_1_bb769bf4-56b8-4907-bd34-43b9dfff9af4.png"
SHA256 = "81ff8cbea1a94e718367f0ef187fc60310432eeb94ba8247fd5c544b6bf54318"
CROP = (242, 170, 759, 963)

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--source", type=pathlib.Path, default=pathlib.Path("artifacts/image-source/gm320-original.png"))
parser.add_argument("--output", type=pathlib.Path, default=pathlib.Path("assets"))
args = parser.parse_args()
if not args.source.exists():
    args.source.parent.mkdir(parents=True, exist_ok=True)
    request = urllib.request.Request(SOURCE, headers={"User-Agent": "UniversalMouseApp asset preparation"})
    args.source.write_bytes(urllib.request.urlopen(request, timeout=30).read())
assert hashlib.sha256(args.source.read_bytes()).hexdigest() == SHA256, "Official image revision changed; review before using it"
image = Image.open(args.source).convert("RGBA").crop(CROP)
neutral = image.copy()
mask = Image.new("RGBA", image.size)
pixels, gray, alpha = image.load(), neutral.load(), mask.load()
for y in range(image.height):
    for x in range(image.width):
        r, g, b, a = pixels[x, y]
        high, low = max(r, g, b), min(r, g, b)
        # Preserve the printed product branding. Only RGB strips/wheel tint.
        logo = 160 < x < 375 and 555 < y < 670
        if a == 0 or logo or high < 60 or high - low < 40:
            continue
        coverage = (high - low) / high
        level = (r * .2126 + g * .7152 + b * .0722) * .20
        gray[x, y] = tuple(round(c * (1 - coverage) + level * coverage) for c in (r, g, b)) + (a,)
        alpha[x, y] = (255, 255, 255, round(a * coverage))
args.output.mkdir(parents=True, exist_ok=True)
for name, prepared in (("gm320-top.png", image), ("gm320-neutral.png", neutral), ("gm320-light-mask.png", mask)):
    prepared.save(args.output / name, optimize=True)
    print(name, prepared.size, hashlib.sha256((args.output / name).read_bytes()).hexdigest())
