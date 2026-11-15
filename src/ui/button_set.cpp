/// @file button_set.cpp
/// @brief Implementação do gerenciador de buttonsets do AoM

#include "ui/button_set.h"
#include <sstream>
#include <algorithm>

#ifdef __ANDROID__
#include <android/log.h>
#define LOG_TAG "AoM-ButtonSet"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#include <cstdio>
#define LOGI(...) printf("[ButtonSet] " __VA_ARGS__), printf("\n")
#define LOGW(...) printf("[ButtonSet WARN] " __VA_ARGS__), printf("\n")
#define LOGE(...) fprintf(stderr, "[ButtonSet ERROR] " __VA_ARGS__), fprintf(stderr, "\n")
#endif

namespace aom {

namespace {

static std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static std::vector<float> parse_floats(const std::string& str) {
    std::vector<float> result;
    std::istringstream iss(str);
    float val = 0.0f;
    while (iss >> val) {
        result.push_back(val);
    }
    return result;
}

} // namespace

bool ButtonSetManager::load_from_xmb_data(const std::vector<uint8_t>& data) {
    if (data.empty()) return false;

    auto root = XmbParser::parse(data);
    if (!root) {
        LOGE("Falha ao analisar dados XMB do buttonsets");
        return false;
    }

    sets_.clear();

    // Raiz deve ser <buttonsets>
    for (const auto& child : root->children) {
        if (!child || child->name != "set") continue;

        std::string set_name = child->get_attribute("name");
        if (set_name.empty()) continue;

        ButtonStateDef def;
        def.name = set_name;
        def.has_bevel = child->has_attribute("bevel");

        if (child->has_attribute("texturecoords")) {
            auto coords = parse_floats(child->get_attribute("texturecoords"));
            if (coords.size() >= 4) {
                def.texture_coords = glm::vec4(coords[0], coords[1], coords[2], coords[3]);
                def.has_texture_coords = true;
            }
        }

        for (const auto& sub : child->children) {
            if (!sub) continue;
            std::string text = trim(sub->text);
            if (sub->name == "normal") {
                def.normal_tex = text;
            } else if (sub->name == "depressed") {
                def.depressed_tex = text;
            } else if (sub->name == "highlight") {
                def.highlight_tex = text;
            } else if (sub->name == "disabled") {
                def.disabled_tex = text;
            }
        }

        LOGI("ButtonSet carregado: '%s' normal='%s' depressed='%s' uv=(%.2f, %.2f, %.2f, %.2f)",
             def.name.c_str(), def.normal_tex.c_str(), def.depressed_tex.c_str(),
             def.texture_coords.x, def.texture_coords.y, def.texture_coords.z, def.texture_coords.w);

        sets_[def.name] = std::move(def);
    }

    LOGI("ButtonSetManager: %zu conjuntos de botões carregados com sucesso", sets_.size());
    return !sets_.empty();
}

bool ButtonSetManager::load_from_bar(BarArchive& bar, const std::string& filename) {
    const BarEntry* entry = bar.find(filename);
    if (!entry) {
        LOGW("Arquivo '%s' não encontrado no BAR", filename.c_str());
        return false;
    }

    auto data = bar.extract(*entry);
    if (data.empty()) {
        LOGE("Falha ao extrair '%s' do BAR", filename.c_str());
        return false;
    }

    return load_from_xmb_data(data);
}

const ButtonStateDef* ButtonSetManager::get_set(const std::string& name) const {
    auto it = sets_.find(name);
    if (it != sets_.end()) {
        return &it->second;
    }
    return nullptr;
}

bool ButtonSetManager::has_set(const std::string& name) const {
    return sets_.find(name) != sets_.end();
}

} // namespace aom
