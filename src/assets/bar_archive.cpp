/// @file bar_archive.cpp
/// @brief Implementation of BAR archive reader for Age of Mythology

#include "assets/bar_archive.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>

#ifdef __ANDROID__
#include <android/log.h>
#define LOG_TAG "AoM-BAR"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#include <cstdio>
#define LOGI(...) printf(__VA_ARGS__)
#define LOGE(...) fprintf(stderr, __VA_ARGS__)
#endif

// zlib for l33t decompression
#include <zlib.h>

namespace aom {

namespace {

// BAR header constants
constexpr size_t BAR_HEADER_SIZE = 28;
constexpr char L33T_MAGIC[] = "l33t";
constexpr size_t L33T_HEADER_SIZE = 8; // 4 magic + 4 decompressed_size

// BAR directory entry structure (packed):
//   u32 data_offset
//   u32 compressed_size
//   u32 decompressed_size
//   u64 metadata (timestamp)
//   char[] filename (null-terminated, no length prefix)

struct BarHeader {
    uint64_t padding;        // Always 0
    uint32_t checksum;       // File checksum
    uint32_t num_entries;    // Number of files
    uint32_t reserved1;      // Total data size or similar
    uint32_t dir_offset;     // Offset to directory section
    uint32_t reserved2;      // Usually 8
};

/// Convert string to lowercase for case-insensitive lookup
std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

} // anonymous namespace

BarArchive::BarArchive() = default;

BarArchive::BarArchive(const std::string& path) {
    open(path);
}

BarArchive::~BarArchive() {
    close();
}

BarArchive::BarArchive(BarArchive&& other) noexcept
    : archive_path_(std::move(other.archive_path_)),
      mmap_data_(other.mmap_data_),
      file_size_(other.file_size_),
      fd_(other.fd_),
      entries_(std::move(other.entries_)),
      name_index_(std::move(other.name_index_)) {
    other.mmap_data_ = nullptr;
    other.file_size_ = 0;
    other.fd_ = -1;
}

BarArchive& BarArchive::operator=(BarArchive&& other) noexcept {
    if (this != &other) {
        close();
        archive_path_ = std::move(other.archive_path_);
        mmap_data_ = other.mmap_data_;
        file_size_ = other.file_size_;
        fd_ = other.fd_;
        entries_ = std::move(other.entries_);
        name_index_ = std::move(other.name_index_);

        other.mmap_data_ = nullptr;
        other.file_size_ = 0;
        other.fd_ = -1;
    }
    return *this;
}

bool BarArchive::open(const std::string& path) {
    close();

    int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        LOGE("Failed to open BAR file: %s (errno: %d)\n", path.c_str(), errno);
        return false;
    }

    struct stat st{};
    if (::fstat(fd, &st) != 0) {
        LOGE("Failed to fstat BAR file: %s\n", path.c_str());
        ::close(fd);
        return false;
    }

    const size_t filesize = static_cast<size_t>(st.st_size);
    if (filesize < BAR_HEADER_SIZE) {
        LOGE("BAR file too small: %zu bytes\n", filesize);
        ::close(fd);
        return false;
    }

    void* mapped = ::mmap(nullptr, filesize, PROT_READ, MAP_SHARED, fd, 0);
    if (mapped == MAP_FAILED) {
        LOGE("Failed to mmap BAR file %s (%zu bytes, errno: %d)\n", path.c_str(), filesize, errno);
        ::close(fd);
        return false;
    }

    fd_ = fd;
    file_size_ = filesize;
    mmap_data_ = static_cast<const uint8_t*>(mapped);
    archive_path_ = path;

#if defined(MADV_RANDOM)
    ::madvise(mapped, filesize, MADV_RANDOM);
#endif

    // Parse header
    BarHeader header{};
    std::memcpy(&header.padding, mmap_data_, 8);
    std::memcpy(&header.checksum, mmap_data_ + 8, 4);
    std::memcpy(&header.num_entries, mmap_data_ + 12, 4);
    std::memcpy(&header.reserved1, mmap_data_ + 16, 4);
    std::memcpy(&header.dir_offset, mmap_data_ + 20, 4);
    std::memcpy(&header.reserved2, mmap_data_ + 24, 4);

