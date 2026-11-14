#pragma once

#include <string>
#include <algorithm>
#include <cctype>
#include <vector>

#include "engine/logger.h"

#ifndef LOG_TAG
#define LOG_TAG "AoM-Engine"
#endif

#define LOGI(...) AOM_LOGI(LOG_TAG, __VA_ARGS__)
#define LOGW(...) AOM_LOGW(LOG_TAG, __VA_ARGS__)
#define LOGE(...) AOM_LOGE(LOG_TAG, __VA_ARGS__)
#define LOGD(...) AOM_LOGD(LOG_TAG, __VA_ARGS__)

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
