#!/usr/bin/env python3
"""Read and extract entries from Age of Mythology BAR archives."""

import argparse
import struct
import sys
import zlib
from dataclasses import dataclass
from pathlib import Path, PureWindowsPath
from typing import Dict, List, Optional, Union


BAR_HEADER = struct.Struct("<QIIIII")
ENTRY_HEADER = struct.Struct("<IIIQ")
BAR_HEADER_SIZE = BAR_HEADER.size
ENTRY_HEADER_SIZE = ENTRY_HEADER.size
L33T_MAGIC = b"l33t"
L33T_HEADER_SIZE = 8


@dataclass(frozen=True)
class BarEntry:
    """A directory record from a BAR archive."""

    filename: str
    data_offset: int
    compressed_size: int
    decompressed_size: int
    metadata: int
    is_compressed: bool


class BarArchive:
    """Parsed BAR archive with case-insensitive lookup and extraction."""

    def __init__(self, path: Union[str, Path]):
        self.path = Path(path)
        self._data = self.path.read_bytes()
        self.entries: List[BarEntry] = []
        self._entries_by_name: Dict[str, BarEntry] = {}
        self.checksum = 0
        self.reserved1 = 0
        self.reserved2 = 0
        self.directory_offset = 0
        self._parse()

    def _parse(self) -> None:
        if len(self._data) < BAR_HEADER_SIZE:
            raise ValueError("BAR header is truncated")

        _, self.checksum, entry_count, self.reserved1, self.directory_offset, self.reserved2 = (
            BAR_HEADER.unpack_from(self._data)
        )
        if self.directory_offset < BAR_HEADER_SIZE or self.directory_offset > len(self._data):
            raise ValueError("BAR directory offset is outside the archive")

        table_size = entry_count * 4
        if table_size > len(self._data) - self.directory_offset:
            raise ValueError("BAR directory offset table is truncated")

        offsets = [
            struct.unpack_from("<I", self._data, self.directory_offset + index * 4)[0]
            for index in range(entry_count)
        ]
        entry_data_start = self.directory_offset + table_size

        if any(left >= right for left, right in zip(offsets, offsets[1:])):
            raise ValueError("BAR directory offsets are not strictly increasing")

        for index, entry_offset in enumerate(offsets):
            start = entry_data_start + entry_offset
            end = (
                entry_data_start + offsets[index + 1]
                if index + 1 < entry_count
                else len(self._data)
            )
            if start < entry_data_start or start >= end or end > len(self._data):
                raise ValueError("BAR entry directory record is outside the archive")
            if end - start < ENTRY_HEADER_SIZE + 1:
                raise ValueError("BAR entry directory record is truncated")

            data_offset, compressed_size, decompressed_size, metadata = (
                ENTRY_HEADER.unpack_from(self._data, start)
            )
            if data_offset > len(self._data) or compressed_size > len(self._data) - data_offset:
                raise ValueError("BAR entry data is outside the archive")

            name_start = start + ENTRY_HEADER_SIZE
            name_end = self._data.find(b"\0", name_start, end)
            if name_end <= name_start:
                raise ValueError("BAR entry has an empty or unterminated filename")
            filename = self._data[name_start:name_end].decode("latin-1")

            is_compressed = (
                compressed_size >= len(L33T_MAGIC)
                and self._data[data_offset:data_offset + len(L33T_MAGIC)] == L33T_MAGIC
            )
            entry = BarEntry(
                filename=filename,
                data_offset=data_offset,
                compressed_size=compressed_size,
                decompressed_size=decompressed_size,
                metadata=metadata,
                is_compressed=is_compressed,
            )
            key = self._normalize_name(filename)
            if key in self._entries_by_name:
                raise ValueError("BAR archive contains duplicate filenames")
            self.entries.append(entry)
            self._entries_by_name[key] = entry

    @staticmethod
    def _normalize_name(filename: str) -> str:
        return filename.replace("\\", "/").casefold()

    def find(self, filename: str) -> Optional[BarEntry]:
        """Find an entry by path, ignoring case and slash direction."""
        return self._entries_by_name.get(self._normalize_name(filename))

    def extract(self, entry_or_filename: Union[BarEntry, str]) -> bytes:
        """Return the raw entry bytes, inflating an l33t/zlib payload if needed."""
        entry = (
            self.find(entry_or_filename)
            if isinstance(entry_or_filename, str)
            else entry_or_filename
        )
        if entry is None:
            raise KeyError("BAR entry not found: {}".format(entry_or_filename))
        if entry not in self.entries:
            raise ValueError("BAR entry does not belong to this archive")

        end = entry.data_offset + entry.compressed_size
        raw = self._data[entry.data_offset:end]
        if not entry.is_compressed:
            return raw
        if len(raw) <= L33T_HEADER_SIZE:
            raise ValueError("l33t entry is too small: {}".format(entry.filename))

        expected_size = struct.unpack_from("<I", raw, 4)[0]
        inflater = zlib.decompressobj()
        try:
            output = inflater.decompress(raw[L33T_HEADER_SIZE:], expected_size + 1)
        except zlib.error as error:
            raise ValueError(
                "Could not decompress {}: {}".format(entry.filename, error)
            ) from error

        if (
            len(output) != expected_size
            or not inflater.eof
            or inflater.unused_data
            or inflater.unconsumed_tail
        ):
            raise ValueError(
                "Decompressed size or stream boundary is invalid for {}".format(
                    entry.filename
                )
            )
        return output