    LOGI("BAR (mmap): %s — %u entries, dir at 0x%x, size %zu MB\n",
         path.c_str(), header.num_entries, header.dir_offset, filesize / (1024 * 1024));

    if (header.dir_offset >= filesize) {
        LOGE("BAR directory offset 0x%x beyond file size %zu\n",
             header.dir_offset, filesize);
        close();
        return false;
    }

    const uint32_t num_entries = header.num_entries;

    // Read offset table (num_entries * uint32)
    const size_t offset_table_size = num_entries * sizeof(uint32_t);
    if (header.dir_offset + offset_table_size > filesize) {
        LOGE("BAR offset table exceeds file bounds\n");
        close();
        return false;
    }

#if defined(MADV_WILLNEED)
    const size_t dir_preload_size = std::min(filesize - header.dir_offset, static_cast<size_t>(2 * 1024 * 1024));
    ::madvise(const_cast<uint8_t*>(mmap_data_ + header.dir_offset), dir_preload_size, MADV_WILLNEED);
#endif

    std::vector<uint32_t> offset_table(num_entries);
    std::memcpy(offset_table.data(),
                mmap_data_ + header.dir_offset,
                offset_table_size);

    const size_t entry_data_base = header.dir_offset + offset_table_size;

    // Parse entries
    entries_.reserve(num_entries);
    for (uint32_t i = 0; i < num_entries; ++i) {
        const size_t entry_offset = entry_data_base + offset_table[i];

        // Calculate entry length
        const size_t next_offset = (i + 1 < num_entries)
            ? entry_data_base + offset_table[i + 1]
            : filesize;

        if (entry_offset >= filesize || next_offset > filesize) {
            LOGE("BAR entry %u: invalid offset\n", i);
            continue;
        }

        const uint8_t* entry_ptr = mmap_data_ + entry_offset;
        const size_t entry_len = next_offset - entry_offset;

        if (entry_len < 22) { // Minimum: fixed fields + 2-byte filename
            LOGE("BAR entry %u: too short (%zu bytes)\n", i, entry_len);
            continue;
        }

        BarEntry entry;

        // Parse fixed fields
        std::memcpy(&entry.data_offset, entry_ptr + 0, 4);
        std::memcpy(&entry.compressed_size, entry_ptr + 4, 4);
        std::memcpy(&entry.decompressed_size, entry_ptr + 8, 4);
        std::memcpy(&entry.metadata, entry_ptr + 12, 8);

        // Data offsets in the directory are absolute file offsets.
        if (entry.data_offset > filesize ||
            entry.compressed_size > filesize - entry.data_offset) {
            LOGE("BAR entry %u: data out of bounds\n", i);
            continue;
        }

        // Filename starts at offset 20 and is NUL-terminated.
        const char* name_ptr = reinterpret_cast<const char*>(entry_ptr + 20);
        const size_t name_max = entry_len - 20;
        size_t name_len = 0;
        while (name_len < name_max && name_ptr[name_len] != '\0') {
            ++name_len;
        }
        if (name_len == 0 || name_len == name_max) {
            LOGE("BAR entry %u: invalid filename\n", i);
            continue;
        }
        entry.filename.assign(name_ptr, name_len);

        // Determine if compressed
        // Check if the data at data_offset starts with "l33t"
        if (entry.data_offset + 4 <= filesize) {
            entry.is_compressed = (std::memcmp(
                mmap_data_ + entry.data_offset, L33T_MAGIC, 4) == 0);
        } else {
            entry.is_compressed = (entry.compressed_size != entry.decompressed_size);
        }

        // Index by lowercase normalized name (forward slashes)
        std::string norm_name = to_lower(entry.filename);
        std::replace(norm_name.begin(), norm_name.end(), '\\', '/');
        name_index_[norm_name] = entries_.size();

        entries_.push_back(std::move(entry));
    }

    LOGI("BAR (mmap): Parsed %zu entries from %s (RAM saved: ~%zu MB)\n",
         entries_.size(), path.c_str(), filesize / (1024 * 1024));
    return true;
}

