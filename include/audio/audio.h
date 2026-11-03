#pragma once

#include <string>
#include <memory>
#include <vector>
#include <cstdint>
#include <glm/glm.hpp>

struct AAssetManager;

namespace aom {

class BarArchive;

class AudioEngine {
public:
    virtual ~AudioEngine() = default;

    virtual bool initialize(AAssetManager* asset_mgr = nullptr) = 0;
    virtual void shutdown() = 0;

    virtual void play_music(const std::string& path, bool loop = true) = 0;
    virtual void stop_music() = 0;
    virtual void set_music_volume(float volume) = 0; // 0.0f - 1.0f
    virtual void set_music_enabled(bool enabled) = 0;

    virtual void play_sfx(const std::string& path, float volume = 1.0f) = 0;
    virtual void play_click() = 0;
    virtual void play_big_button() = 0;
    virtual void play_unit_voice(const std::string& category, bool worker = true) = 0;
    virtual void set_sfx_volume(float volume) = 0;   // 0.0f - 1.0f
    virtual void set_sfx_enabled(bool enabled) = 0;

    virtual bool is_music_enabled() const = 0;
    virtual bool is_sfx_enabled() const = 0;
    virtual float music_volume() const = 0;
    virtual float sfx_volume() const = 0;

    // === Sprint 6: 3D Positional Audio & Authentic AoM Audio System ===
    virtual void set_listener(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& right) = 0;
    virtual glm::vec3 listener_position() const = 0;

    virtual void play_sfx_3d(const std::string& path, const glm::vec3& world_pos, float volume = 1.0f,
                             float min_distance = 5.0f, float max_distance = 65.0f) = 0;

    virtual void play_unit_voice_3d(const std::string& category, const glm::vec3& world_pos,
                                    const std::string& unit_type_name = "greek", bool worker = false) = 0;

    virtual void play_combat_hit(const glm::vec3& world_pos, bool is_metal = true) = 0;
    virtual void play_arrow_shot(const glm::vec3& world_pos) = 0;
    virtual void play_arrow_hit(const glm::vec3& world_pos) = 0;
    virtual void play_harvest_sound(const glm::vec3& world_pos, const std::string& resource_type) = 0;
    virtual void play_building_work(const glm::vec3& world_pos) = 0;
    virtual void play_unit_death(const glm::vec3& world_pos, bool female = false) = 0;

    virtual void set_sound_archive(BarArchive* archive) = 0;
    virtual size_t active_voice_count() const = 0;
};

std::unique_ptr<AudioEngine> create_audio_engine();

} // namespace aom

