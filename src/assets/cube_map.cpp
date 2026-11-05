#include "assets/cube_map.h"
#include "assets/bar_archive.h"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <fstream>

#ifdef __ANDROID__
#include <android/log.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  "AoM-CubeMap", __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  "AoM-CubeMap", __VA_ARGS__)
#else
#define LOGI(...) std::printf(__VA_ARGS__)
#define LOGW(...) std::printf(__VA_ARGS__)
#endif

namespace aom {

namespace {

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

} // anonymous namespace

CubeMapDef parse_cub_file(const std::string& cub_content) {
    CubeMapDef def;
    std::istringstream stream(cub_content);
    std::string line;

    enum class TargetFace { NONE, UP, DOWN, FRONT, BACK, LEFT, RIGHT };
    TargetFace current_face = TargetFace::NONE;

    while (std::getline(stream, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty()) continue;

        if (trimmed.rfind("//", 0) == 0) {
            std::string comment = to_lower(trim(trimmed.substr(2)));
            if (comment.find("up") != std::string::npos) {
                current_face = TargetFace::UP;
            } else if (comment.find("down") != std::string::npos) {
                current_face = TargetFace::DOWN;
            } else if (comment.find("front") != std::string::npos) {
                current_face = TargetFace::FRONT;
            } else if (comment.find("back") != std::string::npos) {
                current_face = TargetFace::BACK;
            } else if (comment.find("left") != std::string::npos) {
                current_face = TargetFace::LEFT;
            } else if (comment.find("right") != std::string::npos) {
                current_face = TargetFace::RIGHT;
            }
            continue;
        }

        switch (current_face) {
            case TargetFace::UP:    def.up = trimmed; break;
            case TargetFace::DOWN:  def.down = trimmed; break;
            case TargetFace::FRONT: def.front = trimmed; break;
            case TargetFace::BACK:  def.back = trimmed; break;
            case TargetFace::LEFT:  def.left = trimmed; break;
            case TargetFace::RIGHT: def.right = trimmed; break;
            default: break;
        }
        current_face = TargetFace::NONE;
    }

    return def;
}

CubeMapDef load_cub_definition(const std::string& filename, BarArchive* archive) {
    if (archive) {
        std::vector<uint8_t> data = archive->extract(filename);
        if (!data.empty()) {
            std::string content(data.begin(), data.end());
            LOGI("Loaded .cub from BAR: %s (%zu bytes)\n", filename.c_str(), data.size());
            return parse_cub_file(content);
        }
    }

    std::ifstream file(filename, std::ios::binary);
    if (file) {
        std::ostringstream ss;
        ss << file.rdbuf();
        LOGI("Loaded .cub from disk: %s\n", filename.c_str());
        return parse_cub_file(ss.str());
    }

    LOGW("load_cub_definition: arquivo '%s' nao encontrado\n", filename.c_str());
    return {};
}

} // namespace aom
