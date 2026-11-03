import struct
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from parse_bar import BarArchive, parse_bar, safe_output_path


def build_archive(entries):
    payload = bytearray()
    payload_records = []
    for filename, contents, compressed in entries:
        data_offset = 28 + len(payload)
        if compressed:
            stored = b"l33t" + struct.pack("<I", len(contents)) + zlib.compress(contents)
            recorded_output_size = len(stored)
        else:
            stored = contents
            recorded_output_size = len(contents)
        payload.extend(stored)
        payload_records.append((filename, data_offset, stored, recorded_output_size))

    directory_offset = 28 + len(payload)
    offsets = []
    records = bytearray()
    for filename, data_offset, stored, recorded_output_size in payload_records:
        offsets.append(len(records))
        records.extend(
            struct.pack(
                "<IIIQ",
                data_offset,
                len(stored),
                recorded_output_size,
                0,
            )
        )
        records.extend(filename.encode("ascii") + b"\0")

    header = struct.pack("<QIIIII", 0, 0, len(entries), 0, directory_offset, 8)
    offset_table = b"".join(struct.pack("<I", offset) for offset in offsets)
    return header + payload + offset_table + records


class BarParserTests(unittest.TestCase):
    def setUp(self):
        self.temp_dir = tempfile.TemporaryDirectory()
        self.archive_path = Path(self.temp_dir.name) / "sample.bar"
        self.raw_contents = b"RTS3 raw entry"
        self.compressed_contents = b"XMB contents " * 128
        self.archive_path.write_bytes(
            build_archive(
                [
                    ("data/raw.bin", self.raw_contents, False),
                    ("proto.xmb", self.compressed_contents, True),
                ]
            )
        )
        self.archive = parse_bar(self.archive_path)

    def tearDown(self):
        self.temp_dir.cleanup()

    def test_reads_directory_offsets_and_null_terminated_names(self):
        self.assertEqual(len(self.archive.entries), 2)
        self.assertEqual(self.archive.entries[0].filename, "data/raw.bin")
        self.assertEqual(self.archive.entries[1].filename, "proto.xmb")

    def test_extracts_raw_and_l33t_entries(self):
        self.assertEqual(self.archive.extract("DATA\\RAW.BIN"), self.raw_contents)
        self.assertEqual(self.archive.extract("proto.xmb"), self.compressed_contents)

    def test_rejects_truncated_directory(self):
        broken = Path(self.temp_dir.name) / "broken.bar"
        broken.write_bytes(self.archive_path.read_bytes()[:32])
        with self.assertRaises(ValueError):
            BarArchive(broken)

    def test_extract_paths_cannot_escape_output_directory(self):
        output_dir = Path(self.temp_dir.name) / "output"
        self.assertEqual(
            safe_output_path(output_dir, "data/raw.bin"),
            output_dir.resolve() / "data" / "raw.bin",
        )
        with self.assertRaises(ValueError):
            safe_output_path(output_dir, "..\\outside.bin")


if __name__ == "__main__":
    unittest.main()