def parse_bar(path: Union[str, Path]) -> BarArchive:
    """Parse a BAR file and return a reusable archive object."""
    return BarArchive(path)


def safe_output_path(output_dir: Union[str, Path], filename: str) -> Path:
    """Resolve an archive member path without allowing it to escape output_dir."""
    parts = filename.replace("\\", "/").split("/")
    if (
        not filename
        or filename.startswith(("/", "\\"))
        or PureWindowsPath(filename).drive
        or any(part in ("", ".", "..") for part in parts)
    ):
        raise ValueError("Unsafe archive path: {}".format(filename))

    root = Path(output_dir).resolve()
    destination = root.joinpath(*parts).resolve()
    try:
        destination.relative_to(root)
    except ValueError as error:
        raise ValueError("Unsafe archive path: {}".format(filename)) from error
    return destination


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path, help="BAR archive to inspect")
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--list", action="store_true", help="list archive entries")
    group.add_argument("--extract", metavar="ENTRY", help="extract one entry")
    parser.add_argument("--output", type=Path, help="output path for --extract")
    args = parser.parse_args()

    try:
        archive = parse_bar(args.archive)
        if args.extract:
            entry = archive.find(args.extract)
            if entry is None:
                raise KeyError("BAR entry not found: {}".format(args.extract))
            output_path = (
                args.output
                if args.output is not None
                else safe_output_path(Path.cwd(), entry.filename)
            )
            output_path.parent.mkdir(parents=True, exist_ok=True)
            output_path.write_bytes(archive.extract(entry))
            print("Extracted {} -> {}".format(entry.filename, output_path))
        elif args.list:
            for entry in archive.entries:
                encoding = "l33t" if entry.is_compressed else "raw"
                print(
                    "{}\t{}\t{}\t{}".format(
                        encoding,
                        entry.compressed_size,
                        entry.decompressed_size,
                        entry.filename,
                    )
                )
        else:
            compressed = sum(entry.is_compressed for entry in archive.entries)
            print("Archive: {}".format(args.archive))
            print("Entries: {}".format(len(archive.entries)))
            print("l33t entries: {}".format(compressed))
            print("Directory offset: 0x{:x}".format(archive.directory_offset))
    except (OSError, ValueError, KeyError) as error:
        print("parse_bar: {}".format(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
