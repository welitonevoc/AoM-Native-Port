#include "renderer/camera_track.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <android/log.h>

#define LOG_TAG "AoM_CameraTrack"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace aom {

CameraTrack::CameraTrack()
    : name_("DefaultTrack") {}

CameraTrack::CameraTrack(const std::string& name)
    : name_(name) {}

void CameraTrack::clear() {
    waypoints_.clear();
    duration_ = 0.0f;
    current_time_ = 0.0f;
    is_playing_ = false;
    is_paused_ = false;
    is_snapshot_ = false;
}

void CameraTrack::add_waypoint(const CameraWaypoint& wp) {
    waypoints_.push_back(wp);
    if (wp.time > duration_) {
        duration_ = wp.time;
    }
}

float CameraTrack::duration() const {
    if (duration_ > 0.0f) return duration_;
    if (waypoints_.empty()) return 0.0f;
    return waypoints_.back().time;
}

void CameraTrack::play(float override_duration) {
    if (override_duration > 0.0f) {
        duration_ = override_duration;
    }
    current_time_ = 0.0f;
    is_playing_ = true;
    is_paused_ = false;
    LOGI("CameraTrack '%s' started playback (duration: %.2fs, waypoints: %zu)",
         name_.c_str(), duration(), waypoints_.size());
}

void CameraTrack::pause() {
    if (is_playing_) {
        is_paused_ = true;
    }
}

void CameraTrack::resume() {
    if (is_playing_ && is_paused_) {
        is_paused_ = false;
    }
}

void CameraTrack::stop() {
    is_playing_ = false;
    is_paused_ = false;
    current_time_ = 0.0f;
}

float CameraTrack::progress() const {
    const float dur = duration();
    if (dur <= 0.0001f) return 1.0f;
    return std::clamp(current_time_ / dur, 0.0f, 1.0f);
}

void CameraTrack::set_current_time(float t) {
    current_time_ = std::clamp(t, 0.0f, duration());
}

bool CameraTrack::update(float delta_time, Camera& out_camera) {
    if (!is_playing_ || is_paused_) {
        return is_playing_;
    }

    current_time_ += delta_time;
    const float dur = duration();

    if (current_time_ >= dur) {
        if (loop_ && dur > 0.001f) {
            current_time_ = std::fmod(current_time_, dur);
        } else {
            current_time_ = dur;
            is_playing_ = false;
        }
    }

    out_camera = evaluate(current_time_);
    return is_playing_;
}

// --- Kochanek-Bartels Spline Interpolation ---

glm::vec3 CameraTrack::hermite(const glm::vec3& p0, const glm::vec3& m0,
                               const glm::vec3& p1, const glm::vec3& m1, float t) {
    const float t2 = t * t;
    const float t3 = t2 * t;

    const float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
    const float h10 = t3 - 2.0f * t2 + t;
    const float h01 = -2.0f * t3 + 3.0f * t2;
    const float h11 = t3 - t2;

    return h00 * p0 + h10 * m0 + h01 * p1 + h11 * m1;
}

glm::vec3 CameraTrack::interpolate_tcb(
    const glm::vec3& p_prev, const glm::vec3& p_curr,
    const glm::vec3& p_next, const glm::vec3& p_post,
    float t_param,
    float tension, float continuity, float bias) {

    // Kochanek-Bartels tangents at p_curr (outgoing) and p_next (incoming)
    // T = Tension [-1, 1], C = Continuity [-1, 1], B = Bias [-1, 1]
    const float T = tension;
    const float C = continuity;
    const float B = bias;

    // Outgoing tangent at p_curr
    const glm::vec3 d_out = 0.5f * ((1.0f - T) * (1.0f + C) * (1.0f + B) * (p_curr - p_prev) +
                                    (1.0f - T) * (1.0f - C) * (1.0f - B) * (p_next - p_curr));

    // Incoming tangent at p_next
    const glm::vec3 d_in = 0.5f * ((1.0f - T) * (1.0f - C) * (1.0f + B) * (p_next - p_curr) +
                                   (1.0f - T) * (1.0f + C) * (1.0f - B) * (p_post - p_next));

    return hermite(p_curr, d_out, p_next, d_in, t_param);
}

