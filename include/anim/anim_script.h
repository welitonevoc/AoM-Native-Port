#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>

namespace aom {
namespace anim {

struct AnimTag {
    std::string name;
    float time_fraction = 0.0f; // 0.0 to 1.0
    bool flag = true;
};

struct AnimAttachment {
    std::string hotspot;
    std::string definition;
};

struct AnimVisual {
    std::string type; // "Visual", "VisualGranny", "VisualParticle"
    std::string name; // e.g. "Infantry G Hoplite_IdleA", "Hop_idleA"
};

struct AnimStateDef {
    std::string name; // "Idle", "Walk", "run", "Attack", "Death", "Bored", "Flail", "Birth", "Cinematic"
    std::vector<AnimVisual> visuals;
    std::vector<AnimTag> tags;
    std::vector<AnimAttachment> attachments;
};

struct AnimScript {
    std::string name;
    std::vector<std::string> imports;
    std::unordered_map<std::string, std::vector<AnimVisual>> definitions; // e.g. "Head", "Spear"
    std::unordered_map<std::string, AnimStateDef> states; // keyed by lowercase state name

    const AnimStateDef* get_state(const std::string& state_name) const {
        std::string lower = state_name;
        for (char& c : lower) c = static_cast<char>(::tolower(c));
        auto it = states.find(lower);
        return (it != states.end()) ? &it->second : nullptr;
    }
};

class AnimScriptParser {
public:
    static bool parse(const std::string& script_text, AnimScript& out_script);
    static bool parse_file(const std::string& filepath, AnimScript& out_script);
};

} // namespace anim
} // namespace aom
