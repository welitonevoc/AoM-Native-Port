#pragma once

#include "anim/anim_script.h"
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <memory>
#include <cstdint>

namespace aom {
namespace anim {

struct AsetHeader {
    uint32_t num_frames = 1;
    float frame_time = 0.1f;
    float anim_speed = 1.0f;
    float duration = 1.0f;
    float step_distance = 0.0f;
    float fps = 10.0f;
};

struct BrgFrame {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
};

class BrgAnimationClip {
public:
    std::string name;
    AsetHeader aset;
    uint32_t num_vertices = 0;
    uint32_t num_faces = 0;
    std::vector<uint32_t> indices;
    std::vector<glm::vec2> uvs;
    std::vector<BrgFrame> frames;
    std::string texture_name;

    bool is_valid() const { return !frames.empty() && num_vertices > 0; }
    size_t frame_count() const { return frames.size(); }
    float duration() const { return aset.duration; }

    // Interpolate vertex positions & normals at time t (in seconds)
    void evaluate(float time_sec, std::vector<glm::vec3>& out_positions, std::vector<glm::vec3>& out_normals) const;
};

class UnitAnimationState {
public:
    UnitAnimationState();
    ~UnitAnimationState() = default;

    void set_script(const AnimScript* script) { script_ = script; }
    void set_clip(const BrgAnimationClip* clip);

    bool play(const std::string& state_name, bool loop = true);
    void pause();
    void resume();
    void stop();

    void update(float dt, const std::function<void(const std::string& tag_name)>& on_tag = nullptr);

    const std::string& current_state() const { return current_state_; }
    float current_time() const { return current_time_; }
    float progress() const { return (duration_ > 0.0f) ? current_time_ / duration_ : 0.0f; }
    bool is_playing() const { return playing_; }
    bool is_looping() const { return loop_; }
    const BrgAnimationClip* clip() const { return current_clip_; }

private:
    const AnimScript* script_ = nullptr;
    const BrgAnimationClip* current_clip_ = nullptr;
    const AnimStateDef* current_state_def_ = nullptr;

    std::string current_state_ = "Idle";
    float current_time_ = 0.0f;
    float duration_ = 1.0f;
    bool loop_ = true;
    bool playing_ = false;

    std::unordered_set<std::string> fired_tags_;
};

class AnimationSystem {
public:
    AnimationSystem() = default;
    ~AnimationSystem() = default;

    // Parsers & loaders
    bool load_script_from_memory(const std::string& name, const std::string& text);
    bool load_brg_clip_from_memory(const std::string& name, const uint8_t* data, size_t size);

    static bool parse_brg_clip(const uint8_t* data, size_t size, const std::string& name, BrgAnimationClip& out_clip);

    const AnimScript* find_script(const std::string& name) const;
    const BrgAnimationClip* find_clip(const std::string& name) const;

    size_t script_count() const { return scripts_.size(); }
    size_t clip_count() const { return clips_.size(); }

private:
    std::unordered_map<std::string, AnimScript> scripts_;
    std::unordered_map<std::string, BrgAnimationClip> clips_;
};

} // namespace anim
} // namespace aom
