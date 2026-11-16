# SPDX-License-Identifier: LGPL-2.1-or-later
"""screenshot.py <png> centred <rrggbb>: is the area of that colour centred?

Used by build/regress.sh for tests with a "Screenshot:" line. The image is
converted to BMP with sips (part of macOS), so no image library is needed.
Prints what it found; exits 0 when the coloured area covers at least a
fifth of the screen and its margins left and right, and top and bottom,
match to within 1% of the screen (at least 4 pixels).
"""

import os
import struct
import subprocess
import sys
import tempfile


def load_bmp(png):
    with tempfile.TemporaryDirectory() as d:
        bmp = os.path.join(d, "s.bmp")
        subprocess.run(["sips", "-s", "format", "bmp", png, "--out", bmp],
                       check=True, capture_output=True)
        data = open(bmp, "rb").read()
    offset = struct.unpack_from("<I", data, 10)[0]
    width, height = struct.unpack_from("<ii", data, 18)
    bpp = struct.unpack_from("<H", data, 28)[0]
    stride = ((width * bpp // 8) + 3) & ~3
    top_down = height < 0
    height = abs(height)

    def pixel(x, y):
        row = y if top_down else height - 1 - y
        o = offset + row * stride + x * bpp // 8
        return data[o + 2], data[o + 1], data[o]

    return width, height, pixel


def main():
    png, check, colour = sys.argv[1], sys.argv[2], sys.argv[3]
    if check != "centred":
        sys.exit(f"screenshot.py: unknown check {check}")
    want = tuple(int(colour[i:i + 2], 16) for i in (0, 2, 4))
    width, height, pixel = load_bmp(png)

    left, top, right, bottom = width, height, -1, -1
    step = 2  # every other pixel is plenty for a solid rectangle
    for y in range(0, height, step):
        for x in range(0, width, step):
            r, g, b = pixel(x, y)
            if abs(r - want[0]) < 40 and abs(g - want[1]) < 40 and abs(b - want[2]) < 40:
                left, right = min(left, x), max(right, x)
                top, bottom = min(top, y), max(bottom, y)
    if right < 0:
        print(f"screenshot: no #{colour} pixels in the {width}x{height} screenshot")
        return 1

    margins = (left, width - 1 - right, top, height - 1 - bottom)
    area = (right - left + 1) * (bottom - top + 1) / (width * height)
    tol_x, tol_y = max(4, width // 100) + step, max(4, height // 100) + step
    ok = (area >= 0.2 and abs(margins[0] - margins[1]) <= tol_x and
          abs(margins[2] - margins[3]) <= tol_y)
    print(f"screenshot: #{colour} area {left},{top}-{right},{bottom} of {width}x{height} "
          f"({area:.0%}); margins left {margins[0]} right {margins[1]} top {margins[2]} "
          f"bottom {margins[3]}: {'centred' if ok else 'NOT centred'}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
