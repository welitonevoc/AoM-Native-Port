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
    : mounts_(std::move(other.mounts_)),
      entries_(std::move(other.entries_)),
      name_index_(std::move(other.name_index_)) {
    other.mounts_.clear();
    other.entries_.clear();
    other.name_index_.clear();
}

BarArchive& BarArchive::operator=(BarArchive&& other) noexcept {
    if (this != &other) {
        close();
        mounts_ = std::move(other.mounts_);
        entries_ = std::move(other.entries_);
        name_index_ = std::move(other.name_index_);

        other.mounts_.clear();
        other.entries_.clear();
        other.name_index_.clear();
    }
    return *this;
}

bool BarArchive::open(const std::string& path) {
    close();
    return mount(path);
}

bool BarArchive::mount(const std::string& path) {
    Mount new_mount;
    std::vector<BarEntry> parsed;
    if (!parse_file(path, new_mount, parsed)) {
        release_mount(new_mount);
        return false;
    }

    const uint32_t mount_index = static_cast<uint32_t>(mounts_.size());
    size_t overridden = 0;

    for (BarEntry& entry : parsed) {
        entry.mount_index = mount_index;
        const std::string key = normalize(entry.filename);

        const auto it = name_index_.find(key);
        if (it != name_index_.end()) {
            // Later mounts win: replace the entry declared by an older mount.
            entries_[it->second] = std::move(entry);
            ++overridden;
        } else {
            name_index_[key] = entries_.size();
            entries_.push_back(std::move(entry));
        }
    }

    mounts_.push_back(new_mount);

    LOGI("BAR: mounted '%s' as overlay #%u — %zu entries (%zu override existing), %zu total\n",
         path.c_str(), mount_index, parsed.size(), overridden, entries_.size());
    return true;
}

bool BarArchive::open_pair(const std::string& base_path, const std::string& expansion_path) {
    close();

    bool ok = false;
    if (!base_path.empty()) {
        ok = open(base_path);
    }
    if (!expansion_path.empty()) {
        if (mount(expansion_path)) {
            ok = true;
        }
    }
    return ok;
}

void BarArchive::close() {
    for (Mount& mount : mounts_) {
        release_mount(mount);
    }
    mounts_.clear();
    entries_.clear();
    name_index_.clear();
}

std::vector<std::string> BarArchive::mount_paths() const {
    std::vector<std::string> paths;
    paths.reserve(mounts_.size());
    for (const Mount& mount : mounts_) {
        paths.push_back(mount.path);
    }
    return paths;
}

const BarEntry* BarArchive::find(const std::string& filename) const {
    const auto it = name_index_.find(normalize(filename));
    if (it != name_index_.end()) {
        return &entries_[it->second];
    }
    return nullptr;
}

std::vector<uint8_t> BarArchive::extract(const BarEntry& entry) const {
    if (entry.mount_index >= mounts_.size()) {
        LOGE("BAR: invalid mount index for '%s'\n", entry.filename.c_str());
        return {};
    }

    const Mount& mount = mounts_[entry.mount_index];
    if (!mount.data) {
        LOGE("BAR: Archive not open\n");
        return {};
    }

    if (entry.data_offset + entry.compressed_size > mount.size) {
        LOGE("BAR: Entry '%s' data exceeds file bounds\n", entry.filename.c_str());
        return {};
    }

    const uint8_t* data_ptr = mount.data + entry.data_offset;

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

std::string BarArchive::normalize(const std::string& filename) {
    std::string out = to_lower(filename);
    std::replace(out.begin(), out.end(), '\\', '/');
    return out;
}

void BarArchive::release_mount(Mount& mount) {
    if (mount.data && mount.data != MAP_FAILED) {
        ::munmap(const_cast<uint8_t*>(mount.data), mount.size);
        mount.data = nullptr;
    }
    if (mount.fd >= 0) {
        ::close(mount.fd);
        mount.fd = -1;
    }
    mount.size = 0;
}

bool BarArchive::parse_file(const std::string& path, Mount& mount,
                            std::vector<BarEntry>& out_entries) {
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

    mount.path = path;
    mount.data = static_cast<const uint8_t*>(mapped);
    mount.size = filesize;
    mount.fd = fd;

#if defined(MADV_RANDOM)
    ::madvise(mapped, filesize, MADV_RANDOM);
#endif

    // Parse header
    BarHeader header{};
    std::memcpy(&header.padding, mount.data, 8);
    std::memcpy(&header.checksum, mount.data + 8, 4);
    std::memcpy(&header.num_entries, mount.data + 12, 4);
    std::memcpy(&header.reserved1, mount.data + 16, 4);
    std::memcpy(&header.dir_offset, mount.data + 20, 4);
    std::memcpy(&header.reserved2, mount.data + 24, 4);

    LOGI("BAR (mmap): %s — %u entries, dir at 0x%x, size %zu MB\n",
         path.c_str(), header.num_entries, header.dir_offset, filesize / (1024 * 1024));

    if (header.dir_offset >= filesize) {
        LOGE("BAR directory offset 0x%x beyond file size %zu\n",
             header.dir_offset, filesize);
        release_mount(mount);
        return false;
    }

    const uint32_t num_entries = header.num_entries;

    // Read offset table (num_entries * uint32)
    const size_t offset_table_size = static_cast<size_t>(num_entries) * sizeof(uint32_t);
    if (header.dir_offset + offset_table_size > filesize) {
        LOGE("BAR offset table exceeds file bounds\n");
        release_mount(mount);
        return false;
    }

#if defined(MADV_WILLNEED)
    const size_t dir_preload_size = std::min(filesize - header.dir_offset, static_cast<size_t>(2 * 1024 * 1024));
    ::madvise(const_cast<uint8_t*>(mount.data + header.dir_offset), dir_preload_size, MADV_WILLNEED);
#endif

    std::vector<uint32_t> offset_table(num_entries);
    std::memcpy(offset_table.data(),
                mount.data + header.dir_offset,
                offset_table_size);

    const size_t entry_data_base = header.dir_offset + offset_table_size;

    // Parse entries
    out_entries.reserve(num_entries);
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

        const uint8_t* entry_ptr = mount.data + entry_offset;
        const size_t entry_len = next_offset - entry_offset;

        if (entry_len < 22) { // Minimum: fixed fields + 2-byte filename
            LOGE("BAR entry %u: too short (%zu bytes)\n", i, entry_len);
            continue;
        }

        BarEntry entry{};

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
                mount.data + entry.data_offset, L33T_MAGIC, 4) == 0);
        } else {
            entry.is_compressed = (entry.compressed_size != entry.decompressed_size);
        }

        out_entries.push_back(std::move(entry));
    }

    LOGI("BAR (mmap): Parsed %zu entries from %s (RAM saved: ~%zu MB)\n",
         out_entries.size(), path.c_str(), filesize / (1024 * 1024));
    return true;
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
