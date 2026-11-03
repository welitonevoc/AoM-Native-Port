#include "graphics/gfx_config.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace aom {

namespace {

std::string to_lower(std::string_view s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return out;
}

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, last - first + 1);
}

std::string get_filename(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos != std::string::npos) {
        return path.substr(pos + 1);
    }
    return path;
}

} // anonymous namespace

bool GfxConfigManager::load_vendor_file(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        return false;
    }

    std::string fname = get_filename(path);
    std::string vendor_id;
    size_t underscore = fname.find('_');
    if (underscore != std::string::npos && fname.rfind("0x", 0) == 0) {
        vendor_id = fname.substr(0, underscore);
    } else {
        vendor_id = "0x0000";
    }

    std::string line;
    std::string current_section;
    size_t added = 0;

    while (std::getline(in, line)) {
        // Strip comments
        size_t cpos = line.find("//");
        if (cpos != std::string::npos) {
            line = line.substr(0, cpos);
        }
        cpos = line.find(';');
        if (cpos != std::string::npos) {
            line = line.substr(0, cpos);
        }

        line = trim(line);
        if (line.empty()) continue;

        if (line.front() == '[' && line.back() == ']') {
            current_section = to_lower(line.substr(1, line.size() - 2));
            continue;
        }

        if (current_section == "device") {
            size_t eq = line.find('=');
            if (eq != std::string::npos) {
                std::string dev_id = trim(line.substr(0, eq));
                std::string rest = trim(line.substr(eq + 1));

                std::string dev_name = rest;
                std::string profile = "";
                size_t comma = rest.find(',');
                if (comma != std::string::npos) {
                    dev_name = trim(rest.substr(0, comma));
                    profile = trim(rest.substr(comma + 1));
                }

                GfxDeviceInfo info;
                info.vendor_id = vendor_id;
                info.device_id = dev_id;
                info.device_name = dev_name;
                info.profile_file = profile;

                std::string key = to_lower(vendor_id) + ":" + to_lower(dev_id);
                devices_[key] = std::move(info);
                ++added;
            }
        }
    }
    return (added > 0 || !devices_.empty());
}

bool GfxConfigManager::load_profile_file(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        return false;
    }

    std::string fname = to_lower(get_filename(path));
    GfxProfile profile;
    profile.name = fname;

    std::string line;
    std::string current_section;

    while (std::getline(in, line)) {
        size_t cpos = line.find("//");
        if (cpos != std::string::npos) {
            line = line.substr(0, cpos);
        }
        line = trim(line);
        if (line.empty()) continue;

        if (line.front() == '[' && line.back() == ']') {
            current_section = to_lower(line.substr(1, line.size() - 2));
            continue;
        }

        size_t eq = line.find('=');
        if (eq != std::string::npos) {
            std::string key = trim(line.substr(0, eq));
            std::string val = trim(line.substr(eq + 1));

            if (current_section == "features") {
                profile.features[key] = val;
                std::string k_low = to_lower(key);
                if (k_low == "anisotropy") {
                    profile.anisotropy = std::stoi(val);
                } else if (k_low == "qualitylevel") {
                    profile.quality_level = val;
                } else if (k_low == "fontuvoffset") {
                    profile.font_uv_offset = std::stof(val);
                } else if (k_low == "win32cursor") {
                    profile.win32_cursor = (val == "1" || val == "true");
                }
            } else if (current_section == "resolutions") {
                profile.resolutions.push_back(val);
            } else if (current_section == "defaultrenderstates") {
                profile.render_states[key] = val;
            }
        }
    }

    profiles_[fname] = std::move(profile);
    if (active_profile_name_.empty()) {
        active_profile_name_ = fname;
    }
    return true;
}

bool GfxConfigManager::load_tss_file(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        return false;
    }

    std::string fname = to_lower(get_filename(path));
    TextureStagePipeline pipeline;
    pipeline.name = fname;

    std::string line;
    std::string current_section;
    TextureStagePass current_pass;
    int current_pass_idx = -1;

    while (std::getline(in, line)) {
        size_t cpos = line.find("//");
        if (cpos != std::string::npos) {
            line = line.substr(0, cpos);
        }
        line = trim(line);
        if (line.empty()) continue;

        if (line.front() == '[' && line.back() == ']') {
            std::string sec = to_lower(line.substr(1, line.size() - 2));
            if (sec.rfind("pass", 0) == 0) {
                if (current_pass_idx >= 0 && !current_pass.operations.empty()) {
                    pipeline.pass_list.push_back(std::move(current_pass));
                    current_pass = TextureStagePass{};
                }
                current_pass_idx = std::stoi(sec.substr(4));
            }
            current_section = sec;
            continue;
        }

        size_t eq = line.find('=');
        if (eq != std::string::npos) {
            std::string key = trim(line.substr(0, eq));
            std::string val = trim(line.substr(eq + 1));

            if (current_section == "config" && to_lower(key) == "passes") {
                pipeline.passes = std::stoi(val);
            } else if (current_section.rfind("pass", 0) == 0) {
                current_pass.operations[key] = val;
            }
        }
    }

    if (!current_pass.operations.empty()) {
        pipeline.pass_list.push_back(std::move(current_pass));
    }

    pipelines_[fname] = std::move(pipeline);
    return true;
}

const GfxDeviceInfo* GfxConfigManager::find_device(const std::string& vendor_id,
                                                   const std::string& device_id) const {
    std::string key = to_lower(vendor_id) + ":" + to_lower(device_id);
    auto it = devices_.find(key);
    if (it != devices_.end()) {
        return &it->second;
    }
    return nullptr;
}

const GfxProfile* GfxConfigManager::find_profile(const std::string& name) const {
    auto it = profiles_.find(to_lower(name));
    if (it != profiles_.end()) {
        return &it->second;
    }
    return nullptr;
}

const TextureStagePipeline* GfxConfigManager::find_pipeline(const std::string& name) const {
    auto it = pipelines_.find(to_lower(name));
    if (it != pipelines_.end()) {
        return &it->second;
    }
    return nullptr;
}

void GfxConfigManager::configure_mobile_gles3(const std::string& gl_renderer) {
    GfxProfile mobile;
    mobile.name = "mobile_gles3";
    mobile.anisotropy = 4;
    mobile.font_uv_offset = 0.0f;
    mobile.win32_cursor = false;

    std::string r_low = to_lower(gl_renderer);
    if (r_low.find("adreno") != std::string::npos ||
        r_low.find("mali-g") != std::string::npos ||
        r_low.find("xclipse") != std::string::npos) {
        mobile.quality_level = "high";
    } else {
        mobile.quality_level = "medium";
    }

    mobile.features["mobileGLES3"] = "true";
    mobile.features["textureCompression"] = "ETC2/EAC";
    mobile.features["maxSimultaneousTextures"] = "8";
    mobile.features["shadows"] = "true";
    mobile.features["waterReflections"] = "true";

    profiles_[mobile.name] = mobile;
    active_profile_name_ = mobile.name;
}

const GfxProfile* GfxConfigManager::active_profile() const {
    if (active_profile_name_.empty()) {
        return nullptr;
    }
    return find_profile(active_profile_name_);
}

} // namespace aom
