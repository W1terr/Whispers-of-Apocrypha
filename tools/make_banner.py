"""Mod page image: an Apocrypha screenshot turned into the green nightmare, Hermaeus Mora's eyes (the game's own
eyeballtile.dds) crowding in from the edges, and the title in plain Windows fonts.

usage: python make_banner.py <screenshot> <eyeballtile.dds or .png> <output.png>   (needs numpy + pillow)
"""

import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

W, H = 1920, 1080
FONTS = "C:/Windows/Fonts/"


def load(path):
    return np.asarray(Image.open(path).convert("RGB").resize((W, H), Image.LANCZOS)).astype(np.float32) / 255.0


def to_image(a):
    return Image.fromarray((np.clip(a, 0, 1) * 255).astype(np.uint8))


def edge_field():
    """0 in the middle of the frame, 1 at the edges (rounded, wider at the corners)."""
    y, x = np.mgrid[0:H, 0:W].astype(np.float32)
    nx = (x / W - 0.5) * 2.0
    ny = (y / H - 0.5) * 2.0
    return np.clip(np.sqrt(nx ** 4 + ny ** 4) ** 0.5, 0, 1.5)


def nightmare(scene, edge):
    """What the mod does to the screen: blurred edges, a sick green tint, less color, darkness."""
    blurred = np.asarray(to_image(scene).filter(ImageFilter.GaussianBlur(9))).astype(np.float32) / 255.0
    k = np.clip((edge - 0.35) / 0.6, 0, 1)[..., None]
    out = scene * (1 - k) + blurred * k
    lum = out @ np.array([0.299, 0.587, 0.114], np.float32)
    out = out * 0.45 + lum[..., None] * 0.55                       # desaturate
    out = out * np.array([0.55, 1.0, 0.62], np.float32) * 1.25     # green
    out *= (1.0 - 0.8 * np.clip(edge - 0.5, 0, 1))[..., None]      # darker toward the edges
    return out


def eyes_layer(texture_path, edge, seed=7):
    """The eyeball texture tiled at a few scales, faded in only toward the edges of the frame."""
    tex = Image.open(texture_path).convert("RGBA")
    rng = np.random.default_rng(seed)
    color = np.zeros((H, W, 3), np.float32)
    alpha = np.zeros((H, W), np.float32)
    for size, strength in ((1500, 1.0), (1050, 0.8)):
        t = tex.resize((size, size), Image.LANCZOS)
        ox, oy = int(rng.integers(size)), int(rng.integers(size))
        canvas = Image.new("RGBA", (W + size, H + size))
        for yy in range(0, H + size, size):
            for xx in range(0, W + size, size):
                canvas.paste(t, (xx, yy))
        tile = np.asarray(canvas.crop((ox, oy, ox + W, oy + H))).astype(np.float32) / 255.0
        a = tile[..., 3] * strength
        color = color * (1 - a[..., None]) + tile[..., :3] * a[..., None]
        alpha = np.maximum(alpha, a)
    # they creep in unevenly: a soft noise pushes the edge of the swarm in and out
    noise = Image.fromarray((rng.random((9, 16)) * 255).astype(np.uint8)).resize((W, H), Image.BICUBIC)
    noise = np.asarray(noise).astype(np.float32) / 255.0
    mask = np.clip((edge + (noise - 0.5) * 0.35 - 0.62) / 0.4, 0, 1) ** 1.2
    return color, alpha * mask


def text_with_glow(base, xy, text, font, fill, glow, glow_radius, anchor="mm"):
    layer = Image.new("RGBA", base.size, (0, 0, 0, 0))
    ImageDraw.Draw(layer).text(xy, text, font=font, fill=glow, anchor=anchor)
    base.alpha_composite(layer.filter(ImageFilter.GaussianBlur(glow_radius)))
    ImageDraw.Draw(base).text(xy, text, font=font, fill=fill, anchor=anchor)


def main():
    screenshot, texture, output = sys.argv[1:4]
    scene = load(screenshot)
    edge = edge_field()

    img = nightmare(scene, edge)
    eye_color, eye_alpha = eyes_layer(texture, edge)
    eye_color = eye_color * np.array([1.25, 1.15, 0.7], np.float32) * 1.6  # the golden irises catch the light
    img = img * (1 - eye_alpha[..., None]) + eye_color * eye_alpha[..., None]

    # a dark band behind the title so it reads on any part of the picture
    y = np.arange(H, dtype=np.float32)[:, None]
    band = np.exp(-((y - H * 0.5) / (H * 0.16)) ** 2) * 0.55
    img *= (1 - band)[..., None]
    banner = to_image(img).convert("RGBA")

    title = ImageFont.truetype(FONTS + "georgia.ttf", 118)
    subtitle = ImageFont.truetype(FONTS + "georgiai.ttf", 40)
    features = ImageFont.truetype(FONTS + "segoeui.ttf", 30)
    text_with_glow(banner, (W // 2, H // 2 - 58), "WHISPERS OF APOCRYPHA", title, (226, 238, 214, 255), (90, 255, 120, 200), 18)
    text_with_glow(banner, (W // 2, H // 2 + 42), "Stay in the dark too long, and Hermaeus Mora will see you.", subtitle,
                   (198, 214, 190, 255), (0, 0, 0, 255), 6)
    text_with_glow(banner, (W // 2, H // 2 + 112), "WHISPERS   \u00b7   THE GREEN NIGHTMARE   \u00b7   THE EYES OF HERMAEUS MORA", features,
                   (150, 210, 150, 255), (0, 0, 0, 255), 5)

    banner.convert("RGB").save(output, quality=95)
    print(f"wrote {output}")


if __name__ == "__main__":
    main()