void BarArchive::close() {
    if (mmap_data_ && mmap_data_ != MAP_FAILED) {
        ::munmap(const_cast<uint8_t*>(mmap_data_), file_size_);
        mmap_data_ = nullptr;
    }
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    file_size_ = 0;
    entries_.clear();
    name_index_.clear();
    archive_path_.clear();
}

const BarEntry* BarArchive::find(const std::string& filename) const {
    std::string query = to_lower(filename);
    std::replace(query.begin(), query.end(), '\\', '/');
    const auto it = name_index_.find(query);
    if (it != name_index_.end()) {
        return &entries_[it->second];
    }
    return nullptr;
}

std::vector<uint8_t> BarArchive::extract(const BarEntry& entry) const {
    if (!mmap_data_) {
        LOGE("BAR: Archive not open\n");
        return {};
    }

    if (entry.data_offset + entry.compressed_size > file_size_) {
        LOGE("BAR: Entry '%s' data exceeds file bounds\n", entry.filename.c_str());
        return {};
    }

    const uint8_t* data_ptr = mmap_data_ + entry.data_offset;

    if (entry.is_compressed) {
        // l33t compressed: skip 8-byte l33t header (4 magic + 4 decompressed_size)
        if (entry.compressed_size <= L33T_HEADER_SIZE) {
            LOGE("BAR: l33t entry '%s' too small\n", entry.filename.c_str());
            return {};
        }

        // Verify l33t magic
        if (std::memcmp(data_ptr, L33T_MAGIC, 4) != 0) {
            LOGE("BAR: Expected l33t magic for '%s'\n", entry.filename.c_str());
            return {};
        }

        uint32_t decomp_size;
        std::memcpy(&decomp_size, data_ptr + 4, 4);

        return decompress_l33t(
            data_ptr + L33T_HEADER_SIZE,
            entry.compressed_size - L33T_HEADER_SIZE,
            decomp_size);
    } else {
        // Uncompressed — just copy
        return std::vector<uint8_t>(data_ptr, data_ptr + entry.compressed_size);
    }
}

std::vector<uint8_t> BarArchive::extract(const std::string& filename) const {
    const auto* entry = find(filename);
    if (!entry) {
        LOGE("BAR: File '%s' not found\n", filename.c_str());
        return {};
    }
    return extract(*entry);
}

void BarArchive::for_each(const std::function<void(const BarEntry&)>& callback) const {
    for (const auto& entry : entries_) {
        callback(entry);
    }
}

size_t BarArchive::extract_all(const std::string& output_dir) const {
    namespace fs = std::filesystem;

    size_t count = 0;
    for (const auto& entry : entries_) {
        const fs::path output_path = fs::path(output_dir) / entry.filename;

        // Create parent directories
        fs::create_directories(output_path.parent_path());

        auto data = extract(entry);
        if (data.empty() && entry.decompressed_size > 0) {
            LOGE("BAR: Failed to extract '%s'\n", entry.filename.c_str());
            continue;
        }

        std::ofstream out(output_path, std::ios::binary);
        if (!out.is_open()) {
            LOGE("BAR: Failed to create '%s'\n", output_path.c_str());
            continue;
        }

        out.write(reinterpret_cast<const char*>(data.data()), data.size());
        ++count;
    }

    LOGI("BAR: Extracted %zu/%zu files to %s\n",
         count, entries_.size(), output_dir.c_str());
    return count;
}

std::vector<uint8_t> BarArchive::decompress_l33t(
    const uint8_t* data, size_t size, uint32_t decompressed_size) {

    std::vector<uint8_t> output(decompressed_size);

    z_stream strm{};
    strm.next_in = const_cast<Bytef*>(data);
    strm.avail_in = static_cast<uInt>(size);
    strm.next_out = output.data();
    strm.avail_out = decompressed_size;

    int ret = inflateInit(&strm);
    if (ret != Z_OK) {
        LOGE("BAR: zlib inflateInit failed: %d\n", ret);
        return {};
    }

    ret = inflate(&strm, Z_FINISH);
    inflateEnd(&strm);

    if (ret != Z_STREAM_END) {
        LOGE("BAR: zlib inflate failed: %d (expected %u, got %lu)\n",
             ret, decompressed_size, strm.total_out);
        // Partial data may still be useful
        output.resize(strm.total_out);
    }

    return output;
}

} // namespace aom
