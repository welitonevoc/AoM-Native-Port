#!/usr/bin/env python3
"""
bar_tool.py — Age of Mythology BAR Archive Tool

Extracts and lists files from AoM .BAR archives.
This is a reference implementation that validates the format understanding
before implementing in C++.

Usage:
    python3 bar_tool.py list <archive.bar>
    python3 bar_tool.py extract <archive.bar> [output_dir]
    python3 bar_tool.py extract-file <archive.bar> <filename> [output_path]
    python3 bar_tool.py info <archive.bar>

BAR File Format:
    Header (28 bytes):
        u64 padding (0)
        u32 checksum
        u32 num_entries
        u32 reserved
        u32 directory_offset
        u32 reserved2
    
    Data section (byte 28 to directory_offset):
        File data, optionally compressed with "l33t" (zlib)
    
    Directory (at directory_offset):
        u32[num_entries] offset_table (relative to entry_data_start)
        Entry data (variable length per entry):
            u32 data_offset
            u32 compressed_size
            u32 decompressed_size
            u64 metadata
            u8  filename_length
            char[filename_length] filename (null-terminated)
"""

import struct
import zlib
import os
import sys
from pathlib import Path
from dataclasses import dataclass
from typing import List, Optional, Tuple

@dataclass
class BarEntry:
    """Represents a single file in a BAR archive."""
    filename: str
    data_offset: int
    compressed_size: int
    decompressed_size: int
    metadata: int
    is_compressed: bool

    @property
    def compression_ratio(self) -> float:
        if self.decompressed_size == 0:
            return 1.0
        return self.compressed_size / self.decompressed_size


class BarArchive:
    """Reader for AoM BAR archive files."""

    L33T_MAGIC = b'l33t'
    HEADER_SIZE = 28

    def __init__(self, path: str):
        self.path = path
        self.entries: List[BarEntry] = []
        self._data: bytes = b''
        self._parse()

    def _parse(self):
        """Parse the BAR archive header and directory."""
        with open(self.path, 'rb') as f:
            self._data = f.read()

        if len(self._data) < self.HEADER_SIZE:
            raise ValueError(f"File too small: {len(self._data)} bytes")

        # Parse header
        (padding, checksum, num_entries, reserved,
         dir_offset, reserved2) = struct.unpack_from('<QIIIII', self._data, 0)

        if dir_offset >= len(self._data):
            raise ValueError(f"Directory offset {dir_offset} beyond file size")

        # Read offset table
        offset_table = []
        for i in range(num_entries):
            pos = dir_offset + i * 4
            if pos + 4 > len(self._data):
                break
            offset_table.append(struct.unpack_from('<I', self._data, pos)[0])

        entry_data_start = dir_offset + num_entries * 4

        # Parse entries
        for i in range(num_entries):
            entry_offset = entry_data_start + offset_table[i]
            next_offset = (entry_data_start + offset_table[i + 1]
                          if i + 1 < num_entries
                          else len(self._data))

            if entry_offset >= len(self._data):
                continue

            entry_data = self._data[entry_offset:next_offset]
            if len(entry_data) < 21:
                continue

            # Parse fixed fields
            data_off = struct.unpack_from('<I', entry_data, 0)[0]
            comp_size = struct.unpack_from('<I', entry_data, 4)[0]
            decomp_size = struct.unpack_from('<I', entry_data, 8)[0]
            metadata = struct.unpack_from('<Q', entry_data, 12)[0]
            fname_len = entry_data[20]

            # Extract filename
            if 21 + fname_len <= len(entry_data):
                fname = entry_data[21:21 + fname_len].decode('ascii', errors='replace')
                fname = fname.rstrip('\x00')
            else:
                # Fallback: scan for ASCII string
                fname = self._find_filename(entry_data[20:])

            # Determine compression
            is_compressed = False
            actual_data_off = data_off
            if actual_data_off + 4 <= len(self._data):
                is_compressed = (self._data[actual_data_off:actual_data_off + 4]
                                == self.L33T_MAGIC)

            self.entries.append(BarEntry(
                filename=fname,
                data_offset=actual_data_off,
                compressed_size=comp_size,
                decompressed_size=decomp_size,
                metadata=metadata,
                is_compressed=is_compressed,
            ))

    def _find_filename(self, data: bytes) -> str:
        """Find a null-terminated ASCII string in raw data."""
        for i in range(len(data)):
            if data[i] == 0 and i > 0:
                start = i
                while start > 0 and 32 <= data[start - 1] < 127:
                    start -= 1
                if i - start > 2:
                    return data[start:i].decode('ascii', errors='replace')
        return '<unknown>'

    def find(self, filename: str) -> Optional[BarEntry]:
        """Find an entry by filename (case-insensitive)."""
        lower = filename.lower()
        for entry in self.entries:
            if entry.filename.lower() == lower:
                return entry
        return None

    def extract(self, entry: BarEntry) -> bytes:
        """Extract and decompress a file entry."""
        raw = self._data[entry.data_offset:
                         entry.data_offset + entry.compressed_size]

        if entry.is_compressed:
            # Skip l33t header (4 magic + 4 decomp_size)
            if len(raw) <= 8:
                raise ValueError(f"l33t entry too small: {len(raw)}")
            compressed = raw[8:]
            try:
                return zlib.decompress(compressed)
            except zlib.error as e:
                raise ValueError(f"Decompression failed for {entry.filename}: {e}")
        else:
            return raw

    def extract_all(self, output_dir: str) -> int:
        """Extract all files to a directory."""
        os.makedirs(output_dir, exist_ok=True)
        count = 0
        for entry in self.entries:
            try:
                data = self.extract(entry)
                out_path = os.path.join(output_dir, entry.filename)
                os.makedirs(os.path.dirname(out_path) or '.', exist_ok=True)
                with open(out_path, 'wb') as f:
                    f.write(data)
                count += 1
            except Exception as e:
                print(f"  ERROR extracting {entry.filename}: {e}", file=sys.stderr)
        return count


