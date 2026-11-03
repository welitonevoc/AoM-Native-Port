#pragma once

/// @file camera_track.h
/// @brief Reverse-engineered Camera and CameraTrack (.cam) system for Age of Mythology
///
/// Dissected and reconstructed from AoM binary (aomxnocd.exe / D:\core\camera.cpp)
/// Supports:
///   1. "CM" chunk: Camera state snapshot (184-byte payload, e.g. supascreen.cam)
///   2. "CT" chunk: Camera Track with multiple waypoints and Kochanek-Bartels (TCB) splines
///   3. Smooth cinematic camera evaluation with Tension, Continuity, and Bias parameters.

#include "renderer/renderer.h"
#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace aom {

/// Waypoint in a cinematic camera track
struct CameraWaypoint {
    float time = 0.0f;            ///< Timestamp in seconds
    glm::vec3 position{0.0f};    ///< World position
    glm::vec3 target{0.0f};      ///< Look-at focus point
    glm::vec3 up{0.0f, 1.0f, 0.0f}; ///< Up vector
    float fov = 45.0f;           ///< Field of view in degrees

    // Kochanek-Bartels (TCB) Spline parameters [-1.0, 1.0]
    float tension = 0.0f;        ///< Tightness of the curve (0 = Catmull-Rom)
    float continuity = 0.0f;     ///< Sharpness of direction change (0 = continuous)
    float bias = 0.0f;           ///< Direction of the curve / overshoot (0 = neutral)

    std::string event_name;      ///< Script event triggered at this waypoint
};

/// Camera mode matching AoM D:\core\camera.cpp
enum class CameraType : uint8_t {
    STRATEGY = 0, ///< Standard RTS isometric overhead camera
    TRACK    = 1, ///< Cinematic camera track follower
    NONE     = 2, ///< Static or uninitialized
    UNKNOWN  = 3
};

/// Full Camera snapshot state parsed from "CM" chunk
struct CameraSnapshot {
    uint32_t version = 2;
    glm::vec3 position{0.0f};
    glm::vec3 forward{0.0f, 0.0f, 1.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    glm::vec3 right{1.0f, 0.0f, 0.0f};
    uint8_t flags = 0;
    CameraType type = CameraType::STRATEGY;
    glm::vec3 target{0.0f};
    float fov = 50.0f;
    float near_plane = 0.075f;
    float far_plane = 1000.0f;
    float aspect = 1.3333f;
    float pitch_min = -1.5708f; ///< -pi/2
    float pitch_max = 1.5708f;  ///< +pi/2
    float fov_default = 63.12f;
};

/// Camera Track managing waypoints, interpolation, and playback
class CameraTrack {
public:
    CameraTrack();
    explicit CameraTrack(const std::string& name);

    /// Clear all waypoints and reset
    void clear();

    /// Add a waypoint to the track
    void add_waypoint(const CameraWaypoint& wp);

    /// Get waypoint count
    [[nodiscard]] size_t waypoint_count() const { return waypoints_.size(); }

    /// Access waypoints
    [[nodiscard]] const std::vector<CameraWaypoint>& waypoints() const { return waypoints_; }
    [[nodiscard]] std::vector<CameraWaypoint>& waypoints() { return waypoints_; }

    /// Set total duration in seconds (if 0, uses last waypoint time)
    void set_duration(float seconds) { duration_ = seconds; }
    [[nodiscard]] float duration() const;

    /// Loop playback
    void set_loop(bool loop) { loop_ = loop; }
    [[nodiscard]] bool is_looping() const { return loop_; }

    /// Track name
    void set_name(const std::string& name) { name_ = name; }
    [[nodiscard]] const std::string& name() const { return name_; }

    // --- Serialization (.cam format) ---

    /// Load from binary .cam data (handles both "CM" snapshot and "CT" tracks)
    bool load_from_memory(const uint8_t* data, size_t size);
    bool load_from_memory(const std::vector<uint8_t>& data) {
        return load_from_memory(data.data(), data.size());
    }

    /// Load from file path on disk
    bool load_from_file(const std::string& path);

    /// Save as binary .cam data (outputs "CT" track format)
    [[nodiscard]] std::vector<uint8_t> save_to_memory() const;

    /// Save to file on disk
    bool save_to_file(const std::string& path) const;

    /// Save a single camera snapshot as "CM" chunk file (e.g. supascreen.cam)
    static std::vector<uint8_t> save_camera_snapshot(const Camera& cam, const CameraSnapshot& snap);

    // --- Playback and Evaluation ---

    /// Start playing the camera track
    void play(float override_duration = -1.0f);

    /// Pause playback
    void pause();

    /// Resume playback
    void resume();

    /// Stop playback and rewind to start
    void stop();

    /// Check if currently playing
    [[nodiscard]] bool is_playing() const { return is_playing_; }
    [[nodiscard]] bool is_paused() const { return is_paused_; }

    /// Current playback progress [0.0, 1.0]
    [[nodiscard]] float progress() const;

    /// Current playback time in seconds
    [[nodiscard]] float current_time() const { return current_time_; }
    void set_current_time(float t);

    /// Advance playback time and compute updated Camera
    /// @param delta_time Elapsed seconds since last frame
    /// @param out_camera Destination camera to receive interpolated position/target/up/fov
    /// @return true if track is still playing, false if reached end
    bool update(float delta_time, Camera& out_camera);

    /// Evaluate camera at an exact time without advancing playback
    [[nodiscard]] Camera evaluate(float time_seconds) const;

    /// Check if the track was loaded from a snapshot ("CM")
    [[nodiscard]] bool is_snapshot() const { return is_snapshot_; }
    [[nodiscard]] const CameraSnapshot& snapshot() const { return snapshot_; }

private:
    /// Compute Kochanek-Bartels (TCB) cubic spline tangents and position
    static glm::vec3 interpolate_tcb(
        const glm::vec3& p_prev, const glm::vec3& p_curr,
        const glm::vec3& p_next, const glm::vec3& p_post,
        float t_param,
        float tension, float continuity, float bias);

    /// Hermite interpolation helper
    static glm::vec3 hermite(const glm::vec3& p0, const glm::vec3& m0,
                             const glm::vec3& p1, const glm::vec3& m1, float t);

    std::string name_;
    std::vector<CameraWaypoint> waypoints_;
    float duration_ = 0.0f;
    float current_time_ = 0.0f;
    bool loop_ = false;
    bool is_playing_ = false;
    bool is_paused_ = false;

    bool is_snapshot_ = false;
    CameraSnapshot snapshot_{};
};

} // namespace aom
