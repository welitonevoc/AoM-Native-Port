#pragma once

#include <string>
#include <algorithm>
#include <cctype>
#include <vector>

#define LOG_TAG "AoM-Engine"
#ifdef __ANDROID__
#include <android/log.h>
#include <cstdio>
#include <cstdarg>

namespace aom {
inline void aom_file_log_write(int prio, const char* tag, const char* fmt, ...) {
    va_list ap, ap_file;
    va_start(ap, fmt);
    va_copy(ap_file, ap);
    __android_log_vprint(prio, tag, fmt, ap);
    va_end(ap);

    static FILE* s_logfile = nullptr;
    static bool s_tried_open = false;
    if (!s_tried_open) {
        s_tried_open = true;
        const char* paths[] = {
            "/sdcard/log.txt",
            "/storage/emulated/0/log.txt",
            "/sdcard/AoM/log.txt",
            "/storage/emulated/0/AoM/log.txt",
            "/sdcard/aom_log.txt",
            "/storage/emulated/0/aom_log.txt"
        };
        for (const char* p : paths) {
            s_logfile = std::fopen(p, "a");
            if (s_logfile) {
                std::fprintf(s_logfile, "=== AoM Session Log Started ===\n");
                std::fflush(s_logfile);
                break;
            }
        }
    }
    if (s_logfile) {
        std::fprintf(s_logfile, "[%s] ", tag);
        std::vfprintf(s_logfile, fmt, ap_file);
        std::fprintf(s_logfile, "\n");
        std::fflush(s_logfile);
    }
    va_end(ap_file);
}
} // namespace aom

#define LOGI(...) ::aom::aom_file_log_write(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) ::aom::aom_file_log_write(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) ::aom::aom_file_log_write(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#include <cstdio>
#define LOGI(...) std::printf(__VA_ARGS__)
#define LOGW(...) std::printf(__VA_ARGS__)
#define LOGE(...) std::fprintf(stderr, __VA_ARGS__)
#endif

namespace aom {

inline std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

inline bool ends_with(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

inline std::string texture_key(std::string s) {
    const size_t slash = s.find_last_of("\\/");
    if (slash != std::string::npos) s.erase(0, slash + 1);
    const size_t dot = s.rfind('.');
    if (dot != std::string::npos && s.size() - dot <= 5) s.erase(dot);
    std::string k;
    for (unsigned char c : s) {
        if (std::isalnum(c)) k.push_back(static_cast<char>(std::tolower(c)));
    }
    return k;
}

} // namespace aom
