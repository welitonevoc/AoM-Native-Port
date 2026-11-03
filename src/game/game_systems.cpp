#include "game/game_systems.h"
#include "assets/xmb_parser.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>

#define LOG_TAG "AoM-GameSystems"
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

std::string trim_spaces(std::string s) {
    while (!s.empty() && (s.back() == ' ' || s.back() == '\r' || s.back() == '\n' || s.back() == '\t')) {
        s.pop_back();
    }
    size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\t')) {
        ++start;
    }
    if (start > 0) s = s.substr(start);
    return s;
}

std::string tag_attribute(const std::string& line, const std::string& attr) {
    std::string needle = attr + "=\"";
    size_t pos = line.find(needle);
    if (pos == std::string::npos) {
        needle = attr + " = \"";
        pos = line.find(needle);
    }
    if (pos == std::string::npos) return "";
    pos += needle.size();
    size_t end = line.find('"', pos);
    if (end == std::string::npos) return "";
    return line.substr(pos, end - pos);
}

std::string tag_inner_text(const std::string& text, const std::string& tag) {
    std::string open = "<" + tag + ">";
    std::string close = "</" + tag + ">";
    size_t start = text.find(open);
    if (start == std::string::npos) return "";
    start += open.size();
    size_t end = text.find(close, start);
    if (end == std::string::npos) return "";
    return text.substr(start, end - start);
}

float parse_f(const std::string& s, float def_val = 0.0f) {
    if (s.empty()) return def_val;
    try {
        return std::stof(s);
    } catch (...) {
        return def_val;
    }
}

} // anonymous namespace

GameSystemsManager::GameSystemsManager() = default;
GameSystemsManager::~GameSystemsManager() = default;

bool GameSystemsManager::load_history_toc(const uint8_t* data, size_t size) {
    if (!data || size < 4) return false;

    std::vector<uint8_t> buf(data, data + size);
    auto root = XmbParser::parse(buf);
    if (!root) return false;

    history_topics_.clear();

    for (const auto& child : root->children) {
        if (!child) continue;
        if (child->name == "topic") {
            HistoryTopic topic;
            topic.name = child->get_attribute("name");
            topic.title_id = child->get_attribute("titleid");
            topic.help_file = child->get_attribute("helpfile");
            topic.icon = child->get_attribute("icon");

            if (!topic.name.empty()) {
                history_topics_.push_back(topic);
            }
        }
    }

    LOGI("Loaded %zu history topics from table of contents", history_topics_.size());
    return !history_topics_.empty();
}

const HistoryTopic* GameSystemsManager::find_topic(const std::string& name) const {
    std::string target = trim_spaces(name);
    for (const auto& t : history_topics_) {
        if (t.name == target || t.help_file == target) {
            return &t;
        }
    }
    return nullptr;
}

HistoryArticle GameSystemsManager::parse_history_article(const std::string& raw) {
    HistoryArticle article;
    article.raw_text = raw;

    // Convert UTF-16LE if detected (every other byte 0)
    std::string text;
    if (raw.size() >= 4 && raw[1] == 0 && raw[3] == 0) {
        for (size_t i = 0; i + 1 < raw.size(); i += 2) {
            char c = raw[i];
            if (c >= 32 && c <= 126) text.push_back(c);
            else if (c == '\n' || c == '\r' || c == '\t') text.push_back(c);
            else if (static_cast<unsigned char>(c) >= 128) text.push_back(c); // latin-1
            else text.push_back(' ');
        }
    } else {
        text = raw;
    }

    // Extract links <link="proto:Villager Greek">aldeões</link>
    size_t pos = 0;
    std::string plain;
    while (pos < text.size()) {
        size_t link_start = text.find("<link=\"", pos);
        if (link_start == std::string::npos) {
            plain += text.substr(pos);
            break;
        }

        plain += text.substr(pos, link_start - pos);

        size_t target_start = link_start + 7;
        size_t target_end = text.find("\">", target_start);
        if (target_end == std::string::npos) {
            plain += text.substr(link_start);
            break;
        }

        std::string full_target = text.substr(target_start, target_end - target_start);
        size_t content_start = target_end + 2;
        size_t content_end = text.find("</link>", content_start);
        if (content_end == std::string::npos) {
            plain += text.substr(link_start);
            break;
        }

        std::string anchor = text.substr(content_start, content_end - content_start);

        RichTextLink link;
        link.anchor_text = anchor;
        link.start_pos = plain.size();
        link.length = anchor.size();

        size_t colon = full_target.find(':');
        if (colon != std::string::npos) {
            link.link_type = full_target.substr(0, colon);
            link.target = full_target.substr(colon + 1);
        } else {
            link.link_type = "help";
            link.target = full_target;
        }

        article.links.push_back(link);
        plain += anchor;

        pos = content_end + 7; // skip </link>
    }

    article.plain_text = plain;
    return article;
}

