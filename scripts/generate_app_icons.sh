#!/system/bin/sh
# -------------------------------------------------------------------------
# generate_app_icons.sh
# -------------------------------------------------------------------------
# Gera o ícone do launcher (ic_launcher.png) em todas as densidades Android
# a partir da imagem base assets/icon.png (192x192). Não requer Pillow,
# ImageMagick ou qualquer dependência externa — apenas Python 3.
#
# Densidades alvo (padrão Android):
#   mdpi      48x48  (base)
#   hdpi      72x72  (1,5x)
#   xhdpi     96x96  (2x)
#   xxhdpi   144x144  (3x)
#   xxxhdpi  192x192  (4x)
# -------------------------------------------------------------------------

set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC_ICON="$ROOT/assets/icon.png"
RES_DIR="$ROOT/android/app/src/main/res"

DENSITIES="
mdpi:48
hdpi:72
xhdpi:96
xxhdpi:144
xxxhdpi:192
"

if [ ! -f "$SRC_ICON" ]; then
    echo "ERRO: ícone base não encontrado: $SRC_ICON" >&2
    exit 1
fi

python3 - "$SRC_ICON" "$RES_DIR" <<'PYEOF'
import sys, os, zlib, struct, math

src_path = sys.argv[1]
res_dir  = sys.argv[2]

densities = [
    ("mdpi",     48),
    ("hdpi",     72),
    ("xhdpi",    96),
    ("xxhdpi",  144),
    ("xxxhdpi", 192),
]

def chunk(tag, data):
    return struct.pack('!I', len(data)) + tag + data + struct.pack('!I', zlib.crc32(tag + data) & 0xffffffff)

def read_png(path):
    with open(path, 'rb') as f:
        data = f.read()
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError(f"{path}: não é um PNG válido")
    pos = 8
    width = height = None
    raw_idat = b''
    while pos < len(data):
        length = struct.unpack('!I', data[pos:pos+4])[0]; pos += 4
        ctype  = data[pos:pos+4]; pos += 4
        cdata  = data[pos:pos+length]; pos += length
        pos += 4  # CRC
        if ctype == b'IHDR':
            width, height = struct.unpack('!II', cdata[:8])
        elif ctype == b'IDAT':
            raw_idat += cdata
        elif ctype == b'IEND':
            break
    if width is None or height is None:
        raise ValueError(f"{path}: sem IHDR")
    # Descomprimir IDAT
    import zlib as _zlib
    decompressed = _zlib.decompress(raw_idat)
    # Desfazer filtros (apenas filter type 0 esperado, mas tratar genérico)
    stride = width * 4  # RGBA
    pixels = []
    row_idx = 0
    # Reconstruir linhas
    out_rows = []
    i = 0
    prev_row = [0] * stride
    while i < len(decompressed):
        filter_type = decompressed[i]; i += 1
        row = list(decompressed[i:i+stride]); i += stride
        if filter_type == 0:
            pass  # row já está correta
        elif filter_type == 1:  # Sub
            for x in range(4, stride):
                row[x] = (row[x] + row[x-4]) & 0xff
        elif filter_type == 2:  # Up
            for x in range(stride):
                row[x] = (row[x] + prev_row[x]) & 0xff
        elif filter_type == 3:  # Average
            for x in range(stride):
                a = row[x-4] if x >= 4 else 0
                b = prev_row[x]
                row[x] = (row[x] + ((a + b) // 2)) & 0xff
        elif filter_type == 4:  # Paeth
            for x in range(stride):
                a = row[x-4] if x >= 4 else 0
                b = prev_row[x]
                c = prev_row[x-4] if x >= 4 else 0
                p = a + b - c
                pa, pb, pc = abs(p-a), abs(p-b), abs(p-c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                row[x] = (row[x] + pr) & 0xff
        out_rows.append(row)
        prev_row = row
    # Converter para lista de tuplas RGBA
    rgba = []
    for row in out_rows:
        for x in range(0, stride, 4):
            rgba.append((row[x], row[x+1], row[x+2], row[x+3]))
    return width, height, rgba

def write_png(path, width, height, rgba):
    # IHDR
    ihdr = struct.pack('!IIBBBBB', width, height, 8, 6, 0, 0, 0)
    # Montar scanlines com filtro 0
    raw = bytearray()
    for y in range(height):
        raw.append(0)
        for x in range(width):
            r, g, b, a = rgba[y * width + x]
            raw.extend([r, g, b, a])
    idat = zlib.compress(bytes(raw), 9)
    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b'IHDR', ihdr)
           + chunk(b'IDAT', idat)
           + chunk(b'IEND', b''))
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'wb') as f:
        f.write(png)

