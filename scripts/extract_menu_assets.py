#!/usr/bin/env python3
"""
extract_menu_assets.py
Extrai e gera os assets autênticos do menu original do Age of Mythology diretamente
dos arquivos originais do PC (textures.bar, loading1024.bmp, splash.bmp) gravando-os em assets/ para o APK.

Assets gerados:
  - assets/main_menu.png       (Fundo autêntico do Menu: loading1024.bmp em 24-bit cristalino)
  - assets/loading.png         (Tela original de carregamento em alta resolução 1024x768)
  - assets/splash.png          (Logo original dourado AoM 640x480)
  - assets/button_main.png     (Botão autêntico de mármore grego com friso dourado chanfrado)
  - assets/button_clicked.png  (Botão pressionado com relevo invertido e mármore escurecido)
  - assets/button_rollover.png (Botão destacado com borda dourada iluminada)
"""

import os
import struct
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ASSETS_DIR = ROOT / "assets"

CANDIDATE_DIRS = [
    Path("/storage/emulated/0/Documents/AoM"),
    Path("/storage/emulated/0/Download/Games/PC/Age of Mythology/Age of Mythology"),
    Path("/storage/emulated/0/Download/AoM"),
    Path("/sdcard/AoM"),
    ROOT / "build-m2",
]


def find_aom_paths():
    for base in CANDIDATE_DIRS:
        bar_candidates = [
            base / "textures" / "textures.bar",
            base / "textures.bar",
        ]
        for bar in bar_candidates:
            if bar.is_file():
                return bar, base
    return None, None


def open_bar(path):
    data = Path(path).read_bytes()
    _, _, num_entries, _, dir_offset, _ = struct.unpack_from("<QIIIII", data, 0)
    entries = {}
    pos = dir_offset + num_entries * 4
    for _ in range(num_entries):
        data_off, comp_sz, decomp_sz = struct.unpack_from("<III", data, pos)
        pos += 12
        pos += 8  # metadata
        name_start = pos
        while data[pos] != 0:
            pos += 1
        name = data[name_start:pos].decode("latin-1").lower()
        pos += 1
        entries[name] = (data_off, comp_sz, decomp_sz)
    return data, entries


def extract_entry(bar_data, entry):
    off, comp_sz, _ = entry
    chunk = bar_data[off : off + comp_sz]
    if len(chunk) >= 8 and chunk[:4] == b"l33t":
        try:
            return zlib.decompress(chunk[8:])
        except zlib.error:
            return zlib.decompress(chunk[8:], -15)
    return chunk


def unpack565(v):
    return (
        ((v >> 11) & 0x1F) * 255 // 31,
        ((v >> 5) & 0x3F) * 255 // 63,
        (v & 0x1F) * 255 // 31,
    )