bool GameSystemsManager::load_con_file(const std::string& con_content) {
    std::istringstream stream(con_content);
    std::string line;
    size_t count = 0;

    while (std::getline(stream, line)) {
        line = trim_spaces(line);
        if (line.empty() || line.rfind("//", 0) == 0) continue;

        if (line.rfind("map", 0) == 0) {
            // Format: map ("key", "context", "action")
            std::vector<std::string> args;
            size_t idx = 0;
            while (idx < line.size()) {
                size_t q1 = line.find('"', idx);
                if (q1 == std::string::npos) break;
                size_t q2 = line.find('"', q1 + 1);
                if (q2 == std::string::npos) break;
                args.push_back(line.substr(q1 + 1, q2 - q1 - 1));
                idx = q2 + 1;
            }

            if (args.size() >= 3) {
                KeyBinding kb;
                kb.key = args[0];
                kb.context = args[1];
                kb.action = args[2];
                bindings_.push_back(kb);
                ++count;
            }
        }
    }

    LOGI("Loaded %zu keybindings from .con file", count);
    return count > 0;
}

const KeyBinding* GameSystemsManager::find_binding(const std::string& key, const std::string& context) const {
    std::string k = trim_spaces(key);
    std::string ctx = trim_spaces(context);
    for (const auto& b : bindings_) {
        if (b.key == k && (b.context == ctx || b.context == "root")) {
            return &b;
        }
    }
    return nullptr;
}

RandomMapDef GameSystemsManager::parse_rm_xml(const std::string& xml_content) {
    RandomMapDef def;
    def.name = tag_attribute(xml_content, "name");
    if (def.name.empty()) def.name = tag_attribute(xml_content, "displayNameID");
    def.display_name_id = tag_attribute(xml_content, "displayNameID");
    def.image_path = tag_attribute(xml_content, "imagepath");
    def.help_text_id = tag_attribute(xml_content, "helpTextID");
    return def;
}

RandomMapSet GameSystemsManager::parse_rm_set(const std::string& set_content, const std::string& set_name) {
    RandomMapSet set;
    set.set_name = set_name;
    set.display_name_id = tag_attribute(set_content, "displayNameID");

    std::istringstream stream(set_content);
    std::string line;
    while (std::getline(stream, line)) {
        size_t m1 = line.find("<map>");
        if (m1 != std::string::npos) {
            size_t m2 = line.find("</map>", m1 + 5);
            if (m2 != std::string::npos) {
                std::string map_name = line.substr(m1 + 5, m2 - m1 - 5);
                map_name = trim_spaces(map_name);
                if (!map_name.empty()) {
                    set.maps.push_back(map_name);
                }
            }
        }
    }
    return set;
}