Camera CameraTrack::evaluate(float time_seconds) const {
    Camera cam;

    // If loaded from a static "CM" snapshot
    if (is_snapshot_) {
        cam.position = snapshot_.position;
        cam.target = snapshot_.position + snapshot_.forward * 10.0f;
        cam.up = snapshot_.up;
        cam.fov = snapshot_.fov;
        cam.near_plane = snapshot_.near_plane;
        cam.far_plane = snapshot_.far_plane;
        cam.aspect = snapshot_.aspect;
        return cam;
    }

    if (waypoints_.empty()) {
        return cam;
    }

    if (waypoints_.size() == 1 || time_seconds <= waypoints_.front().time) {
        const auto& wp = waypoints_.front();
        cam.position = wp.position;
        cam.target = wp.target;
        cam.up = wp.up;
        cam.fov = wp.fov;
        return cam;
    }

    if (time_seconds >= waypoints_.back().time) {
        const auto& wp = waypoints_.back();
        cam.position = wp.position;
        cam.target = wp.target;
        cam.up = wp.up;
        cam.fov = wp.fov;
        return cam;
    }

    // Locate waypoint segment [idx, idx+1]
    size_t idx = 0;
    for (size_t i = 0; i + 1 < waypoints_.size(); ++i) {
        if (time_seconds >= waypoints_[i].time && time_seconds <= waypoints_[i + 1].time) {
            idx = i;
            break;
        }
    }

    const auto& wp0 = waypoints_[idx];
    const auto& wp1 = waypoints_[idx + 1];

    const float segment_dur = wp1.time - wp0.time;
    const float t_norm = (segment_dur > 0.0001f) ? (time_seconds - wp0.time) / segment_dur : 0.0f;

    // Surrounding points for spline boundary conditions
    const auto& p_prev = (idx > 0) ? waypoints_[idx - 1].position : (wp0.position * 2.0f - wp1.position);
    const auto& p_curr = wp0.position;
    const auto& p_next = wp1.position;
    const auto& p_post = (idx + 2 < waypoints_.size()) ? waypoints_[idx + 2].position : (wp1.position * 2.0f - wp0.position);

    const auto& t_prev = (idx > 0) ? waypoints_[idx - 1].target : (wp0.target * 2.0f - wp1.target);
    const auto& t_curr = wp0.target;
    const auto& t_next = wp1.target;
    const auto& t_post = (idx + 2 < waypoints_.size()) ? waypoints_[idx + 2].target : (wp1.target * 2.0f - wp0.target);

    // Interpolate position and look-at target using TCB spline
    cam.position = interpolate_tcb(p_prev, p_curr, p_next, p_post, t_norm,
                                   wp0.tension, wp0.continuity, wp0.bias);
    cam.target = interpolate_tcb(t_prev, t_curr, t_next, t_post, t_norm,
                                 wp0.tension, wp0.continuity, wp0.bias);

    // Up vector linear interpolation with re-normalization
    glm::vec3 interp_up = glm::mix(wp0.up, wp1.up, t_norm);
    if (glm::length(interp_up) > 0.0001f) {
        cam.up = glm::normalize(interp_up);
    } else {
        cam.up = glm::vec3(0.0f, 1.0f, 0.0f);
    }

    // FOV linear interpolation
    cam.fov = glm::mix(wp0.fov, wp1.fov, t_norm);

    return cam;
}

// --- Binary .cam Format Parsing ---

bool CameraTrack::load_from_memory(const uint8_t* data, size_t size) {
    if (!data || size < 6) {
        LOGE("CameraTrack::load_from_memory: file too small (%zu bytes)", size);
        return false;
    }

    clear();

    const char tag0 = static_cast<char>(data[0]);
    const char tag1 = static_cast<char>(data[1]);

    uint32_t chunk_len = 0;
    std::memcpy(&chunk_len, data + 2, 4);

    LOGI("CameraTrack tag '%c%c', chunk length: %u bytes (total size: %zu)",
         tag0, tag1, chunk_len, size);

    if (tag0 == 'C' && tag1 == 'M') {
        // "CM" chunk: Camera Snapshot (e.g. supascreen.cam)
        is_snapshot_ = true;
        const size_t payload_len = std::min(static_cast<size_t>(chunk_len), size - 6);
        const uint8_t* p = data + 6;

        if (payload_len >= 144) {
            std::memcpy(&snapshot_.version, p + 0, 4);
            std::memcpy(&snapshot_.position, p + 4, 12);
            std::memcpy(&snapshot_.forward, p + 16, 12);
            std::memcpy(&snapshot_.up, p + 28, 12);
            std::memcpy(&snapshot_.right, p + 40, 12);
            snapshot_.flags = p[52];

            // Target derived from position + forward
            snapshot_.target = snapshot_.position + snapshot_.forward * 20.0f;

            if (payload_len >= 184) {
                std::memcpy(&snapshot_.fov, p + 144, 4);
                std::memcpy(&snapshot_.aspect, p + 148, 4);
                std::memcpy(&snapshot_.near_plane, p + 152, 4);
                std::memcpy(&snapshot_.far_plane, p + 156, 4);
            }

            // Create a default single waypoint from snapshot
            CameraWaypoint wp;
            wp.time = 0.0f;
            wp.position = snapshot_.position;
            wp.target = snapshot_.target;
            wp.up = snapshot_.up;
            wp.fov = snapshot_.fov;
            add_waypoint(wp);

            LOGI("Loaded 'CM' Camera Snapshot: pos(%.2f, %.2f, %.2f), fov=%.1f",
                 snapshot_.position.x, snapshot_.position.y, snapshot_.position.z, snapshot_.fov);
            return true;
        } else {
            LOGE("'CM' chunk too short for camera state (%zu bytes)", payload_len);
            return false;
        }
    } else if (tag0 == 'C' && tag1 == 'T') {
        // "CT" chunk: Multi-waypoint Camera Track
        is_snapshot_ = false;
        const uint8_t* p = data + 6;
        size_t offset = 0;
        const size_t max_off = std::min(static_cast<size_t>(chunk_len), size - 6);

        if (max_off < 12) {
            LOGE("'CT' chunk too small (%zu bytes)", max_off);
            return false;
        }

        uint32_t version = 0;
        std::memcpy(&version, p + offset, 4);
        offset += 4;

        std::memcpy(&duration_, p + offset, 4);
        offset += 4;

        uint32_t num_waypoints = 0;
        std::memcpy(&num_waypoints, p + offset, 4);
        offset += 4;

        LOGI("'CT' Track version %u, duration %.2fs, waypoints: %u",
             version, duration_, num_waypoints);

        for (uint32_t i = 0; i < num_waypoints && offset + 48 <= max_off; ++i) {
            CameraWaypoint wp;
            std::memcpy(&wp.time, p + offset, 4); offset += 4;
            std::memcpy(&wp.position, p + offset, 12); offset += 12;
            std::memcpy(&wp.target, p + offset, 12); offset += 12;
            std::memcpy(&wp.up, p + offset, 12); offset += 12;
            std::memcpy(&wp.fov, p + offset, 4); offset += 4;

            if (offset + 12 <= max_off) {
                std::memcpy(&wp.tension, p + offset, 4); offset += 4;
                std::memcpy(&wp.continuity, p + offset, 4); offset += 4;
                std::memcpy(&wp.bias, p + offset, 4); offset += 4;
            }

            waypoints_.push_back(wp);
        }

        LOGI("Successfully loaded %zu waypoints from 'CT' track", waypoints_.size());
        return !waypoints_.empty();
    }

    LOGE("Unknown camera track chunk tag: '%c%c'", tag0, tag1);
    return false;
}