def cmd_info(archive_path: str):
    """Print archive information."""
    bar = BarArchive(archive_path)
    total_compressed = sum(e.compressed_size for e in bar.entries)
    total_decompressed = sum(e.decompressed_size for e in bar.entries)
    compressed_count = sum(1 for e in bar.entries if e.is_compressed)

    print(f"Archive: {archive_path}")
    print(f"  File size:    {os.path.getsize(archive_path):,} bytes")
    print(f"  Entries:      {len(bar.entries)}")
    print(f"  Compressed:   {compressed_count} / {len(bar.entries)}")
    print(f"  Total size:   {total_compressed:,} bytes (compressed)")
    print(f"                {total_decompressed:,} bytes (decompressed)")
    if total_decompressed > 0:
        ratio = total_compressed / total_decompressed
        print(f"  Ratio:        {ratio:.1%}")

    # File type breakdown
    extensions = {}
    for entry in bar.entries:
        ext = Path(entry.filename).suffix.lower() or '(none)'
        extensions[ext] = extensions.get(ext, 0) + 1
    print(f"\n  File types:")
    for ext, count in sorted(extensions.items(), key=lambda x: -x[1]):
        print(f"    {ext:12s}  {count}")


def cmd_list(archive_path: str):
    """List files in archive."""
    bar = BarArchive(archive_path)
    print(f"{'Filename':<50s} {'Comp.':>10s} {'Decomp.':>10s} {'Ratio':>7s} {'Compressed':>10s}")
    print("-" * 92)
    for entry in sorted(bar.entries, key=lambda e: e.filename):
        ratio = f"{entry.compression_ratio:.0%}" if entry.is_compressed else "  raw"
        comp = "l33t" if entry.is_compressed else "none"
        print(f"{entry.filename:<50s} {entry.compressed_size:>10,d} "
              f"{entry.decompressed_size:>10,d} {ratio:>7s} {comp:>10s}")
    print(f"\nTotal: {len(bar.entries)} files")


def cmd_extract(archive_path: str, output_dir: str = None):
    """Extract all files."""
    if output_dir is None:
        output_dir = Path(archive_path).stem + '_extracted'
    bar = BarArchive(archive_path)
    print(f"Extracting {len(bar.entries)} files to {output_dir}...")
    count = bar.extract_all(output_dir)
    print(f"Extracted {count}/{len(bar.entries)} files")


def cmd_extract_file(archive_path: str, filename: str, output_path: str = None):
    """Extract a single file."""
    bar = BarArchive(archive_path)
    entry = bar.find(filename)
    if not entry:
        print(f"File not found: {filename}", file=sys.stderr)
        sys.exit(1)
    data = bar.extract(entry)
    if output_path is None:
        output_path = entry.filename
    os.makedirs(os.path.dirname(output_path) or '.', exist_ok=True)
    with open(output_path, 'wb') as f:
        f.write(data)
    print(f"Extracted {entry.filename} -> {output_path} ({len(data):,} bytes)")


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)

    command = sys.argv[1]
    archive = sys.argv[2]

    if command == 'info':
        cmd_info(archive)
    elif command == 'list':
        cmd_list(archive)
    elif command == 'extract':
        output = sys.argv[3] if len(sys.argv) > 3 else None
        cmd_extract(archive, output)
    elif command == 'extract-file':
        if len(sys.argv) < 4:
            print("Usage: bar_tool.py extract-file <archive.bar> <filename> [output]")
            sys.exit(1)
        fname = sys.argv[3]
        output = sys.argv[4] if len(sys.argv) > 4 else None
        cmd_extract_file(archive, fname, output)
    else:
        print(f"Unknown command: {command}")
        sys.exit(1)


if __name__ == '__main__':
    main()