def resize_bilinear(src_w, src_h, src_rgba, dst_w, dst_h):
    """Redimensiona com interpolação bilinear (qualidade adequada para ícones)."""
    dst = []
    for y in range(dst_h):
        fy = (y + 0.5) * src_h / dst_h - 0.5
        y0 = int(math.floor(fy)); y1 = min(y0 + 1, src_h - 1)
        fy_frac = fy - y0
        for x in range(dst_w):
            fx = (x + 0.5) * src_w / dst_w - 0.5
            x0 = int(math.floor(fx)); x1 = min(x0 + 1, src_w - 1)
            fx_frac = fx - x0
            for c in range(4):
                v00 = src_rgba[y0 * src_w + x0][c]
                v01 = src_rgba[y0 * src_w + x1][c]
                v10 = src_rgba[y1 * src_w + x0][c]
                v11 = src_rgba[y1 * src_w + x1][c]
                top   = v00 * (1 - fx_frac) + v01 * fx_frac
                bottom = v10 * (1 - fx_frac) + v11 * fx_frac
                val   = top * (1 - fy_frac) + bottom * fy_frac
                dst.append(round(val))
            # Não estamos montando RGBA corretamente aqui; refazer abaixo
        # placeholder
    # Reimplementar de forma mais clara: retornar lista de tuplas
    dst_rgba = []
    for y in range(dst_h):
        fy = (y + 0.5) * src_h / dst_h - 0.5
        y0 = int(math.floor(fy)); y1 = min(y0 + 1, src_h - 1)
        fy_frac = fy - y0
        for x in range(dst_w):
            fx = (x + 0.5) * src_w / dst_w - 0.5
            x0 = int(math.floor(fx)); x1 = min(x0 + 1, src_w - 1)
            fx_frac = fx - x0
            rgba_out = []
            for c in range(4):
                v00 = src_rgba[y0 * src_w + x0][c]
                v01 = src_rgba[y0 * src_w + x1][c]
                v10 = src_rgba[y1 * src_w + x0][c]
                v11 = src_rgba[y1 * src_w + x1][c]
                top   = v00 * (1 - fx_frac) + v01 * fx_frac
                bottom = v10 * (1 - fx_frac) + v11 * fx_frac
                val   = top * (1 - fy_frac) + bottom * fy_frac
                rgba_out.append(round(val))
            dst_rgba.append(tuple(rgba_out))
    return dst_rgba

# Ler ícone base
base_w, base_h, base_rgba = read_png(src_path)
print(f"Ícone base: {base_w}x{base_h}")

for name, size in densities:
    out_rgba = resize_bilinear(base_w, base_h, base_rgba, size, size)
    out_path = os.path.join(res_dir, f"mipmap-{name}", "ic_launcher.png")
    write_png(out_path, size, size, out_rgba)
    print(f"Gerado: mipmap-{name}/ic_launcher.png ({size}x{size})")

# Pasta base mipmap/ (fallback mdpi) — usa 48x48
out_rgba = resize_bilinear(base_w, base_h, base_rgba, 48, 48)
out_path = os.path.join(res_dir, "mipmap", "ic_launcher.png")
write_png(out_path, 48, 48, out_rgba)
print(f"Gerado: mipmap/ic_launcher.png (48x48, fallback mdpi)")

print("\nTodos os ícones de launcher foram gerados com sucesso.")
PYEOF

echo "=== Ícones gerados ==="
ls -la "$RES_DIR"/mipmap*/ic_launcher.png "$RES_DIR"/mipmap/ic_launcher.png 2>/dev/null
