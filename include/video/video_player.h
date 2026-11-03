#pragma once

#include "video/video_database_gen.h"
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <cstdint>

namespace aom {
namespace video {

enum class VideoState {
    STOPPED,
    PLAYING,
    PAUSED,
    COMPLETED
};

struct BikHeader {
    char magic[4] = {'\0', '\0', '\0', '\0'}; // "BIKi"
    uint32_t file_size = 0;
    uint32_t num_frames = 0;
    uint32_t largest_frame_size = 0;
    uint32_t frame_count = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t fps_dividend = 0;
    uint32_t fps_divider = 0;
    uint32_t flags = 0;
    uint32_t num_audio_tracks = 0;

    float fps() const {
        return (fps_divider > 0) ? static_cast<float>(fps_dividend) / static_cast<float>(fps_divider) : 30.0f;
    }
    float duration() const {
        return (fps() > 0.0f) ? static_cast<float>(num_frames) / fps() : 0.0f;
    }
    bool is_valid() const {
        return magic[0] == 'B' && magic[1] == 'I' && magic[2] == 'K';
    }
};

class VideoPlayer {
public:
    VideoPlayer();
    ~VideoPlayer() = default;

    // Direct BIK header parsing from disk or memory
    static bool parse_bik_header(const std::string& path, BikHeader& out_header);
    static bool parse_bik_header_memory(const uint8_t* data, size_t size, BikHeader& out_header);

    // Playlist parsing from .aom files
    static std::vector<std::string> parse_playlist_file(const std::string& path);

    // Playback control
    bool play_cinematic(CinematicId id);
    bool play_playlist(PlaylistId id);
    bool play_file(const std::string& filename, float duration = 0.0f, uint32_t width = 640, uint32_t height = 480, float fps = 30.0f);
    void pause();
    void resume();
    void stop();
    void skip();

    void update(float dt);

    // Query state
    VideoState state() const { return state_; }
    bool is_playing() const { return state_ == VideoState::PLAYING; }
    float current_time() const { return current_time_; }
    float total_duration() const { return current_meta_.duration_sec; }
    float progress() const {
        return (current_meta_.duration_sec > 0.0f) ? current_time_ / current_meta_.duration_sec : 0.0f;
    }
    uint32_t current_frame() const { return current_frame_; }
    const CinematicMeta& current_meta() const { return current_meta_; }
    const std::string& current_file() const { return current_file_; }

    // Callbacks
    std::function<void(const std::string&)> on_video_started;
    std::function<void(const std::string&)> on_video_finished;
    std::function<void()> on_playlist_completed;

private:
    void advance_playlist();

    VideoState state_ = VideoState::STOPPED;
    CinematicMeta current_meta_{};
    std::string current_file_;
    float current_time_ = 0.0f;
    uint32_t current_frame_ = 0;

    std::vector<std::string> playlist_queue_;
    size_t playlist_index_ = 0;
};

} // namespace video
} // namespace aom