LightningDef GameSystemsManager::parse_lng(const std::string& lng_content) {
    LightningDef def;
    def.width = parse_f(tag_inner_text(lng_content, "width"), 0.8f);
    def.branch_width = parse_f(tag_inner_text(lng_content, "BranchWidth"), 0.3f);
    def.segment_length = parse_f(tag_inner_text(lng_content, "segmentLength"), 1.0f);
    def.min_branch_length = parse_f(tag_inner_text(lng_content, "MinBranchLength"), 2.0f);
    def.max_branch_length = parse_f(tag_inner_text(lng_content, "MaxBranchLength"), 4.0f);
    def.min_perturb_angle = parse_f(tag_inner_text(lng_content, "MinPerturbAngle"), 0.1f);
    def.max_perturb_angle = parse_f(tag_inner_text(lng_content, "MaxPerturbAngle"), 0.5f);

    size_t mat_pos = lng_content.find("<material>");
    if (mat_pos != std::string::npos) {
        size_t mat_end = lng_content.find("</material>", mat_pos);
        if (mat_end != std::string::npos) {
            std::string mat_str = lng_content.substr(mat_pos, mat_end - mat_pos);
            def.texture = tag_inner_text(mat_str, "texture");
            def.blend_mode = tag_inner_text(mat_str, "blend");
            if (def.blend_mode.empty()) def.blend_mode = "additive";
        }
    }

    // Keyframes <key time="0.10" brightness="0.6"/>
    size_t pos = 0;
    while ((pos = lng_content.find("<key ", pos)) != std::string::npos) {
        size_t end_key = lng_content.find("/>", pos);
        if (end_key == std::string::npos) break;
        std::string key_str = lng_content.substr(pos, end_key - pos);
        LightningKeyframe kf;
        kf.time = parse_f(tag_attribute(key_str, "time"), 0.0f);
        kf.brightness = parse_f(tag_attribute(key_str, "brightness"), 1.0f);
        def.anim_keys.push_back(kf);
        pos = end_key + 2;
    }

    return def;
}

std::vector<LightningBranch> GameSystemsManager::generate_lightning_bolt(
    const LightningDef& def, const glm::vec3& start, const glm::vec3& end, uint32_t seed) {
    std::vector<LightningBranch> result;
    LightningBranch trunk;
    trunk.width = def.width;
    trunk.points.push_back(start);

    glm::vec3 dir = end - start;
    float dist = glm::length(dir);
    if (dist < 0.01f) {
        trunk.points.push_back(end);
        result.push_back(trunk);
        return result;
    }

    glm::vec3 norm_dir = glm::normalize(dir);
    // Orthogonal vector for perpendicular displacement
    glm::vec3 up = (std::abs(norm_dir.y) < 0.9f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
    glm::vec3 side = glm::normalize(glm::cross(norm_dir, up));

    int num_segs = std::max(2, static_cast<int>(dist / std::max(def.segment_length, 0.5f)));
    uint32_t state = seed;
    auto lcg_rand = [&state]() -> float {
        state = state * 1664525u + 1013904223u;
        return static_cast<float>(state & 0x00FFFFFF) / static_cast<float>(0x00FFFFFF);
    };

    for (int i = 1; i < num_segs; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(num_segs);
        glm::vec3 base_pt = start + dir * t;
        float jitter_mag = (lcg_rand() * 2.0f - 1.0f) * def.segment_length * 0.4f;
        glm::vec3 pt = base_pt + side * jitter_mag;
        trunk.points.push_back(pt);

        // Branching chance
        if (i % 3 == 0 && lcg_rand() > 0.4f) {
            LightningBranch branch;
            branch.width = def.branch_width;
            branch.points.push_back(pt);

            float b_len = def.min_branch_length + lcg_rand() * (def.max_branch_length - def.min_branch_length);
            glm::vec3 b_dir = glm::normalize(norm_dir + side * (lcg_rand() > 0.5f ? 1.0f : -1.0f));
            branch.points.push_back(pt + b_dir * b_len);
            result.push_back(branch);
        }
    }
    trunk.points.push_back(end);
    result.insert(result.begin(), trunk);
    return result;
}

} // namespace aom
