#include "graphics/lighting.h"

#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <cctype>
#include <glm/gtc/constants.hpp>

namespace aom {

namespace {

std::string to_lower(std::string_view s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return out;
}

std::string clean_token(const std::string& str) {
    std::string s = str;
    // Remove quotes
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        s = s.substr(1, s.size() - 2);
    }
    return s;
}

} // anonymous namespace

glm::vec3 LightingSet::calculate_sun_direction() const {
    const float rad_incl = glm::radians(sun_inclination);
    const float rad_rot = glm::radians(sun_rotation);

    // Direction vector from spherical coords (inclination = elevation, rotation = azimuth)
    float x = std::cos(rad_incl) * std::sin(rad_rot);
    float y = std::sin(rad_incl);
    float z = std::cos(rad_incl) * std::cos(rad_rot);

    glm::vec3 dir(x, y, z);
    float len = glm::length(dir);
    if (len > 0.0001f) {
        return glm::normalize(dir);
    }
    return glm::vec3(0.0f, 1.0f, 0.0f);
}

bool LightingManager::load_file(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        return false;
    }
    std::string content((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
    return parse_lgt_text(content, path);
}

bool LightingManager::parse_lgt_text(const std::string& text, const std::string& fallback_name) {
    std::istringstream stream(text);
    std::string line;

    LightingSet current_set;
    if (!fallback_name.empty()) {
        current_set.name = fallback_name;
    }

    bool has_set = false;

    while (std::getline(stream, line)) {
        // Strip comments starting with \\ or //
        size_t cpos = line.find("\\\\");
        if (cpos != std::string::npos) {
            line = line.substr(0, cpos);
        }
        cpos = line.find("//");
        if (cpos != std::string::npos) {
            line = line.substr(0, cpos);
        }

        // Trim leading and trailing whitespace
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) {
            continue;
        }
        size_t end = line.find_last_not_of(" \t\r\n");
        line = line.substr(start, end - start + 1);

        if (line.empty()) {
            continue;
        }

        std::istringstream line_stream(line);
        std::string command;
        line_stream >> command;
        std::string cmd_lower = to_lower(command);

        if (cmd_lower == "lightingset") {
            std::string name;
            std::getline(line_stream >> std::ws, name);
            current_set.name = clean_token(name);
            has_set = true;
        } else if (cmd_lower == "suncolor") {
            line_stream >> current_set.sun_color.r >> current_set.sun_color.g >> current_set.sun_color.b;
        } else if (cmd_lower == "sunspecular") {
            line_stream >> current_set.sun_specular.r >> current_set.sun_specular.g >> current_set.sun_specular.b;
        } else if (cmd_lower == "ambientcolor") {
            line_stream >> current_set.ambient_color.r >> current_set.ambient_color.g >> current_set.ambient_color.b;
        } else if (cmd_lower == "terrainambientcolor") {
            line_stream >> current_set.terrain_ambient_color.r >> current_set.terrain_ambient_color.g >> current_set.terrain_ambient_color.b;
        } else if (cmd_lower == "suninclination") {
            line_stream >> current_set.sun_inclination;
        } else if (cmd_lower == "sunrotation") {
            line_stream >> current_set.sun_rotation;
        }
    }

    if (current_set.name.empty()) {
        current_set.name = "Default";
    }

    std::string key = to_lower(current_set.name);
    sets_[key] = current_set;

    if (active_set_name_.empty() || key == "default") {
        active_set_name_ = key;
    }

    return true;
}

const LightingSet* LightingManager::get_set(const std::string& name) const {
    auto it = sets_.find(to_lower(name));
    if (it != sets_.end()) {
        return &it->second;
    }
    return nullptr;
}

const LightingSet* LightingManager::active_set() const {
    if (active_set_name_.empty()) {
        return nullptr;
    }
    return get_set(active_set_name_);
}

bool LightingManager::set_active(const std::string& name) {
    std::string key = to_lower(name);
    if (sets_.find(key) != sets_.end()) {
        active_set_name_ = key;
        return true;
    }
    return false;
}

} // namespace aom
