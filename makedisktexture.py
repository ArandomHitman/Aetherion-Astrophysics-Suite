## ---- Structured imports ---- ##
from PIL import Image
import math

## ---- Main code ---- ##

size = 512 # 512 x 512, can be modified but this is not recommended for performance and scaling reasons
img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
pixels = img.load()

center = size / 2.0
r_in = size * 0.22
r_out = size * 0.48

for y in range(size): # for each pixel, calculate its distance from the center and determine its color and alpha based on that distance
    for x in range(size):
        dx = x - center
        dy = y - center
        r = math.sqrt(dx * dx + dy * dy)
        if r < r_in or r > r_out: ## if the pixel is outside the disk, make it fully transparent
            continue
        t = (r - r_in) / (r_out - r_in)
        # Alpha map brightens at inner edge, fades outward
        alpha = int(255 * (1 - t) ** 1.5)
        # Color map blue-white inside, yellow-orange outside
        rcol = min(255, int(255 * (1 - t) + 255 * t))
        gcol = min(255, int(220 * (1 - t) + 120 * t))
        bcol = min(255, int(255 * (1 - t) + 40 * t))
        pixels[x, y] = (rcol, gcol, bcol, alpha)

## ---- Save the image ---- ##

img.save("src/disk_texture.png") 
print("Saved src/disk_texture.png")
