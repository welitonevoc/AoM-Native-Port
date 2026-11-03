import json
import os
import unittest
from pathlib import Path

class VideoDatabaseTests(unittest.TestCase):
    def setUp(self):
        self.repo_root = Path(__file__).resolve().parents[1]
        self.json_path = self.repo_root / "assets" / "video" / "video_database.json"
        self.header_path = self.repo_root / "include" / "video" / "video_database_gen.h"

    def test_json_database_exists_and_has_all_six_cinematics(self):
        self.assertTrue(self.json_path.exists(), "video_database.json must exist")
        with open(self.json_path, "r", encoding="utf-8") as f:
            data = json.load(f)

        self.assertEqual(data["total_videos"], 6)
        expected_videos = {
            "aom logo.bik",
            "introcg.bik",
            "xintrocg.bik",
            "outtakes.bik",
            "credits.bik",
            "xcredits.bik",
        }
        self.assertEqual(set(data["videos"].keys()), expected_videos)

        # Check resolution and frame rate
        for vid, vdata in data["videos"].items():
            self.assertEqual(vdata["header"]["width"], 640)
            self.assertEqual(vdata["header"]["height"], 480)
            self.assertGreater(vdata["header"]["num_frames"], 0)
            self.assertGreater(vdata["header"]["duration_sec"], 0)
            self.assertTrue(vdata["converted_mp4"].endswith(".mp4"))

    def test_json_playlists_structure(self):
        with open(self.json_path, "r", encoding="utf-8") as f:
            data = json.load(f)

        self.assertIn("playlist", data["playlists"])
        self.assertIn("xplaylist", data["playlists"])
        self.assertIn("outtakes", data["playlists"])
        self.assertIn("credits", data["playlists"])
        self.assertIn("xcredits", data["playlists"])

        # Classic playlist has 2 videos
        self.assertEqual(
            data["playlists"]["playlist"]["videos"],
            ["AOM LOGO.bik", "introcg.bik"],
        )
        # Titans playlist has 2 videos
        self.assertEqual(
            data["playlists"]["xplaylist"]["videos"],
            ["AOM LOGO.bik", "xintrocg.bik"],
        )

    def test_cpp_header_integrity(self):
        self.assertTrue(self.header_path.exists(), "video_database_gen.h must exist")
        with open(self.header_path, "r", encoding="utf-8") as f:
            content = f.read()

        self.assertIn("enum class CinematicId", content)
        self.assertIn("kAllCinematics", content)
        self.assertIn("enum class PlaylistId", content)
        self.assertIn("kAllPlaylists", content)
        self.assertIn("AOM_LOGO", content)
        self.assertIn("XINTRO_CG", content)
        self.assertIn("OUTTAKES", content)

        # Balanced braces and brackets
        self.assertEqual(content.count("{"), content.count("}"))
        self.assertEqual(content.count("("), content.count(")"))
        self.assertEqual(content.count("["), content.count("]"))


if __name__ == "__main__":
    unittest.main()
