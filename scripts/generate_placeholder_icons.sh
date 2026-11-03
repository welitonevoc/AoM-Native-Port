#!/usr/bin/env bash

# -------------------------------------------------------------------------
# generate_placeholder_icons.sh
# -------------------------------------------------------------------------
# This script creates three simple placeholder PNG icons (play, options, quit)
# in the "assets/icons" directory. The icons are 64 × 64 pixels and are
# filled with solid colours so you can see them instantly in the UI.
#
# The script uses an embedded Python snippet that builds a PNG from raw
# RGBA data (no external dependencies). It works on Android Termux, Linux
# and macOS as long as Python 3 is available.
# -------------------------------------------------------------------------

set -e

ICON_DIR="$(dirname "$0")/../assets/icons"
mkdir -p "$ICON_DIR"

PYTHON_SCRIPT=$(cat <<'EOF'
import os, sys, zlib, struct, base64

def make_png(width, height, color):
    """Generate a solid‑color RGBA PNG.
    `color` is a 4‑tuple (R, G, B, A) with 0‑255 values.
    """
    def chunk(tag, data):
        return struct.pack('!I', len(data)) + tag + data + struct.pack('!I', zlib.crc32(tag + data) & 0xffffffff)
    # IHDR chunk (width, height, bit depth=8, color type=6=RGBA)
    ihdr = struct.pack('!IIBBBBB', width, height, 8, 6, 0, 0, 0)
    # Image data – each scanline prefixed with filter type 0
    raw = bytearray()
    r, g, b, a = color
    for _ in range(height):
        raw.append(0)  # no filter
        raw.extend([r, g, b, a] * width)
    idat = zlib.compress(bytes(raw), 9)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b'IHDR', ihdr) + chunk(b'IDAT', idat) + chunk(b'IEND', b'')
    return png

def write_icon(name, color):
    png = make_png(64, 64, color)
    path = os.path.join(sys.argv[1], f"{name}.png")
    with open(path, 'wb') as f:
        f.write(png)
    print(f"Created {path}")

if __name__ == '__main__':
    # argv[1] is the destination folder
    dest = sys.argv[1]
    write_icon('play',    (0, 200, 0, 255))   # green
    write_icon('options', (0, 0, 200, 255))   # blue
    write_icon('quit',    (200, 0, 0, 255))   # red
EOF
)

python3 - <<PYTHON "$ICON_DIR"
$PYTHON_SCRIPT
PYTHON

echo "All placeholder icons have been generated in $ICON_DIR"