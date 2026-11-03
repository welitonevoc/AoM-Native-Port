#include "game/god_powers.h"
#include "assets/bar_archive.h"
#include "assets/xmb_parser.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>

#define LOG_TAG "AoM-GodPowers"
#ifdef __ANDROID__
#include <android/log.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#include <cstdio>
#define LOGI(...) std::printf(__VA_ARGS__)
#define LOGE(...) std::fprintf(stderr, __VA_ARGS__)
#endif

namespace aom {

namespace {

std::string to_lower_trim(std::string s) {
    while (!s.empty() && (s.back() == ' ' || s.back() == '\r' || s.back() == '\n' || s.back() == '\t')) {
        s.pop_back();
    }
    size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\t')) {
        ++start;
    }
    if (start > 0) s = s.substr(start);
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

float parse_float_safe(const std::string& str, float default_val = 0.0f) {
    if (str.empty()) return default_val;
    try {
        return std::stof(str);
    } catch (...) {
        return default_val;
    }
}

GodPowerTargetType parse_target_type(const std::string& t) {
    std::string s = to_lower_trim(t);
    if (s == "unit") return GodPowerTargetType::UNIT;
    if (s == "area" || s == "ground") return GodPowerTargetType::AREA;
    if (s == "global") return GodPowerTargetType::GLOBAL;
    if (s == "water") return GodPowerTargetType::WATER;
    return GodPowerTargetType::UNKNOWN;
}

GodPowerCategory infer_category(const std::string& name, const GodPowerDef& def) {
    std::string n = to_lower_trim(name);
    if (n.find("bolt") != std::string::npos || n.find("meteor") != std::string::npos ||
        n.find("earthquake") != std::string::npos || n.find("lightning") != std::string::npos ||
        n.find("tornado") != std::string::npos || n.find("implode") != std::string::npos ||
        n.find("volcano") != std::string::npos || n.find("undermine") != std::string::npos) {
        return GodPowerCategory::OFFENSIVE;
    }
    if (n.find("rain") != std::string::npos || n.find("prosperity") != std::string::npos ||
        n.find("hunt") != std::string::npos || n.find("plenty") != std::string::npos ||
        n.find("gaia") != std::string::npos || n.find("dwarven") != std::string::npos) {
        return GodPowerCategory::ECONOMIC;
    }
    if (n.find("serpent") != std::string::npos || n.find("nidhogg") != std::string::npos ||
        n.find("osiris") != std::string::npos || n.find("tartar") != std::string::npos ||
        n.find("titan") != std::string::npos || n.find("skeleton") != std::string::npos ||
        n.find("sentinel") != std::string::npos) {
        return GodPowerCategory::SUMMONING;
    }
    if (n.find("cease") != std::string::npos || n.find("restoration") != std::string::npos ||
        n.find("bronze") != std::string::npos || n.find("frost") != std::string::npos ||
        n.find("healing") != std::string::npos) {
        return GodPowerCategory::DEFENSIVE;
    }
    return GodPowerCategory::UTILITY;
}

} // anonymous namespace

GodPowerSystem::GodPowerSystem() = default;
GodPowerSystem::~GodPowerSystem() = default;

bool GodPowerSystem::load_from_bar(BarArchive* archive, bool is_expansion) {
    if (!archive || archive->file_count() == 0) return false;

    size_t loaded = 0;
    for (const auto& entry : archive->entries()) {
        std::string fn = to_lower_trim(entry.filename);
        if (fn.size() < 4 || fn.compare(fn.size() - 4, 4, ".xmb") != 0) continue;

        std::vector<uint8_t> raw = archive->extract(entry);
        if (raw.empty()) continue;

        if (load_power_xmb(raw.data(), raw.size(), is_expansion)) {
            ++loaded;
        }
    }

    LOGI("Loaded %zu god powers from %s (total: %zu)", loaded,
         is_expansion ? "godpowers2.bar" : "godpowers.bar", powers_.size());
    return loaded > 0;
}

bool GodPowerSystem::load_power_xmb(const uint8_t* data, size_t size, bool is_expansion) {
    if (!data || size < 4) return false;

    std::vector<uint8_t> buf(data, data + size);
    auto root = XmbParser::parse(buf);
    if (!root) return false;

    return load_power_from_element(*root, is_expansion);
}

bool GodPowerSystem::load_power_from_element(const XmbElement& root, bool is_expansion) {
    if (root.name != "power") return false;

    GodPowerDef def;
    def.is_expansion = is_expansion;

    def.name = root.get_attribute("name");
    def.tech_name = root.get_attribute("techname");
    def.target_type = parse_target_type(root.get_attribute("type"));

    for (const auto& child : root.children) {
        if (!child) continue;
        if (child->name == "activetime") {
            def.active_time = parse_float_safe(child->text);
        } else if (child->name == "builduptime") {
            def.buildup_time = parse_float_safe(child->text);
        } else if (child->name == "radius") {
            def.radius = parse_float_safe(child->text);
        } else if (child->name == "accuracy") {
            def.accuracy = parse_float_safe(child->text, 1.0f);
        } else if (child->name == "unitaitype") {
            def.valid_target_type = child->text;
        } else if (child->name == "soundset") {
            def.sound_set = child->text;
        } else if (child->name == "strike") {
            def.strike_vfx = child->text;
        } else if (child->name == "sparks") {
            def.sparks_vfx = child->text;
        } else if (child->name == "scorch") {
            def.scorch_vfx = child->text;
        } else if (child->name == "icon") {
            def.icon = child->text;
        } else if (child->name == "usedicon") {
            def.used_icon = child->text;
        } else if (child->name == "placement") {
            def.match_type = child->get_attribute("matchtype");
            std::string p_type = child->get_attribute("type");
            if (!p_type.empty()) {
                def.target_type = parse_target_type(p_type);
            }
        } else if (child->name == "gpdamagemodel") {
            for (const auto& dc : child->children) {
                if (!dc) continue;
                if (dc->name == "basedamagepercentunit") def.damage_unit_percent = parse_float_safe(dc->text);
                else if (dc->name == "basedamagepercentvillager") def.damage_villager_percent = parse_float_safe(dc->text);
                else if (dc->name == "basedamagepercentbuilding") def.damage_building_percent = parse_float_safe(dc->text);
                else if (dc->name == "minhpdamageunit") def.min_hp_damage_unit = parse_float_safe(dc->text);
                else if (dc->name == "maxhpdamageunit") def.max_hp_damage_unit = parse_float_safe(dc->text);
            }
        } else if (child->name == "unitthrowchance") {
            def.unit_throw_chance = parse_float_safe(child->text);
        } else if (child->name == "minthrowdistance") {
            def.min_throw_distance = parse_float_safe(child->text);
        } else if (child->name == "maxthrowdistance") {
            def.max_throw_distance = parse_float_safe(child->text);
        } else if (child->name == "minthrowheight") {
            def.min_throw_height = parse_float_safe(child->text);
        }
    }

    if (def.target_type == GodPowerTargetType::UNKNOWN) {
        std::string mt = to_lower_trim(def.match_type);
        if (mt.find("unit") != std::string::npos || mt.find("target") != std::string::npos ||
            mt.find("villager") != std::string::npos || mt.find("hero") != std::string::npos) {
            def.target_type = GodPowerTargetType::UNIT;
        } else if (mt.find("water") != std::string::npos) {
            def.target_type = GodPowerTargetType::WATER;
        } else if (def.radius > 0.0f || mt.find("land") != std::string::npos || mt.find("area") != std::string::npos) {
            def.target_type = GodPowerTargetType::AREA;
        } else if (def.active_time > 0.0f) {
            def.target_type = GodPowerTargetType::GLOBAL;
        }
    }

    if (def.name.empty()) {
        def.name = def.tech_name;
    }
    if (def.name.empty()) return false;

    def.category = infer_category(def.name, def);

    std::string key = to_lower_trim(def.name);
    powers_[key] = def;
    return true;
}

void GodPowerSystem::register_power(const GodPowerDef& def) {
    if (def.name.empty()) return;
    std::string key = to_lower_trim(def.name);
    powers_[key] = def;
}

const GodPowerDef* GodPowerSystem::find_power(const std::string& name) const {
    std::string key = to_lower_trim(name);
    auto it = powers_.find(key);
    if (it != powers_.end()) return &it->second;
    return nullptr;
}

bool GodPowerSystem::cast_power(const std::string& name, uint32_t player_id,
                                const glm::vec3& target_pos, uint32_t target_unit_id) {
    const GodPowerDef* def = find_power(name);
    if (!def) {
        LOGE("Cannot cast unknown god power: %s", name.c_str());
        return false;
    }

    ActiveGodPower active;
    active.power_name = def->name;
    active.caster_player_id = player_id;
    active.target_pos = target_pos;
    active.target_unit_id = target_unit_id;
    active.elapsed_time = 0.0f;
    active.total_duration = std::max(def->active_time, 0.5f);
    active.is_active = true;

    active_powers_.push_back(active);
    LOGI("Cast God Power: %s by player %u at (%.1f, %.1f, %.1f), duration=%.1fs",
         def->name.c_str(), player_id, target_pos.x, target_pos.y, target_pos.z, active.total_duration);

    if (on_power_cast_) {
        on_power_cast_(active);
    }
    return true;
}

void GodPowerSystem::update(float delta_time) {
    for (auto it = active_powers_.begin(); it != active_powers_.end();) {
        it->elapsed_time += delta_time;
        if (it->elapsed_time >= it->total_duration) {
            it->is_active = false;
            it = active_powers_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace aom
