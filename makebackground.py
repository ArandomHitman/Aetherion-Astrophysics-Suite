##
##  ⚠️  MODIFY AT YOUR OWN RISK  ⚠️
##  ╔═══════════════════════════════════════╗
##  ║   This script generates the default   ║
##  ║   background image. Changes to the    ║
##  ║   resolution, colors, or algorithms   ║
##  ║   may produce unexpected results and  ║
##  ║   require careful testing.            ║
##  ╚═══════════════════════════════════════╝
##

## ---------------- Structured imports ---------------- ##
from PIL import Image, ImageFilter, ImageDraw
import numpy as np
import random, math

## ---------------- Main code ---------------- ##
random.seed(99)
np.random.seed(99)
w, h = 4096, 2048 ## this is the resolution of the background image, the seed is for reproducibility of the random star placement and nebula patterns. 
## Adjusting the resolution will affect the level of detail and performance, but 4096x2048 is a good balance for a high-quality background without being too large.

# Work on a 1.5x-wide canvas, then blend the edges to make a seamless horizontal wrap.
ww = w + w // 2  # extra width for blending

# Base: near-black with slight blue tint
arr = np.full((h, ww, 3), [2, 2, 6], dtype=np.float32)


def make_seamless_h(layer_arr, canvas_w, final_w):
    out = layer_arr[:, :final_w, :].copy()
    overlap = canvas_w - final_w
    # The extra region wraps back over the left edge with a linear crossfade
    for x in range(overlap):
        t = x / float(overlap)  # 0 at left, 1 at right of overlap
        out[:, x, :] = out[:, x, :] * t + layer_arr[:, final_w + x, :] * (1.0 - t) ## you've been hit by you've been struck by a smooth 𝚌̶𝚛̶𝚒̶𝚖̶𝚒̶𝚗̶𝚊̶𝚕̶ wraparound
    return out


## ------------ L1: Large diffuse nebula clouds ------------ ##
nebula = Image.new("RGB", (ww, h), (0, 0, 0))
nd = ImageDraw.Draw(nebula)

nebula_colors = [
    (8, 3, 12), (4, 6, 14), (10, 3, 5), (3, 8, 9),
    (7, 2, 10), (3, 4, 11), (9, 5, 3), (2, 7, 8),
]

for _ in range(18):
    cx = random.randint(-200, ww + 200)
    cy = random.randint(-200, h + 200)
    rx = random.randint(250, 900)
    ry = random.randint(200, 650)
    col = random.choice(nebula_colors)
    nd.ellipse([cx - rx, cy - ry, cx + rx, cy + ry], fill=col)

for _ in range(10):
    nebula = nebula.filter(ImageFilter.GaussianBlur(radius=70))
print("  Nebula layer done")

neb_arr = np.array(nebula, dtype=np.float32) * 0.5
neb_arr = make_seamless_h(neb_arr, ww, w)

## ------------ L2: Wispy clouds and subtle color variations ------------ ##
wisps = Image.new("RGB", (ww, h), (0, 0, 0))
wd = ImageDraw.Draw(wisps)

wisp_colors = [(14, 4, 18), (6, 10, 20), (16, 5, 7), (4, 12, 14), (12, 3, 15)]

for _ in range(8):
    cx = random.randint(0, ww)
    cy = random.randint(0, h)
    rx = random.randint(100, 400)
    ry = random.randint(80, 300)
    col = wisp_colors[random.randint(0, len(wisp_colors) - 1)]
    wd.ellipse([cx - rx, cy - ry, cx + rx, cy + ry], fill=col)

for _ in range(6):
    wisps = wisps.filter(ImageFilter.GaussianBlur(radius=40))
print("  Wisps layer done")

wisp_arr = np.array(wisps, dtype=np.float32) * 0.25
wisp_arr = make_seamless_h(wisp_arr, ww, w)

# Combine base + nebulas + wisps
arr = np.full((h, w, 3), [2, 2, 6], dtype=np.float32)
arr += neb_arr
arr += wisp_arr

## ------------ L3: Stars of var. brightness ------------ ##
# Dim background stars (25000)
ys = np.random.randint(0, h, 25000)
xs = np.random.randint(0, w, 25000)
bs = np.random.randint(40, 121, 25000).astype(np.float32)
for i in range(25000):
    b = bs[i]
    tint = random.choice([
        (b, b, b), (b, b, b * 0.8), (b * 0.8, b * 0.85, b), (b, b * 0.9, b * 0.7)
    ])
    arr[ys[i], xs[i]] = [tint[0], tint[1], tint[2]]

# Medium stars (6000)
ys = np.random.randint(0, h, 6000)
xs = np.random.randint(0, w, 6000)
bs = np.random.randint(130, 211, 6000).astype(np.float32)
for i in range(6000):
    b = bs[i]
    tint = random.choice([
        (b, b, b), (b, b, b * 0.85), (b * 0.85, b * 0.9, b), (b, b * 0.88, b * 0.65)
    ])
    arr[ys[i], xs[i]] = [tint[0], tint[1], tint[2]]
print("  Star scatter done")

# Bright stars with soft glow (1200)
star_arr = np.zeros((h, w, 3), dtype=np.float32)
for _ in range(1200):
    sx = random.randint(2, w - 3)
    sy = random.randint(2, h - 3)
    b = random.randint(200, 255)
    temp = random.random()
    if temp < 0.3:
        col = np.array([b, b, b], dtype=np.float32)
    elif temp < 0.5:
        col = np.array([b, b * 0.85, b * 0.6], dtype=np.float32)
    elif temp < 0.65:
        col = np.array([b, b * 0.7, b * 0.5], dtype=np.float32)
    elif temp < 0.8:
        col = np.array([b * 0.7, b * 0.8, b], dtype=np.float32)
    else:
        col = np.array([b * 0.6, b * 0.7, b], dtype=np.float32)

    for dx in range(-2, 3):
        for dy in range(-2, 3):
            dist = math.sqrt(dx * dx + dy * dy)
            if dist > 2.5:
                continue
            f = max(0.0, 1.0 - dist / 2.5)
            f = f * f
            nx, ny = sx + dx, sy + dy
            if 0 <= nx < w and 0 <= ny < h:
                star_arr[ny, nx] += col * f

# Slight blur for glow
star_img = Image.fromarray(np.clip(star_arr, 0, 255).astype(np.uint8))
star_img = star_img.filter(ImageFilter.GaussianBlur(radius=0.7))
arr += np.array(star_img, dtype=np.float32)
print("  Bright stars done")

## ------------ L4: Nebula and color variants ------------ ##
for _ in range(80):
    sx = random.randint(3, w - 4)
    sy = random.randint(3, h - 4)
    temp = random.random()
    if temp < 0.4:
        col = np.array([255, 255, 255], dtype=np.float32)
    elif temp < 0.6:
        col = np.array([255, 240, 180], dtype=np.float32)
    elif temp < 0.8:
        col = np.array([180, 210, 255], dtype=np.float32)
    else:
        col = np.array([255, 200, 150], dtype=np.float32)

    for dx in range(-3, 4):
        for dy in range(-3, 4):
            dist = math.sqrt(dx * dx + dy * dy)
            if dist > 3.5:
                continue
            f = max(0.0, 1.0 - dist / 3.5) ** 1.5
            nx, ny = sx + dx, sy + dy
            if 0 <= nx < w and 0 <= ny < h:
                arr[ny, nx] += col * f

## ------------ Finalize and save ------------ ##

result = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8))
result.save("build/background.png", "PNG")
print(f"Saved build/background.png ({w}x{h})")
