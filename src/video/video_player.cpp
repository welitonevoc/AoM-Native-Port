#include "video/video_player.h"
#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <iostream>

namespace aom {
namespace video {

VideoPlayer::VideoPlayer() = default;

bool VideoPlayer::parse_bik_header(const std::string& path, BikHeader& out_header) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    uint8_t buffer[44];
    file.read(reinterpret_cast<char*>(buffer), sizeof(buffer));
    if (file.gcount() < static_cast<std::streamsize>(sizeof(buffer))) return false;

    return parse_bik_header_memory(buffer, sizeof(buffer), out_header);
}

bool VideoPlayer::parse_bik_header_memory(const uint8_t* data, size_t size, BikHeader& out_header) {
    if (!data || size < 44) return false;

    std::memcpy(out_header.magic, data, 4);
    if (!out_header.is_valid()) return false;

    const uint32_t* u32 = reinterpret_cast<const uint32_t*>(data + 4);
    out_header.file_size = u32[0];
    out_header.num_frames = u32[1];
    out_header.largest_frame_size = u32[2];
    out_header.frame_count = u32[3];
    out_header.width = u32[4];
    out_header.height = u32[5];
    out_header.fps_dividend = u32[6];
    out_header.fps_divider = u32[7];
    out_header.flags = u32[8];
    out_header.num_audio_tracks = u32[9];

    return true;
}

std::vector<std::string> VideoPlayer::parse_playlist_file(const std::string& path) {
    std::vector<std::string> results;
    std::ifstream file(path);
    if (!file.is_open()) return results;

    std::string line;
    while (std::getline(file, line)) {
        // Strip trailing \r and whitespace
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) {
            line.pop_back();
        }
        // Normalize slashes
        for (char& c : line) {
            if (c == '\\') c = '/';
        }
        if (!line.empty()) {
            results.push_back(line);
        }
    }
    return results;
}

bool VideoPlayer::play_cinematic(CinematicId id) {
    for (const auto& meta : kAllCinematics) {
        if (meta.id == id) {
            playlist_queue_.clear();
            playlist_index_ = 0;
            current_meta_ = meta;
            current_file_ = std::string(meta.original_bik);
            current_time_ = 0.0f;
            current_frame_ = 0;
            state_ = VideoState::PLAYING;
            if (on_video_started) on_video_started(current_file_);
            return true;
        }
    }
    return false;
}

bool VideoPlayer::play_playlist(PlaylistId id) {
    for (const auto& pl : kAllPlaylists) {
        if (pl.id == id) {
            playlist_queue_.clear();
            for (size_t i = 0; i < pl.file_count; ++i) {
                if (!pl.mp4_files[i].empty()) {
                    playlist_queue_.emplace_back(pl.mp4_files[i]);
                }
            }
            playlist_index_ = 0;
            advance_playlist();
            return true;
        }
    }
    return false;
}

bool VideoPlayer::play_file(const std::string& filename, float duration, uint32_t width, uint32_t height, float fps) {
    playlist_queue_.clear();
    playlist_index_ = 0;

    // Check if known cinematic
    bool matched = false;
    for (const auto& meta : kAllCinematics) {
        if (filename.find(meta.original_bik) != std::string::npos ||
            filename.find(meta.converted_mp4) != std::string::npos) {
            current_meta_ = meta;
            matched = true;
            break;
        }
    }

    if (!matched) {
        current_meta_.id = CinematicId::COUNT;
        current_meta_.original_bik = "";
        current_meta_.converted_mp4 = "";
        current_meta_.title = "Video Clip";
        current_meta_.width = width;
        current_meta_.height = height;
        current_meta_.fps = (fps > 0.0f) ? fps : 30.0f;
        current_meta_.duration_sec = (duration > 0.0f) ? duration : 10.0f;
        current_meta_.num_frames = static_cast<uint32_t>(current_meta_.duration_sec * current_meta_.fps);
        current_meta_.audio_tracks = 1;
        current_meta_.file_size = 0;
    }

    current_file_ = filename;
    current_time_ = 0.0f;
    current_frame_ = 0;
    state_ = VideoState::PLAYING;
    if (on_video_started) on_video_started(current_file_);
    return true;
}

void VideoPlayer::pause() {
    if (state_ == VideoState::PLAYING) {
        state_ = VideoState::PAUSED;
    }
}

void VideoPlayer::resume() {
    if (state_ == VideoState::PAUSED) {
        state_ = VideoState::PLAYING;
    }
}

void VideoPlayer::stop() {
    state_ = VideoState::STOPPED;
    current_time_ = 0.0f;
    current_frame_ = 0;
    playlist_queue_.clear();
    playlist_index_ = 0;
}

void VideoPlayer::skip() {
    if (state_ == VideoState::STOPPED || state_ == VideoState::COMPLETED) return;

    if (on_video_finished) {
        on_video_finished(current_file_);
    }

    if (playlist_index_ + 1 < playlist_queue_.size()) {
        playlist_index_++;
        advance_playlist();
    } else {
        state_ = VideoState::COMPLETED;
        if (on_playlist_completed) {
            on_playlist_completed();
        }
    }
}

void VideoPlayer::advance_playlist() {
    if (playlist_index_ < playlist_queue_.size()) {
        const std::string& next_file = playlist_queue_[playlist_index_];
        play_file(next_file);
    } else {
        state_ = VideoState::COMPLETED;
        if (on_playlist_completed) {
            on_playlist_completed();
        }
    }
}

void VideoPlayer::update(float dt) {
    if (state_ != VideoState::PLAYING) return;

    current_time_ += dt;
    current_frame_ = static_cast<uint32_t>(current_time_ * current_meta_.fps);

    if (current_time_ >= current_meta_.duration_sec) {
        current_time_ = current_meta_.duration_sec;
        current_frame_ = current_meta_.num_frames;
        skip();
    }
}

} // namespace video
} // namespace aom
