import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from tools.convert_bik_to_mp4 import (
    STARTUP_VIDEOS,
    ALL_VIDEOS,
    PLAYLISTS,
    build_ffmpeg_command,
    convert_startup_videos,
    convert_videos,
)


class ConvertBikToMp4Tests(unittest.TestCase):
    def test_command_targets_android_compatible_codecs(self):
        command = build_ffmpeg_command(
            "ffmpeg", Path("intro.bik"), Path("intro.mp4"), False
        )

        self.assertIn("libx264", command)
        self.assertIn("baseline", command)
        self.assertIn("yuv420p", command)
        self.assertIn("aac", command)
        self.assertIn("0:a?", command)
        self.assertIn("-n", command)

    def test_converts_base_game_startup_playlist(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            game_dir = root / "game"
            avi_dir = game_dir / "avi"
            output_dir = root / "converted"
            avi_dir.mkdir(parents=True)
            for source_name, _ in STARTUP_VIDEOS:
                (avi_dir / source_name).write_bytes(b"test Bink input")

            def fake_ffmpeg(command, check):
                Path(command[-1]).write_bytes(b"test MP4 output")

            with patch("tools.convert_bik_to_mp4.subprocess.run", side_effect=fake_ffmpeg) as run:
                converted = convert_startup_videos(
                    game_dir, output_dir, "ffmpeg", False
                )

            self.assertEqual(
                [path.name for path in converted],
                ["aom-logo.mp4", "introcg.mp4"],
            )
            self.assertTrue(all(path.stat().st_size > 0 for path in converted))
            self.assertEqual(run.call_count, 2)

    def test_converts_all_cinematics_including_titans_and_outtakes(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            game_dir = root / "game"
            avi_dir = game_dir / "avi"
            output_dir = root / "converted"
            avi_dir.mkdir(parents=True)
            for source_name, _ in ALL_VIDEOS:
                (avi_dir / source_name).write_bytes(b"fake bik")

            def fake_ffmpeg(command, check):
                Path(command[-1]).write_bytes(b"fake mp4")

            with patch("tools.convert_bik_to_mp4.subprocess.run", side_effect=fake_ffmpeg) as run:
                converted = convert_videos(
                    game_dir, output_dir, "ffmpeg", False, ALL_VIDEOS
                )

            self.assertEqual(len(converted), 6)
            self.assertIn("xintrocg.mp4", [p.name for p in converted])
            self.assertIn("outtakes.mp4", [p.name for p in converted])
            self.assertEqual(run.call_count, 6)

    def test_converts_named_playlist(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            game_dir = root / "game"
            avi_dir = game_dir / "avi"
            output_dir = root / "converted"
            avi_dir.mkdir(parents=True)
            for source_name, _ in PLAYLISTS["xplaylist"]:
                (avi_dir / source_name).write_bytes(b"fake bik")

            def fake_ffmpeg(command, check):
                Path(command[-1]).write_bytes(b"fake mp4")

            with patch("tools.convert_bik_to_mp4.subprocess.run", side_effect=fake_ffmpeg) as run:
                converted = convert_videos(
                    game_dir, output_dir, "ffmpeg", False, PLAYLISTS["xplaylist"]
                )

            self.assertEqual([p.name for p in converted], ["aom-logo.mp4", "xintrocg.mp4"])
            self.assertEqual(run.call_count, 2)

    def test_does_not_overwrite_existing_output_without_opt_in(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            avi_dir = root / "game" / "avi"
            output_dir = root / "converted"
            avi_dir.mkdir(parents=True)
            for source_name, _ in STARTUP_VIDEOS:
                (avi_dir / source_name).write_bytes(b"test Bink input")
            output_dir.mkdir()
            existing = output_dir / STARTUP_VIDEOS[0][1]
            existing.write_bytes(b"keep")

            with patch("tools.convert_bik_to_mp4.subprocess.run") as run:
                with self.assertRaises(FileExistsError):
                    convert_startup_videos(root / "game", output_dir, "ffmpeg", False)

            self.assertEqual(existing.read_bytes(), b"keep")
            run.assert_not_called()

    def test_missing_original_video_is_reported(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            avi_dir = root / "game" / "avi"
            avi_dir.mkdir(parents=True)

            with self.assertRaises(FileNotFoundError):
                convert_startup_videos(
                    root / "game", root / "converted", "ffmpeg", False
                )


if __name__ == "__main__":
    unittest.main()
