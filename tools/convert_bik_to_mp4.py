#!/usr/bin/env python3
"""Convert the original AoM Bink videos for Android's H.264 player."""

from __future__ import annotations

import argparse
import shutil
import subprocess
from pathlib import Path


STARTUP_VIDEOS = (
    ("AOM LOGO.bik", "aom-logo.mp4"),
    ("introcg.bik", "introcg.mp4"),
)

ALL_VIDEOS = (
    ("AOM LOGO.bik", "aom-logo.mp4"),
    ("introcg.bik", "introcg.mp4"),
    ("xintrocg.bik", "xintrocg.mp4"),
    ("outtakes.bik", "outtakes.mp4"),
    ("credits.bik", "credits.mp4"),
    ("xcredits.bik", "xcredits.mp4"),
)

PLAYLISTS = {
    "playlist": (("AOM LOGO.bik", "aom-logo.mp4"), ("introcg.bik", "introcg.mp4")),
    "xplaylist": (("AOM LOGO.bik", "aom-logo.mp4"), ("xintrocg.bik", "xintrocg.mp4")),
    "aomcinematic": (("introcg.bik", "introcg.mp4"),),
    "aomxcinematic": (("xintrocg.bik", "xintrocg.mp4"),),
    "credits": (("credits.bik", "credits.mp4"),),
    "xcredits": (("xcredits.bik", "xcredits.mp4"),),
    "outtakes": (("outtakes.bik", "outtakes.mp4"),),
}


def build_ffmpeg_command(
    ffmpeg: str, source: Path, destination: Path, overwrite: bool
) -> list[str]:
    return [
        ffmpeg,
        "-hide_banner",
        "-loglevel",
        "warning",
        "-y" if overwrite else "-n",
        "-i",
        str(source),
        "-map",
        "0:v:0",
        "-map",
        "0:a?",
        "-vf",
        "scale=trunc(iw/2)*2:trunc(ih/2)*2",
        "-c:v",
        "libx264",
        "-profile:v",
        "baseline",
        "-level:v",
        "3.0",
        "-preset",
        "medium",
        "-crf",
        "20",
        "-pix_fmt",
        "yuv420p",
        "-c:a",
        "aac",
        "-b:a",
        "128k",
        "-movflags",
        "+faststart",
        str(destination),
    ]


def convert_videos(
    game_dir: Path,
    output_dir: Path,
    ffmpeg: str,
    overwrite: bool,
    video_list: tuple[tuple[str, str], ...] = STARTUP_VIDEOS,
) -> list[Path]:
    avi_dir = game_dir / "avi"
    output_dir.mkdir(parents=True, exist_ok=True)
    converted: list[Path] = []

    for source_name, output_name in video_list:
        source = avi_dir / source_name
        if not source.is_file():
            raise FileNotFoundError(f"Original game video not found: {source}")

        destination = output_dir / output_name
        if destination.exists() and not overwrite:
            raise FileExistsError(
                f"Output already exists (pass --overwrite to replace it): {destination}"
            )

        command = build_ffmpeg_command(ffmpeg, source, destination, overwrite)
        subprocess.run(command, check=True)
        if not destination.is_file() or destination.stat().st_size == 0:
            raise RuntimeError(f"FFmpeg produced no video output: {destination}")
        converted.append(destination)

    return converted


def convert_startup_videos(
    game_dir: Path, output_dir: Path, ffmpeg: str, overwrite: bool
) -> list[Path]:
    return convert_videos(game_dir, output_dir, ffmpeg, overwrite, STARTUP_VIDEOS)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--game-dir",
        required=True,
        type=Path,
        help="Age of Mythology installation directory containing avi/",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path(__file__).resolve().parents[1] / "build" / "cinematics",
        help="Output directory (default: build/cinematics in the project)",
    )
    parser.add_argument(
        "--ffmpeg",
        default=shutil.which("ffmpeg"),
        help="FFmpeg executable (default: ffmpeg from PATH)",
    )
    parser.add_argument(
        "--overwrite",
        action="store_true",
        help="Replace converted files that already exist",
    )
    parser.add_argument(
        "--all",
        action="store_true",
        help="Convert all AoM cinematics (Vanilla + The Titans + Outtakes + Credits)",
    )
    parser.add_argument(
        "--playlist",
        choices=list(PLAYLISTS.keys()),
        help="Convert a specific playlist (playlist, xplaylist, outtakes, credits, etc.)",
    )
    parser.add_argument(
        "--video",
        help="Convert a specific video by original filename (e.g. outtakes.bik)",
    )
    args = parser.parse_args()

    if not args.ffmpeg:
        parser.error("FFmpeg was not found; pass its executable with --ffmpeg")

    if args.all:
        selected_videos = ALL_VIDEOS
    elif args.playlist:
        selected_videos = PLAYLISTS[args.playlist]
    elif args.video:
        target = args.video.lower()
        matched = [v for v in ALL_VIDEOS if v[0].lower() == target]
        if not matched:
            out_name = Path(args.video).stem + ".mp4"
            selected_videos = ((args.video, out_name),)
        else:
            selected_videos = (matched[0],)
    else:
        selected_videos = STARTUP_VIDEOS

    try:
        converted = convert_videos(
            args.game_dir, args.output_dir, args.ffmpeg, args.overwrite, selected_videos
        )
    except (FileNotFoundError, FileExistsError, RuntimeError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"ERROR: {error}\n")

    for path in converted:
        print(f"Converted: {path}")
    print(
        "Package these files with tools/package_apk.py or scripts/package_apk.sh; "
        "they are intentionally kept outside the source assets."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
