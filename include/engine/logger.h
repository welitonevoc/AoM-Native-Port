#pragma once

#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <mutex>
#include <string>
#include <sys/time.h>
#include <unistd.h>

#ifdef __ANDROID__
#include <android/log.h>
#endif

namespace aom {

inline const char* log_prio_char(int prio) {
#ifdef __ANDROID__
    switch (prio) {
        case ANDROID_LOG_VERBOSE: return "V";
        case ANDROID_LOG_DEBUG:   return "D";
        case ANDROID_LOG_INFO:    return "I";
        case ANDROID_LOG_WARN:    return "W";
        case ANDROID_LOG_ERROR:   return "E";
        case ANDROID_LOG_FATAL:   return "F";
        default:                  return "?";
    }
#else
    (void)prio;
    return "I";
#endif
}

inline void aom_file_log_write(int prio, const char* tag, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

#ifdef __ANDROID__
    va_list ap_logcat;
    va_copy(ap_logcat, ap);
    __android_log_vprint(prio, tag, fmt, ap_logcat);
    va_end(ap_logcat);
#endif

    static std::mutex s_log_mutex;
    std::lock_guard<std::mutex> lock(s_log_mutex);

    static FILE* s_logfile = nullptr;
    static int s_retry_count = 0;
    static bool s_session_started = false;

    if (!s_logfile && (++s_retry_count % 10 == 1)) {
        const char* paths[] = {
            "/sdcard/log.txt",
            "/storage/emulated/0/log.txt",
            "/storage/emulated/0/Documents/log.txt",
            "/sdcard/Documents/log.txt",
            "/storage/emulated/0/Download/log.txt",
            "/sdcard/Download/log.txt",
            "/sdcard/AoM/log.txt",
            "/storage/emulated/0/AoM/log.txt",
            "/sdcard/aom_log.txt",
            "/storage/emulated/0/aom_log.txt"
        };
        for (const char* p : paths) {
            FILE* test_f = std::fopen(p, "r");
            bool truncate = false;
            if (test_f) {
                std::fseek(test_f, 0, SEEK_END);
                long sz = std::ftell(test_f);
                std::fclose(test_f);
                if (sz > 1024 * 1024) truncate = true;
            }
            s_logfile = std::fopen(p, truncate ? "w" : "a");
            if (s_logfile) break;
        }
    }

    if (s_logfile) {
        struct timeval tv;
        gettimeofday(&tv, nullptr);
        struct tm tm_info;
        time_t sec = tv.tv_sec;
        localtime_r(&sec, &tm_info);

        if (!s_session_started) {
            s_session_started = true;
            char date_str[64];
            std::strftime(date_str, sizeof(date_str), "%Y-%m-%d %H:%M:%S", &tm_info);
            std::fprintf(s_logfile, "\n======================================================================\n");
            std::fprintf(s_logfile, "=== AoM Session Log Started: %s (PID: %d) ===\n", date_str, getpid());
            std::fprintf(s_logfile, "======================================================================\n");
        }

        char time_str[32];
        std::snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d.%03d",
                      tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec,
                      static_cast<int>(tv.tv_usec / 1000));

        size_t fmt_len = std::strlen(fmt);
        bool has_nl = (fmt_len > 0 && fmt[fmt_len - 1] == '\n');
        std::fprintf(s_logfile, "[%s] [%s] [%s] ", time_str, log_prio_char(prio), tag);
        std::vfprintf(s_logfile, fmt, ap);
        if (!has_nl) {
            std::fprintf(s_logfile, "\n");
        }
        std::fflush(s_logfile);
    }
#ifndef __ANDROID__
    else {
        std::printf("[%s] ", tag);
        std::vprintf(fmt, ap);
        std::printf("\n");
    }
#endif

    va_end(ap);
}

} // namespace aom

#ifdef __ANDROID__
#define AOM_LOGI(tag, ...) ::aom::aom_file_log_write(ANDROID_LOG_INFO, tag, __VA_ARGS__)
#define AOM_LOGW(tag, ...) ::aom::aom_file_log_write(ANDROID_LOG_WARN, tag, __VA_ARGS__)
#define AOM_LOGE(tag, ...) ::aom::aom_file_log_write(ANDROID_LOG_ERROR, tag, __VA_ARGS__)
#define AOM_LOGD(tag, ...) ::aom::aom_file_log_write(ANDROID_LOG_DEBUG, tag, __VA_ARGS__)
#else
#define AOM_LOGI(tag, ...) ::aom::aom_file_log_write(0, tag, __VA_ARGS__)
#define AOM_LOGW(tag, ...) ::aom::aom_file_log_write(1, tag, __VA_ARGS__)
#define AOM_LOGE(tag, ...) ::aom::aom_file_log_write(2, tag, __VA_ARGS__)
#define AOM_LOGD(tag, ...) ::aom::aom_file_log_write(3, tag, __VA_ARGS__)
#endif
