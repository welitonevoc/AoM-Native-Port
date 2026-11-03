#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>

namespace aom {

/// History Topic Definition (table of contents.xmb)
struct HistoryTopic {
    std::string name;
    std::string title_id;
    std::string help_file;
    std::string icon;
};

/// Rich Text Link in History (<link="proto:Villager Greek">)
struct RichTextLink {
    std::string link_type;   // "proto", "tech", "god", "help"
    std::string target;      // "Villager Greek"
    std::string anchor_text; // "aldeões"
    size_t start_pos = 0;
    size_t length = 0;
};

/// Parsed History Article with rich links
struct HistoryArticle {
    std::string raw_text;
    std::string plain_text;
    std::vector<RichTextLink> links;
};

/// Keybinding / Console Command (game.con)
struct KeyBinding {
    std::string key;
    std::string context; // "root", "game", "developer", "console"
    std::string action;  // "gadgetScrollDown", "pause", "toggleScore"
};

/// Random Map Metadata (.xml and .set)
struct RandomMapDef {
    std::string name;
    std::string display_name_id;
    std::string image_path;
    std::string help_text_id;
};

struct RandomMapSet {
    std::string set_name;
    std::string display_name_id;
    std::vector<std::string> maps;
};

/// Lightning VFX Definition (.lng)
struct LightningKeyframe {
    float time = 0.0f;
    float brightness = 1.0f;
};

struct LightningDef {
    float width = 0.8f;
    float branch_width = 0.3f;
    float segment_length = 1.0f;
    float min_branch_length = 2.0f;
    float max_branch_length = 4.0f;
    float min_perturb_angle = 0.1f;
    float max_perturb_angle = 0.5f;
    std::string texture;
    std::string blend_mode = "additive";
    std::vector<LightningKeyframe> anim_keys;
};

struct LightningBranch {
    std::vector<glm::vec3> points;
    float width = 0.8f;
};

class GameSystemsManager {
public:
    GameSystemsManager();
    ~GameSystemsManager();

    // --- History System ---
    bool load_history_toc(const uint8_t* data, size_t size);
    [[nodiscard]] size_t history_topic_count() const { return history_topics_.size(); }
    [[nodiscard]] const HistoryTopic* find_topic(const std::string& name) const;
    [[nodiscard]] const std::vector<HistoryTopic>& history_topics() const { return history_topics_; }
    static HistoryArticle parse_history_article(const std::string& article_utf16_or_ascii);

    // --- Console & Keybindings (.con) ---
    bool load_con_file(const std::string& con_content);
    [[nodiscard]] size_t keybinding_count() const { return bindings_.size(); }
    [[nodiscard]] const KeyBinding* find_binding(const std::string& key, const std::string& context) const;

    // --- Random Map (.xml and .set) ---
    static RandomMapDef parse_rm_xml(const std::string& xml_content);
    static RandomMapSet parse_rm_set(const std::string& set_content, const std::string& set_name);

    // --- Lightning VFX (.lng) ---
    static LightningDef parse_lng(const std::string& lng_content);
    static std::vector<LightningBranch> generate_lightning_bolt(
        const LightningDef& def, const glm::vec3& start, const glm::vec3& end, uint32_t seed = 42);

private:
    std::vector<HistoryTopic> history_topics_;
    std::vector<KeyBinding> bindings_;
};

} // namespace aom