bool CameraTrack::load_from_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        LOGE("Failed to open camera file: %s", path.c_str());
        return false;
    }

    const auto sz = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(sz);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), sz)) {
        LOGE("Failed to read camera file: %s", path.c_str());
        return false;
    }

    return load_from_memory(buffer.data(), buffer.size());
}

std::vector<uint8_t> CameraTrack::save_to_memory() const {
    std::vector<uint8_t> out;

    // Header: "CT" tag
    out.push_back('C');
    out.push_back('T');

    // Compute payload size:
    // version(4) + duration(4) + num_waypoints(4) + waypoints * 60 bytes
    const uint32_t wp_size = 4 + 12 + 12 + 12 + 4 + 4 + 4 + 4; // 56 bytes
    const uint32_t payload_len = 12 + static_cast<uint32_t>(waypoints_.size()) * wp_size;

    out.resize(6 + payload_len);
    std::memcpy(out.data() + 2, &payload_len, 4);

    uint8_t* p = out.data() + 6;
    size_t off = 0;

    const uint32_t version = 1;
    std::memcpy(p + off, &version, 4); off += 4;
    const float dur = duration();
    std::memcpy(p + off, &dur, 4); off += 4;
    const uint32_t num_wps = static_cast<uint32_t>(waypoints_.size());
    std::memcpy(p + off, &num_wps, 4); off += 4;

    for (const auto& wp : waypoints_) {
        std::memcpy(p + off, &wp.time, 4); off += 4;
        std::memcpy(p + off, &wp.position, 12); off += 12;
        std::memcpy(p + off, &wp.target, 12); off += 12;
        std::memcpy(p + off, &wp.up, 12); off += 12;
        std::memcpy(p + off, &wp.fov, 4); off += 4;
        std::memcpy(p + off, &wp.tension, 4); off += 4;
        std::memcpy(p + off, &wp.continuity, 4); off += 4;
        std::memcpy(p + off, &wp.bias, 4); off += 4;
    }

    return out;
}

bool CameraTrack::save_to_file(const std::string& path) const {
    auto data = save_to_memory();
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) return false;
    out.write(reinterpret_cast<const char*>(data.data()), data.size());
    return true;
}

std::vector<uint8_t> CameraTrack::save_camera_snapshot(const Camera& cam, const CameraSnapshot& snap) {
    std::vector<uint8_t> out(190, 0);

    // "CM" tag + 184 bytes payload
    out[0] = 'C';
    out[1] = 'M';
    const uint32_t len = 184;
    std::memcpy(out.data() + 2, &len, 4);

    uint8_t* p = out.data() + 6;
    const uint32_t version = 2;
    std::memcpy(p + 0, &version, 4);
    std::memcpy(p + 4, &cam.position, 12);

    glm::vec3 fwd = glm::normalize(cam.target - cam.position);
    glm::vec3 right = glm::normalize(glm::cross(fwd, cam.up));
    glm::vec3 up = glm::cross(right, fwd);

    std::memcpy(p + 16, &fwd, 12);
    std::memcpy(p + 28, &up, 12);
    std::memcpy(p + 40, &right, 12);

    // FOV & projection tuning
    std::memcpy(p + 144, &cam.fov, 4);
    std::memcpy(p + 148, &cam.aspect, 4);
    std::memcpy(p + 152, &cam.near_plane, 4);
    std::memcpy(p + 156, &cam.far_plane, 4);

    return out;
}

} // namespace aom
