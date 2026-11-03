#pragma once

/// @file bar_archive.h
/// @brief Parser for Age of Mythology .BAR archive files
///
/// BAR file format (reverse-engineered from AoM Extended Edition):
///
/// Header (28 bytes):
///   [0-7]   u64  padding (always 0)
///   [8-11]  u32  checksum
///   [12-15] u32  num_entries
///   [16-19] u32  reserved / total_data_size
///   [20-23] u32  directory_offset (offset to the directory section)
///   [24-27] u32  reserved (usually 8)
///
/// Data section (from byte 28 to directory_offset):
///   Contains file data, optionally compressed.
///   Compressed entries begin with "l33t" magic (4 bytes), followed by:
///     [0-3]  char[4] "l33t"
///     [4-7]  u32     decompressed_size
///     [8..]  zlib-compressed data
///   Uncompressed entries (e.g., textures with "RTS3" magic) are stored raw.
///   Entry data offsets are absolute file offsets.
///
/// Directory section (at directory_offset):
///   Offset table:  num_entries * u32 (relative offsets into entry data)
///   Entry data:    variable-length records, each containing:
///     [0-3]   u32  data_offset      (offset into the data section)
///     [4-7]   u32  compressed_size   (size in the BAR file)
///     [8-11]  u32  decompressed_size
///     [12-19] u64  timestamp/metadata
///     [20..]  char[] null-terminated ASCII filename (no length prefix)

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <optional>
#include <mutex>
#include <cstdio>

namespace aom {

/// Represents a single file entry in a BAR archive
struct BarEntry {
    std::string filename;
    uint32_t data_offset;       ///< Offset of data within the BAR file (from start)
    uint32_t compressed_size;   ///< Size of data as stored (may be compressed)
    uint32_t decompressed_size; ///< Original size of data
    uint64_t metadata;          ///< Timestamp or other metadata
    bool is_compressed;         ///< True if data is l33t-compressed
};

/// BAR archive reader — reads and extracts files from .BAR archives
class BarArchive {
public:
    BarArchive();
    explicit BarArchive(const std::string& path);
    ~BarArchive();

    // Non-copyable, movable
    BarArchive(const BarArchive&) = delete;
    BarArchive& operator=(const BarArchive&) = delete;
    BarArchive(BarArchive&& other) noexcept;
    BarArchive& operator=(BarArchive&& other) noexcept;

    /// Open and parse a BAR archive file
    /// @param path Path to the .bar file
    /// @return true on success
    bool open(const std::string& path);

    /// Close the archive and free resources
    void close();

    /// Check if the archive is open
    [[nodiscard]] bool is_open() const { return mmap_data_ != nullptr && !entries_.empty(); }

    /// Get number of files in the archive
    [[nodiscard]] size_t file_count() const { return entries_.size(); }

    /// Get all file entries
    [[nodiscard]] const std::vector<BarEntry>& entries() const { return entries_; }

    /// Find a file entry by name (case-insensitive)
    /// @param filename The filename to search for
    /// @return Pointer to entry or nullptr if not found
    [[nodiscard]] const BarEntry* find(const std::string& filename) const;

    /// Extract a file's data (decompressed if necessary)
    /// @param entry The entry to extract
    /// @return Decompressed file data, or empty vector on error
    [[nodiscard]] std::vector<uint8_t> extract(const BarEntry& entry) const;

    /// Extract a file by name
    /// @param filename The filename to extract
    /// @return Decompressed file data, or empty vector if not found
    [[nodiscard]] std::vector<uint8_t> extract(const std::string& filename) const;

    /// Iterate over all entries
    void for_each(const std::function<void(const BarEntry&)>& callback) const;

    /// Extract all files to a directory
    /// @param output_dir Directory to extract to
    /// @return Number of files extracted
    size_t extract_all(const std::string& output_dir) const;

private:
    /// Decompress l33t-compressed data
    static std::vector<uint8_t> decompress_l33t(const uint8_t* data, size_t size,
                                                 uint32_t decompressed_size);

    std::string archive_path_;
    const uint8_t* mmap_data_ = nullptr;
    size_t file_size_ = 0;
    int fd_ = -1;
    std::vector<BarEntry> entries_;
    std::unordered_map<std::string, size_t> name_index_; ///< lowercase name → entry index
};

} // namespace aom
