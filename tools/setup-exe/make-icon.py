# Draws setup.ico: a small dock on a blue-violet rounded square.
from PIL import Image, ImageDraw
S = 1024
img = Image.new("RGBA", (S, S))
grad = Image.new("RGBA", (S, S))
gd = ImageDraw.Draw(grad)
for y in range(S):
    t = y / S
    gd.line([(0, y), (S, y)], fill=(int(94 + (10 - 94) * t), int(92 + (132 - 92) * t), int(230 + (255 - 230) * t), 255))
mask = Image.new("L", (S, S))
ImageDraw.Draw(mask).rounded_rectangle([40, 40, S - 40, S - 40], 220, fill=255)
img.paste(grad, (0, 0), mask)
d = ImageDraw.Draw(img, "RGBA")
d.rounded_rectangle([170, 190, S - 170, 250], 30, fill=(255, 255, 255, 150))
dw, dh = 740, 210
dx, dy = (S - dw) // 2, 640
d.rounded_rectangle([dx, dy, dx + dw, dy + dh], 70, fill=(255, 255, 255, 110))
cols = [(255, 214, 10), (48, 209, 88), (255, 69, 58), (255, 255, 255)]
ts = 140
gap = (dw - 4 * ts) / 5
for i, c in enumerate(cols):
    x = dx + gap + i * (ts + gap)
    d.rounded_rectangle([x, dy + 35, x + ts, dy + 35 + ts], 36, fill=c + (255,))
img.save("setup.ico", sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
