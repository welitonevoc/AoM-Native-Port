#!/usr/bin/env python3
"""
tools/compile_avi.py - Video & Cinematic Metadata Compiler for Age of Mythology Native Port
Parses Bink Video files (.bik) and playlist definitions (.aom) from E:/Port/Age of Mythology/avi.
Generates:
  1. assets/video/video_database.json (Structured metadata for all cinematics and playlists)
  2. include/video/video_database_gen.h (Typed C++20 definitions, enums and tables)
"""

import os
import sys
import struct
import json
from pathlib import Path

DEFAULT_AVI_DIR = r"E:\Port\Age of Mythology\avi"

VIDEO_TITLE_MAP = {
    "aom logo.bik": "Logos Microsoft Games & Ensemble Studios",
    "introcg.bik": "A Queda do Tridente - Abertura Cinematográfica",
    "xintrocg.bik": "A Nova Atlântida (The Titans) - Abertura Cinematográfica",
    "outtakes.bik": "Erros de Gravação & Cenas Deletadas (Outtakes)",
    "credits.bik": "Créditos Finais Clássicos",
    "xcredits.bik": "Créditos Finais The Titans",
}

MP4_FILENAME_MAP = {
    "aom logo.bik": "aom-logo.mp4",
    "introcg.bik": "introcg.mp4",
    "xintrocg.bik": "xintrocg.mp4",
    "outtakes.bik": "outtakes.mp4",
    "credits.bik": "credits.mp4",
    "xcredits.bik": "xcredits.mp4",
}

ENUM_IDENTIFIER_MAP = {
    "aom logo.bik": "AOM_LOGO",
    "introcg.bik": "INTRO_CG",
    "xintrocg.bik": "XINTRO_CG",
    "outtakes.bik": "OUTTAKES",
    "credits.bik": "CREDITS",
    "xcredits.bik": "XCREDITS",
}

def parse_bik_header(filepath):
    try:
        with open(filepath, "rb") as f:
            data = f.read(52)
    except Exception as e:
        print(f"Error reading {filepath}: {e}", file=sys.stderr)
        return None

    if len(data) < 44:
        return None

    magic = data[:4]
    if magic not in (b"BIKi", b"BIKb", b"BIKh", b"BIKg", b"BIKf"):
        return None

    file_size, num_frames = struct.unpack("<II", data[4:12])
    max_frame_size = struct.unpack("<I", data[12:16])[0]
    num_frames2 = struct.unpack("<I", data[16:20])[0]
    width, height = struct.unpack("<II", data[20:28])
    fps_dividend, fps_divider = struct.unpack("<II", data[28:36])
    flags = struct.unpack("<I", data[36:40])[0]
    num_audio_tracks = struct.unpack("<I", data[40:44])[0]

    fps = (fps_dividend / fps_divider) if fps_divider != 0 else 30.0
    duration_sec = (num_frames / fps) if fps != 0 else 0.0

    return {
        "magic": magic.decode("ascii", errors="replace"),
        "file_size": os.path.getsize(filepath),
        "num_frames": num_frames,
        "width": width,
        "height": height,
        "fps": round(fps, 2),
        "duration_sec": round(duration_sec, 2),
        "num_audio_tracks": num_audio_tracks,
        "flags": flags,
    }

def parse_aom_playlist(filepath):
    videos = []
    with open(filepath, "rb") as f:
        content = f.read().decode("latin-1", errors="replace")
    for line in content.splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        # Format usually: avi\AOM LOGO.bik or avi/introcg.bik
        norm = line.replace("/", "\\")
        parts = norm.split("\\")
        video_filename = parts[-1]
        videos.append(video_filename)
    return videos