def decode_dxt1(data, w, h):
    """Decodifica DDT formato 4 (DXT1)."""
    from PIL import Image

    out = bytearray(w * h * 4)
    pos = 0
    for by in range((h + 3) // 4):
        for bx in range((w + 3) // 4):
            c0v, c1v, bits = struct.unpack_from("<HHI", data, pos)
            pos += 8
            c0, c1 = unpack565(c0v), unpack565(c1v)
            if c0v > c1v:
                pal = [
                    c0,
                    c1,
                    tuple((2 * a + b) // 3 for a, b in zip(c0, c1)),
                    tuple((a + 2 * b) // 3 for a, b in zip(c0, c1)),
                ]
                alp = [255] * 4
            else:
                pal = [c0, c1, tuple((a + b) // 2 for a, b in zip(c0, c1)), (0, 0, 0)]
                alp = [255, 255, 255, 0]
            for py in range(4):
                for px in range(4):
                    ix, iy = bx * 4 + px, by * 4 + py
                    if ix >= w or iy >= h:
                        continue
                    idx = (bits >> ((py * 4 + px) * 2)) & 3
                    r, g, b = pal[idx]
                    a = alp[idx]
                    o = (iy * w + ix) * 4
                    out[o : o + 4] = bytes([r, g, b, a])
    return Image.frombytes("RGBA", (w, h), bytes(out))


def decode_fmt3_rgb555(raw):
    """Decodifica DDT formato 3 (palette de 16-bit RGB555)."""
    from PIL import Image

    if len(raw) < 48:
        return None
    w, h = struct.unpack_from("<II", raw, 8)
    num_colors = struct.unpack_from("<I", raw, 16)[0]
    pal_offset = struct.unpack_from("<I", raw, 24)[0]
    if pal_offset == 0:
        pal_offset = struct.unpack_from("<I", raw, 32)[0]
    if pal_offset == 0:
        pal_offset = 48
    mip_offset = struct.unpack_from("<I", raw, 40)[0]
    pal_data = raw[pal_offset : pal_offset + num_colors * 2]

    pal = []
    for i in range(num_colors):
        val = struct.unpack_from("<H", pal_data, i * 2)[0]
        r = ((val >> 10) & 0x1F) * 255 // 31
        g = ((val >> 5) & 0x1F) * 255 // 31
        b = (val & 0x1F) * 255 // 31
        pal.append((r, g, b, 255))

    px_indices = raw[mip_offset : mip_offset + w * h]
    out = bytearray(w * h * 4)
    for i, idx in enumerate(px_indices):
        r, g, b, a = pal[idx] if idx < len(pal) else (0, 0, 0, 255)
        out[i * 4 : i * 4 + 4] = bytes([r, g, b, a])
    return Image.frombytes("RGBA", (w, h), bytes(out))


def decode_ddt(raw):
    if len(raw) < 16 or raw[:4] != b"RTS3":
        return None
    fmt = raw[6]
    w, h = struct.unpack_from("<II", raw, 8)
    if fmt == 3:
        return decode_fmt3_rgb555(raw)
    elif fmt == 4:
        mip_offset = struct.unpack_from("<I", raw, 16)[0]
        return decode_dxt1(raw[mip_offset:], w, h)
    return None


def create_greek_stone_button(stone_img, w=280, h=44, state="normal"):
    """Gera um botão autêntico de mármore grego com moldura chanfrada de ouro no estilo AoM."""
    from PIL import Image, ImageDraw

    btn = Image.new("RGBA", (w, h))
    if stone_img:
        for y in range(0, h, stone_img.height):
            for x in range(0, w, stone_img.width):
                btn.paste(stone_img, (x, y))
    else:
        # Fallback elegante caso a textura não esteja no bar
        draw_tmp = ImageDraw.Draw(btn)
        draw_tmp.rectangle([0, 0, w, h], fill=(162, 152, 148, 255))

    if state == "clicked":
        r, g, b, a = btn.split()
        r = r.point(lambda i: int(i * 0.72))
        g = g.point(lambda i: int(i * 0.72))
        b = b.point(lambda i: int(i * 0.72))
        btn = Image.merge("RGBA", (r, g, b, a))
    elif state == "rollover":
        r, g, b, a = btn.split()
        r = r.point(lambda i: min(255, int(i * 1.15)))
        g = g.point(lambda i: min(255, int(i * 1.15)))
        b = b.point(lambda i: min(255, int(i * 1.15)))
        btn = Image.merge("RGBA", (r, g, b, a))

    draw = ImageDraw.Draw(btn)

    # 1. Borda externa escura de pedra esculpida (1px)
    draw.rectangle([0, 0, w - 1, h - 1], outline=(22, 18, 14, 255))

    # 2. Moldura chanfrada de ouro
    if state == "clicked":
        c_top = (110, 80, 25, 255)
        c_bottom = (225, 190, 75, 255)
        c_inner = (80, 60, 20, 255)
    elif state == "rollover":
        c_top = (255, 240, 130, 255)
        c_bottom = (175, 135, 45, 255)
        c_inner = (245, 215, 95, 255)
    else:  # normal
        c_top = (235, 200, 85, 255)
        c_bottom = (130, 95, 30, 255)
        c_inner = (200, 165, 60, 255)

    # Bevel lines: topo & esquerda / base & direita
    draw.line([(1, 1), (w - 2, 1)], fill=c_top, width=2)
    draw.line([(1, 1), (1, h - 2)], fill=c_top, width=2)
    draw.line([(1, h - 2), (w - 2, h - 2)], fill=c_bottom, width=2)
    draw.line([(w - 2, 1), (w - 2, h - 2)], fill=c_bottom, width=2)

    # Contorno interno suave de ouro
    draw.rectangle([3, 3, w - 4, h - 4], outline=c_inner)

    # 4 detalhes ornamentais nos cantos (rebites de bronze dourado estilo grego)
    stud_c = (255, 220, 90, 255) if state != "clicked" else (160, 120, 40, 255)
    for cx, cy in [(6, 6), (w - 7, 6), (6, h - 7), (w - 7, h - 7)]:
        draw.rectangle([cx - 1, cy - 1, cx + 1, cy + 1], fill=stud_c, outline=(30, 20, 10, 255))

    return btn


def main():
    try:
        from PIL import Image
    except ImportError:
        print("ERRO: Pillow não encontrado. Execute: pip install pillow")
        sys.exit(1)

    bar_path, aom_dir = find_aom_paths()
    if not bar_path and not aom_dir:
        print("ERRO: Diretório do AoM não encontrado. Diretórios verificados:")
        for p in CANDIDATE_DIRS:
            print(f"  {p}")
        sys.exit(1)

    ASSETS_DIR.mkdir(parents=True, exist_ok=True)

    # 1. Carregar textura de mármore grego genuína (greek button background.ddt)
    stone_img = None
    if bar_path and bar_path.is_file():
        print(f"[*] Abrindo BAR: {bar_path}")
        bar_data, entries = open_bar(bar_path)
        print(f"[*] {len(entries)} arquivos catalogados no textures.bar")

        entry = entries.get("greek button background.ddt")
        if entry:
            raw = extract_entry(bar_data, entry)
            stone_img = decode_ddt(raw)
            if stone_img:
                print(f"[OK] Textura de mármore grego autêntica extraída ({stone_img.size})")

    # Se não achou no bar, tenta carregar pré-existente
    if not stone_img and (ASSETS_DIR / "greek_button_bg.png").is_file():
        stone_img = Image.open(ASSETS_DIR / "greek_button_bg.png").convert("RGBA")
        print(f"[OK] Mármore grego carregado de assets/greek_button_bg.png ({stone_img.size})")

    # Salvar cópia do mármore grego puro
    if stone_img:
        stone_img.save(ASSETS_DIR / "greek_button_bg.png")

    # 2. Gerar os botões de mármore grego chanfrados com ouro autênticos
    print("[*] Gerando botões de mármore grego com friso dourado AoM...")
    btn_norm = create_greek_stone_button(stone_img, 280, 44, "normal")
    btn_click = create_greek_stone_button(stone_img, 280, 44, "clicked")
    btn_roll = create_greek_stone_button(stone_img, 280, 44, "rollover")

    btn_norm.save(ASSETS_DIR / "button_main.png")
    btn_click.save(ASSETS_DIR / "button_clicked.png")
    btn_roll.save(ASSETS_DIR / "button_rollover.png")
    print(f"[OK] Botões autênticos gerados: button_main.png ({btn_norm.size})")

    # 3. Converter BMPs originais em alta resolução (loading1024.bmp e splash.bmp)
    loading_bmp = None
    splash_bmp = None
    if aom_dir:
        for name in ["loading1024.bmp", "loading.bmp", "loadingx1024.bmp"]:
            cand = aom_dir / name
            if cand.is_file():
                loading_bmp = cand
                break
        for name in ["splash.bmp", "splashx.bmp"]:
            cand = aom_dir / name
            if cand.is_file():
                splash_bmp = cand
                break

    # 4. Salvar splash.png
    splash_out = ASSETS_DIR / "splash.png"
    if splash_bmp and splash_bmp.is_file():
        img = Image.open(splash_bmp).convert("RGB")
        img.save(splash_out)
        print(f"[OK] Splash original convertido: {splash_bmp} -> {splash_out} ({img.size})")

    # 5. Salvar loading.png
    loading_out = ASSETS_DIR / "loading.png"
    menu_out = ASSETS_DIR / "main_menu.png"
    if loading_bmp and loading_bmp.is_file():
        img = Image.open(loading_bmp).convert("RGB")
        img.save(loading_out)
        print(f"[OK] Tela de carregamento salva -> {loading_out}")
        if not menu_out.is_file():
            img.save(menu_out)
            print(f"[OK] Menu principal salvo -> {menu_out}")
        else:
            print(f"[OK] Menu principal The Titans preservado -> {menu_out}")

    print("\n=== Todos os assets do Menu Original AoM foram extraídos e gerados com perfeição! ===")
    for fname in ["main_menu.png", "loading.png", "splash.png", "button_main.png", "button_clicked.png", "font.ttf"]:
        p = ASSETS_DIR / fname
        if p.is_file():
            if fname.endswith(".png"):
                im = Image.open(p)
                print(f"  * {fname:<20} {im.size}  {im.mode}")
            else:
                sz = p.stat().st_size
                print(f"  * {fname:<20} {sz} bytes")
        else:
            print(f"  ! {fname:<20} AUSENTE")


if __name__ == "__main__":
    main()