def compile_avi(avi_dir, repo_root):
    avi_path = Path(avi_dir)
    if not avi_path.exists():
        print(f"Directory not found: {avi_path}", file=sys.stderr)
        return False

    print(f"[*] Scanning AVI directory: {avi_path}")

    # 1. Parse all .bik files
    videos_data = {}
    for filename in sorted(os.listdir(avi_path)):
        if filename.lower().endswith(".bik"):
            fp = avi_path / filename
            header = parse_bik_header(fp)
            if header:
                low_name = filename.lower()
                clean_key = filename
                # Match key in title map case-insensitively
                for k in VIDEO_TITLE_MAP:
                    if k.lower() == low_name:
                        clean_key = k
                        break

                videos_data[clean_key] = {
                    "id": ENUM_IDENTIFIER_MAP.get(clean_key, clean_key.upper().replace(" ", "_").replace(".", "_")),
                    "original_bik": filename,
                    "converted_mp4": MP4_FILENAME_MAP.get(clean_key, filename.replace(".bik", ".mp4")),
                    "title": VIDEO_TITLE_MAP.get(clean_key, filename),
                    "header": header
                }
                print(f"  [BIK] {filename:18s} | {header['width']}x{header['height']} @ {header['fps']} fps | {header['duration_sec']:.1f}s | Audio: {header['num_audio_tracks']}")

    # 2. Parse all .aom playlist files
    playlists_data = {}
    for filename in sorted(os.listdir(avi_path)):
        if filename.lower().endswith(".aom"):
            fp = avi_path / filename
            vids = parse_aom_playlist(fp)
            playlist_name = filename[:-4]
            playlists_data[playlist_name] = {
                "file": filename,
                "videos": vids,
                "mp4_sequence": [MP4_FILENAME_MAP.get(v.lower(), v.replace(".bik", ".mp4")) for v in vids]
            }
            print(f"  [AOM] {filename:18s} -> {vids}")

    # Add virtual playlists if not present
    if "outtakes" not in playlists_data:
        playlists_data["outtakes"] = {
            "file": "outtakes.aom (virtual)",
            "videos": ["outtakes.bik"],
            "mp4_sequence": ["outtakes.mp4"]
        }

    # 3. Export assets/video/video_database.json
    out_json_dir = Path(repo_root) / "assets" / "video"
    out_json_dir.mkdir(parents=True, exist_ok=True)
    out_json_file = out_json_dir / "video_database.json"

    export_json = {
        "version": "1.0",
        "source_directory": str(avi_path),
        "total_videos": len(videos_data),
        "total_playlists": len(playlists_data),
        "videos": videos_data,
        "playlists": playlists_data
    }

    with open(out_json_file, "w", encoding="utf-8") as f:
        json.dump(export_json, f, indent=2, ensure_ascii=False)
    print(f"[+] Exported JSON database: {out_json_file} ({out_json_file.stat().st_size} bytes)")

    # 4. Generate C++ Header: include/video/video_database_gen.h
    out_h_dir = Path(repo_root) / "include" / "video"
    out_h_dir.mkdir(parents=True, exist_ok=True)
    out_h_file = out_h_dir / "video_database_gen.h"

    cpp_lines = [
        "// Auto-generated by tools/compile_avi.py",
        "// Forensic video metadata and playlist definitions for Age of Mythology",
        "#pragma once",
        "",
        "#include <cstdint>",
        "#include <string_view>",
        "#include <array>",
        "",
        "namespace aom {",
        "namespace video {",
        "",
        "enum class CinematicId : uint32_t {",
    ]

    for key, data in sorted(videos_data.items()):
        cpp_lines.append(f"    {data['id']},")
    cpp_lines.extend([
        "    COUNT",
        "};",
        "",
        "struct CinematicMeta {",
        "    CinematicId id;",
        "    std::string_view original_bik;",
        "    std::string_view converted_mp4;",
        "    std::string_view title;",
        "    uint32_t width;",
        "    uint32_t height;",
        "    float fps;",
        "    uint32_t num_frames;",
        "    float duration_sec;",
        "    uint32_t audio_tracks;",
        "    uint64_t file_size;",
        "};",
        "",
        f"inline constexpr std::array<CinematicMeta, {len(videos_data)}> kAllCinematics = {{{{",
    ])

    for key, data in sorted(videos_data.items()):
        h = data["header"]
        cpp_lines.append(f"    {{CinematicId::{data['id']}, \"{data['original_bik']}\", \"{data['converted_mp4']}\", \"{data['title']}\", {h['width']}, {h['height']}, {h['fps']}f, {h['num_frames']}, {h['duration_sec']}f, {h['num_audio_tracks']}, {h['file_size']}ULL}},")

    cpp_lines.extend([
        "}};",
        "",
        "enum class PlaylistId : uint32_t {",
        "    OPENING_CLASSIC,     // playlist.aom (AOM LOGO -> introcg)",
        "    OPENING_TITANS,      // xplaylist.aom (AOM LOGO -> xintrocg)",
        "    CINEMATIC_CLASSIC,   // aomcinematic.aom (introcg)",
        "    CINEMATIC_TITANS,    // aomxcinematic.aom (xintrocg)",
        "    CREDITS_CLASSIC,     // credits.aom (credits)",
        "    CREDITS_TITANS,      // xcredits.aom (xcredits)",
        "    OUTTAKES,            // outtakes (outtakes)",
        "    COUNT",
        "};",
        "",
        "struct PlaylistDef {",
        "    PlaylistId id;",
        "    std::string_view name;",
        "    std::string_view title;",
        "    std::array<std::string_view, 2> mp4_files;",
        "    size_t file_count;",
        "};",
        "",
        "inline constexpr std::array<PlaylistDef, 7> kAllPlaylists = {{",
        "    {PlaylistId::OPENING_CLASSIC,   \"playlist\",      \"Abertura Clássica (Fall of the Trident)\",  {{\"aom-logo.mp4\", \"introcg.mp4\"}}, 2},",
        "    {PlaylistId::OPENING_TITANS,    \"xplaylist\",     \"Abertura The Titans (The New Atlantis)\",   {{\"aom-logo.mp4\", \"xintrocg.mp4\"}}, 2},",
        "    {PlaylistId::CINEMATIC_CLASSIC, \"aomcinematic\",  \"Cinemática Clássica\",                      {{\"introcg.mp4\", \"\"}}, 1},",
        "    {PlaylistId::CINEMATIC_TITANS,  \"aomxcinematic\", \"Cinemática The Titans\",                   {{\"xintrocg.mp4\", \"\"}}, 1},",
        "    {PlaylistId::CREDITS_CLASSIC,   \"credits\",       \"Créditos Finais Clássicos\",                {{\"credits.mp4\", \"\"}}, 1},",
        "    {PlaylistId::CREDITS_TITANS,    \"xcredits\",      \"Créditos The Titans\",                      {{\"xcredits.mp4\", \"\"}}, 1},",
        "    {PlaylistId::OUTTAKES,          \"outtakes\",      \"Erros de Gravação (Outtakes)\",             {{\"outtakes.mp4\", \"\"}}, 1},",
        "}};",
        "",
        "} // namespace video",
        "} // namespace aom",
        ""
    ])

    with open(out_h_file, "w", encoding="utf-8") as f:
        f.write("\n".join(cpp_lines))
    print(f"[+] Generated C++ header: {out_h_file}")

    return True

def main():
    repo_root = Path(__file__).resolve().parents[1]
    avi_dir = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_AVI_DIR
    success = compile_avi(avi_dir, repo_root)
    if not success:
        sys.exit(1)
    print("[+] Video compilation completed successfully!")

if __name__ == "__main__":
    main()
