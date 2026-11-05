#include "audio/audio.h"
#include "assets/bar_archive.h"

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <android/asset_manager.h>
#include <android/log.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#define LOG_TAG "AoM-Audio"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace aom {

namespace {

constexpr size_t BUFFER_FRAMES = 1024;    // 1024 frames @ 22050 Hz = ~46.4ms
constexpr uint32_t MASTER_SAMPLE_RATE = 22050;
constexpr size_t MAX_VOICES = 32;

struct WavData {
    uint32_t sample_rate = 22050;
    uint16_t channels = 1;
    uint16_t bits_per_sample = 16;
    std::vector<int16_t> samples;
    size_t total_frames = 0;
};

bool parse_wav_memory(const uint8_t* data, size_t size, WavData& out) {
    if (!data || size < 44) return false;
    if (std::memcmp(data, "RIFF", 4) != 0) return false;
    if (std::memcmp(data + 8, "WAVE", 4) != 0) return false;

    size_t pos = 12;
    bool found_fmt = false;
    bool found_data = false;

    while (pos + 8 <= size) {
        char chunk_id[4];
        std::memcpy(chunk_id, data + pos, 4);
        uint32_t chunk_len = *reinterpret_cast<const uint32_t*>(data + pos + 4);
        pos += 8;

        if (std::memcmp(chunk_id, "fmt ", 4) == 0 && pos + 16 <= size) {
            uint16_t format_tag = *reinterpret_cast<const uint16_t*>(data + pos);
            if (format_tag != 1) return false; // PCM only
            out.channels = *reinterpret_cast<const uint16_t*>(data + pos + 2);
            out.sample_rate = *reinterpret_cast<const uint32_t*>(data + pos + 4);
            out.bits_per_sample = *reinterpret_cast<const uint16_t*>(data + pos + 14);
            found_fmt = true;
        } else if (std::memcmp(chunk_id, "data", 4) == 0) {
            size_t to_copy = std::min<size_t>(chunk_len, size - pos);
            if (out.bits_per_sample == 16) {
                size_t count = to_copy / sizeof(int16_t);
                out.samples.resize(count);
                std::memcpy(out.samples.data(), data + pos, count * sizeof(int16_t));
            } else if (out.bits_per_sample == 8) {
                out.samples.resize(to_copy);
                for (size_t i = 0; i < to_copy; ++i) {
                    uint8_t u = data[pos + i];
                    out.samples[i] = static_cast<int16_t>((static_cast<int>(u) - 128) << 8);
                }
                out.bits_per_sample = 16;
            }
            out.total_frames = (out.channels > 0) ? (out.samples.size() / out.channels) : 0;
            found_data = true;
            break;
        }
        pos += chunk_len;
    }

    return found_fmt && found_data && !out.samples.empty();
}

bool load_wav_file(const std::string& path, WavData& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    f.seekg(0, std::ios::end);
    size_t sz = static_cast<size_t>(f.tellg());
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(sz);
    f.read(reinterpret_cast<char*>(buffer.data()), sz);
    return parse_wav_memory(buffer.data(), buffer.size(), out);
}

bool load_wav_asset(AAssetManager* mgr, const std::string& path, WavData& out) {
    if (!mgr) return false;
    AAsset* asset = AAssetManager_open(mgr, path.c_str(), AASSET_MODE_BUFFER);
    if (!asset) return false;
    off_t len = AAsset_getLength(asset);
    std::vector<uint8_t> buffer(static_cast<size_t>(len));
    AAsset_read(asset, buffer.data(), len);
    AAsset_close(asset);
    return parse_wav_memory(buffer.data(), buffer.size(), out);
}

SLmillibel volume_to_millibels(float volume) {
    if (volume <= 0.0001f) return SL_MILLIBEL_MIN;
    if (volume >= 1.0f) return 0;
    float db = 20.0f * std::log10(volume);
    SLmillibel mb = static_cast<SLmillibel>(db * 100.0f);
    return std::max<SLmillibel>(SL_MILLIBEL_MIN, std::min<SLmillibel>(0, mb));
}

std::string get_clean_basename(const std::string& path) {
    size_t last_slash = path.find_last_of("/\\");
    if (last_slash != std::string::npos) {
        return path.substr(last_slash + 1);
    }
    return path;
}

} // anonymous namespace

class OpenSLEngine : public AudioEngine {
public:
    OpenSLEngine() {
        buffer_a_.resize(BUFFER_FRAMES * 2, 0);
        buffer_b_.resize(BUFFER_FRAMES * 2, 0);
    }

    ~OpenSLEngine() override {
        shutdown();
    }

    bool initialize(AAssetManager* asset_mgr) override {
        asset_mgr_ = asset_mgr;

        SLresult res = slCreateEngine(&engine_obj_, 0, nullptr, 0, nullptr, nullptr);
        if (res != SL_RESULT_SUCCESS) {
            LOGI("slCreateEngine unavailable (code 0x%x), operating in software audio mode", res);
            has_opensl_device_ = false;
            preload_sounds();
            return true;
        }

        res = (*engine_obj_)->Realize(engine_obj_, SL_BOOLEAN_FALSE);
        if (res != SL_RESULT_SUCCESS) {
            LOGE("engine Realize failed: 0x%x", res);
            has_opensl_device_ = false;
            preload_sounds();
            return true;
        }

        res = (*engine_obj_)->GetInterface(engine_obj_, SL_IID_ENGINE, &engine_);
        if (res != SL_RESULT_SUCCESS) {
            LOGE("GetInterface SL_IID_ENGINE failed: 0x%x", res);
            has_opensl_device_ = false;
            preload_sounds();
            return true;
        }

        const SLInterfaceID ids[1] = { SL_IID_VOLUME };
        const SLboolean req[1] = { SL_BOOLEAN_FALSE };
        res = (*engine_)->CreateOutputMix(engine_, &output_mix_, 1, ids, req);
        if (res != SL_RESULT_SUCCESS) {
            LOGE("CreateOutputMix failed: 0x%x", res);
            has_opensl_device_ = false;
            preload_sounds();
            return true;
        }

        res = (*output_mix_)->Realize(output_mix_, SL_BOOLEAN_FALSE);
        if (res != SL_RESULT_SUCCESS) {
            LOGE("output_mix Realize failed: 0x%x", res);
            has_opensl_device_ = false;
            preload_sounds();
            return true;
        }

        has_opensl_device_ = true;
        init_mixer_player();
        preload_sounds();

        LOGI("OpenSL ES 3D positional audio engine initialized successfully");
        return true;
    }

    void shutdown() override {
        stop_music();
        destroy_mixer_player();

        if (output_mix_) {
            (*output_mix_)->Destroy(output_mix_);
            output_mix_ = nullptr;
        }
        if (engine_obj_) {
            (*engine_obj_)->Destroy(engine_obj_);
            engine_obj_ = nullptr;
            engine_ = nullptr;
        }

        std::lock_guard<std::mutex> lock(audio_mutex_);
        active_voices_.clear();
        wav_cache_.clear();
        LOGI("Audio engine shut down");
    }

    void play_music(const std::string& path, bool loop) override {
        if (!music_enabled_ || !engine_) return;

        stop_music();
        current_music_path_ = path;

        std::string resolved_path = path;
        std::ifstream test_f(resolved_path);
        if (!test_f) {
            const std::string fallbacks[] = {
                "/storage/emulated/0/Documents/AoM-Native-Port/" + path,
                "/storage/emulated/0/Documents/AoM-Native-Port/assets/" + path,
                "/storage/emulated/0/Documents/AoM/sound/music/interface/a cat named mittens.mp3",
                "/storage/emulated/0/Download/Games/PC/Age of Mythology/Age of Mythology/sound/music/interface/a cat named mittens.mp3"
            };
            for (const auto& fb : fallbacks) {
                std::ifstream f(fb);
                if (f) {
                    resolved_path = fb;
                    break;
                }
            }
        }

        LOGI("Playing background music: %s", resolved_path.c_str());

        SLDataLocator_URI loc_uri = { SL_DATALOCATOR_URI, reinterpret_cast<SLchar*>(const_cast<char*>(resolved_path.c_str())) };
        SLDataFormat_MIME format_mime = { SL_DATAFORMAT_MIME, nullptr, SL_CONTAINERTYPE_UNSPECIFIED };
        SLDataSource audio_src = { &loc_uri, &format_mime };

        SLDataLocator_OutputMix loc_outmix = { SL_DATALOCATOR_OUTPUTMIX, output_mix_ };
        SLDataSink audio_snk = { &loc_outmix, nullptr };

        const SLInterfaceID ids[3] = { SL_IID_PLAY, SL_IID_SEEK, SL_IID_VOLUME };
        const SLboolean req[3] = { SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE };

        SLresult res = (*engine_)->CreateAudioPlayer(engine_, &music_player_obj_, &audio_src, &audio_snk, 3, ids, req);
        if (res != SL_RESULT_SUCCESS) {
            LOGE("CreateAudioPlayer for music failed: 0x%x", res);
            return;
        }

        res = (*music_player_obj_)->Realize(music_player_obj_, SL_BOOLEAN_FALSE);
        if (res != SL_RESULT_SUCCESS) {
            LOGE("music_player Realize failed: 0x%x", res);
            destroy_music_player();
            return;
        }

        (*music_player_obj_)->GetInterface(music_player_obj_, SL_IID_PLAY, &music_play_);
        (*music_player_obj_)->GetInterface(music_player_obj_, SL_IID_SEEK, &music_seek_);
        (*music_player_obj_)->GetInterface(music_player_obj_, SL_IID_VOLUME, &music_volume_itf_);

        if (music_seek_ && loop) {
            (*music_seek_)->SetLoop(music_seek_, SL_BOOLEAN_TRUE, 0, SL_TIME_UNKNOWN);
        }

        if (music_volume_itf_) {
            (*music_volume_itf_)->SetVolumeLevel(music_volume_itf_, volume_to_millibels(music_volume_));
        }

        if (music_play_) {
            (*music_play_)->SetPlayState(music_play_, SL_PLAYSTATE_PLAYING);
        }
    }

    void stop_music() override {
        destroy_music_player();
    }

    void set_music_volume(float volume) override {
        music_volume_ = std::clamp(volume, 0.0f, 1.0f);
        if (music_volume_itf_) {
            (*music_volume_itf_)->SetVolumeLevel(music_volume_itf_, volume_to_millibels(music_enabled_ ? music_volume_ : 0.0f));
        }
    }

    void set_music_enabled(bool enabled) override {
        music_enabled_ = enabled;
        if (!enabled) {
            if (music_play_) (*music_play_)->SetPlayState(music_play_, SL_PLAYSTATE_PAUSED);
        } else {
            if (music_play_) {
                set_music_volume(music_volume_);
                (*music_play_)->SetPlayState(music_play_, SL_PLAYSTATE_PLAYING);
            } else if (!current_music_path_.empty()) {
                play_music(current_music_path_, true);
            }
        }
    }

    void play_sfx(const std::string& path, float volume) override {
        if (!sfx_enabled_) return;
        const WavData* wav = get_or_load_wav(path);
        if (!wav || wav->samples.empty()) {
            play_click();
            return;
        }

        std::lock_guard<std::mutex> lock(audio_mutex_);
        enqueue_voice(wav, volume * sfx_volume_, volume * sfx_volume_, false, false, glm::vec3(0.0f));
    }

    void play_click() override {
        if (!sfx_enabled_ || click_pcm_.samples.empty()) return;
        std::lock_guard<std::mutex> lock(audio_mutex_);
        enqueue_voice(&click_pcm_, sfx_volume_, sfx_volume_, false, false, glm::vec3(0.0f));
    }

    void play_big_button() override {
        if (!sfx_enabled_ || big_button_pcm_.samples.empty()) return;
        std::lock_guard<std::mutex> lock(audio_mutex_);
        enqueue_voice(&big_button_pcm_, sfx_volume_, sfx_volume_, false, false, glm::vec3(0.0f));
    }

    void play_unit_voice(const std::string& category, bool worker) override {
        play_unit_voice_3d(category, listener_pos_, "greek", worker);
    }

    void set_sfx_volume(float volume) override {
        sfx_volume_ = std::clamp(volume, 0.0f, 1.0f);
    }

    void set_sfx_enabled(bool enabled) override {
        sfx_enabled_ = enabled;
        if (!enabled) {
            std::lock_guard<std::mutex> lock(audio_mutex_);
            active_voices_.clear();
        }
    }

    bool is_music_enabled() const override { return music_enabled_; }
    bool is_sfx_enabled() const override { return sfx_enabled_; }
    float music_volume() const override { return music_volume_; }
    float sfx_volume() const override { return sfx_volume_; }

    // === Sprint 6: 3D Positional Audio & Authentic AoM Audio System ===

    void set_listener(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& right) override {
        listener_pos_ = position;
        listener_forward_ = (glm::length(forward) > 0.001f) ? glm::normalize(forward) : glm::vec3(0.0f, 0.0f, 1.0f);
        listener_right_ = (glm::length(right) > 0.001f) ? glm::normalize(right) : glm::vec3(1.0f, 0.0f, 0.0f);

        // Update spatial panning for active 3D sounds
        std::lock_guard<std::mutex> lock(audio_mutex_);
        for (auto& v : active_voices_) {
            if (v.active && v.is_3d) {
                calculate_3d_spatial(v.world_pos, v.base_volume, v.min_dist, v.max_dist, v.gain_l, v.gain_r);
            }
        }
    }

    glm::vec3 listener_position() const override {
        return listener_pos_;
    }

    void play_sfx_3d(const std::string& path, const glm::vec3& world_pos, float volume,
                     float min_distance, float max_distance) override {
        if (!sfx_enabled_) return;

        float gl = 0.0f, gr = 0.0f;
        calculate_3d_spatial(world_pos, volume, min_distance, max_distance, gl, gr);
        if (gl <= 0.0001f && gr <= 0.0001f) return; // Inaudible

        const WavData* wav = get_or_load_wav(path);
        if (!wav || wav->samples.empty()) return;

        std::lock_guard<std::mutex> lock(audio_mutex_);
        enqueue_voice(wav, gl, gr, false, true, world_pos, volume, min_distance, max_distance);
    }

    void play_unit_voice_3d(const std::string& category, const glm::vec3& world_pos,
                            const std::string& unit_type_name, bool worker) override {
        if (!sfx_enabled_) return;

        std::string name_lower = unit_type_name;
        std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);

        static int counter = 0;
        counter = (counter + 1) % 1000;

        std::string cue;
        if (name_lower.find("arkantos") != std::string::npos) {
            if (category == "attack") {
                cue = "sound/arkantosattack1.wav";
            } else {
                cue = "sound/arkantosselect1.wav";
            }
        } else if (name_lower.find("kraken") != std::string::npos) {
            cue = "sound/krakenacknowledge1.wav";
        } else {
            // Authentic Greek voices
            if (category == "select") {
                if (worker) {
                    int n = (counter % 4) + 1;
                    cue = "sound/gvms" + std::to_string(n) + ".wav";
                } else {
                    int n = (counter % 3) + 1;
                    cue = "sound/gms" + std::to_string(n) + ".wav";
                }
            } else if (category == "move") {
                if (worker) {
                    int n = (counter % 4) + 1;
                    cue = "sound/gvmm" + std::to_string(n) + ".wav";
                } else {
                    int n = (counter % 3) + 1;
                    cue = "sound/gmm" + std::to_string(n) + ".wav";
                }
            } else if (category == "attack") {
                int n = (counter % 4) + 1;
                cue = "sound/gma" + std::to_string(n) + ".wav";
            } else {
                cue = "sound/gms1.wav";
            }
        }

        play_sfx_3d(cue, world_pos, 1.0f, 6.0f, 60.0f);
    }

    void play_combat_hit(const glm::vec3& world_pos, bool is_metal) override {
        static int hit_idx = 0;
        hit_idx = (hit_idx + 1) % 2;
        std::string sound_file = is_metal
            ? (hit_idx == 0 ? "sound/swordonmetal1.wav" : "sound/swordonmetal2.wav")
            : (hit_idx == 0 ? "sound/swordonflesh1.wav" : "sound/swordonflesh2.wav");
        play_sfx_3d(sound_file, world_pos, 0.95f, 4.0f, 50.0f);
    }

    void play_arrow_shot(const glm::vec3& world_pos) override {
        play_sfx_3d("sound/arrow1.wav", world_pos, 0.85f, 4.0f, 45.0f);
    }

    void play_arrow_hit(const glm::vec3& world_pos) override {
        play_sfx_3d("sound/arrowonflesh1.wav", world_pos, 0.90f, 4.0f, 45.0f);
    }

    void play_harvest_sound(const glm::vec3& world_pos, const std::string& resource_type) override {
        static int h_idx = 0;
        h_idx = (h_idx + 1) % 2;
        std::string file;
        std::string res_lower = resource_type;
        std::transform(res_lower.begin(), res_lower.end(), res_lower.begin(), ::tolower);

        if (res_lower.find("wood") != std::string::npos || res_lower.find("madeira") != std::string::npos) {
            file = (h_idx == 0) ? "sound/woodchop1.wav" : "sound/woodchop2.wav";
        } else if (res_lower.find("gold") != std::string::npos || res_lower.find("ouro") != std::string::npos) {
            file = (h_idx == 0) ? "sound/mine1.wav" : "sound/mine2.wav";
        } else {
            file = (h_idx == 0) ? "sound/forage1.wav" : "sound/meatgather1.wav";
        }
        play_sfx_3d(file, world_pos, 0.90f, 4.0f, 50.0f);
    }

    void play_building_work(const glm::vec3& world_pos) override {
        static int b_idx = 0;
        b_idx = (b_idx + 1) % 2;
        std::string file = (b_idx == 0) ? "sound/build1.wav" : "sound/build2.wav";
        play_sfx_3d(file, world_pos, 0.85f, 5.0f, 55.0f);
    }

    void play_unit_death(const glm::vec3& world_pos, bool female) override {
        std::string file = female ? "sound/femaledeath1.wav" : "sound/edeath1.wav";
        play_sfx_3d(file, world_pos, 1.0f, 6.0f, 60.0f);
    }

    void set_sound_archive(BarArchive* archive) override {
        sound_bar_ = archive;
    }

    size_t active_voice_count() const override {
        std::lock_guard<std::mutex> lock(audio_mutex_);
        return active_voices_.size();
    }

    size_t dispatched_voice_count() const override {
        std::lock_guard<std::mutex> lock(audio_mutex_);
        return dispatched_voices_;
    }

    // Public method for automated testing & software mixer step
    void mix_audio_frames(int16_t* out_stereo, size_t num_frames) {
        if (!out_stereo || num_frames == 0) return;
        std::fill(out_stereo, out_stereo + num_frames * 2, 0);

        std::lock_guard<std::mutex> lock(audio_mutex_);
        if (!sfx_enabled_ || active_voices_.empty()) return;

        for (auto& v : active_voices_) {
            if (!v.active || !v.wav || v.wav->samples.empty()) continue;

            const auto& wav = *v.wav;
            const size_t total_frames = wav.total_frames;
            if (total_frames == 0) {
                v.active = false;
                continue;
            }

            const float gl = v.gain_l;
            const float gr = v.gain_r;
            const bool is_stereo = (wav.channels == 2);

            for (size_t f = 0; f < num_frames; ++f) {
                size_t idx0 = static_cast<size_t>(v.sample_cursor);
                if (idx0 >= total_frames) {
                    if (v.loop) {
                        v.sample_cursor = 0.0;
                        idx0 = 0;
                    } else {
                        v.active = false;
                        break;
                    }
                }

                size_t idx1 = (idx0 + 1 < total_frames) ? (idx0 + 1) : (v.loop ? 0 : idx0);
                float frac = static_cast<float>(v.sample_cursor - idx0);

                float s_l = 0.0f;
                float s_r = 0.0f;

                if (is_stereo) {
                    float l0 = wav.samples[idx0 * 2];
                    float l1 = wav.samples[idx1 * 2];
                    float r0 = wav.samples[idx0 * 2 + 1];
                    float r1 = wav.samples[idx1 * 2 + 1];
                    s_l = l0 + frac * (l1 - l0);
                    s_r = r0 + frac * (r1 - r0);
                } else {
                    float s0 = wav.samples[idx0];
                    float s1 = wav.samples[idx1];
                    float s = s0 + frac * (s1 - s0);
                    s_l = s;
                    s_r = s;
                }

                int32_t cur_l = out_stereo[f * 2];
                int32_t cur_r = out_stereo[f * 2 + 1];

                cur_l += static_cast<int32_t>(s_l * gl);
                cur_r += static_cast<int32_t>(s_r * gr);

                out_stereo[f * 2]     = static_cast<int16_t>(std::clamp<int32_t>(cur_l, -32768, 32767));
                out_stereo[f * 2 + 1] = static_cast<int16_t>(std::clamp<int32_t>(cur_r, -32768, 32767));

                v.sample_cursor += v.cursor_step;
            }
        }

        active_voices_.erase(
            std::remove_if(active_voices_.begin(), active_voices_.end(),
                           [](const Voice& v) { return !v.active; }),
            active_voices_.end());
    }

private:
    struct Voice {
        const WavData* wav = nullptr;
        double sample_cursor = 0.0;
        double cursor_step = 1.0;
        float gain_l = 1.0f;
        float gain_r = 1.0f;
        bool active = false;
        bool loop = false;
        bool is_3d = false;
        glm::vec3 world_pos{0.0f};
        float base_volume = 1.0f;
        float min_dist = 5.0f;
        float max_dist = 65.0f;
    };

    void calculate_3d_spatial(const glm::vec3& world_pos, float base_vol, float min_dist, float max_dist,
                              float& gain_l, float& gain_r) const {
        glm::vec3 diff = world_pos - listener_pos_;
        float dist = glm::length(diff);

        // Distance attenuation
        float atten = 0.0f;
        if (dist <= min_dist) {
            atten = 1.0f;
        } else if (dist < max_dist) {
            float norm_d = (dist - min_dist) / (max_dist - min_dist);
            float roll_off = min_dist / (min_dist + (dist - min_dist) * 0.85f);
            float fade = (1.0f - norm_d);
            atten = roll_off * (fade * fade);
        } else {
            atten = 0.0f;
        }

        if (atten <= 0.0001f) {
            gain_l = 0.0f;
            gain_r = 0.0f;
            return;
        }

        // Horizontal panning
        float pan = 0.0f;
        glm::vec2 diff_2d(diff.x, diff.z);
        glm::vec2 right_2d(listener_right_.x, listener_right_.z);
        float len_d = glm::length(diff_2d);
        float len_r = glm::length(right_2d);

        if (len_d > 0.05f && len_r > 0.05f) {
            float dot = glm::dot(diff_2d / len_d, right_2d / len_r);
            float pan_span = std::clamp(len_d / 20.0f, 0.25f, 1.0f);
            pan = std::clamp(dot * pan_span, -0.85f, 0.85f);
        }

        // Constant-power panning
        float angle = (pan + 1.0f) * 0.25f * 3.14159265f; // [0, pi/2]
        float p_l = std::cos(angle);
        float p_r = std::sin(angle);

        gain_l = base_vol * atten * p_l * sfx_volume_;
        gain_r = base_vol * atten * p_r * sfx_volume_;
    }

    void enqueue_voice(const WavData* wav, float gl, float gr, bool loop,
                       bool is_3d, const glm::vec3& world_pos,
                       float base_vol = 1.0f, float min_d = 5.0f, float max_d = 65.0f) {
        if (!wav || wav->samples.empty()) return;

        if (active_voices_.size() >= MAX_VOICES) {
            auto quietest = std::min_element(
                active_voices_.begin(), active_voices_.end(),
                [](const Voice& a, const Voice& b) {
                    return (a.gain_l + a.gain_r) < (b.gain_l + b.gain_r);
                });
            if (quietest != active_voices_.end()) {
                active_voices_.erase(quietest);
            }
        }

        Voice v;
        v.wav = wav;
        v.sample_cursor = 0.0;
        v.cursor_step = static_cast<double>(wav->sample_rate) / static_cast<double>(MASTER_SAMPLE_RATE);
        v.gain_l = gl;
        v.gain_r = gr;
        v.active = true;
        v.loop = loop;
        v.is_3d = is_3d;
        v.world_pos = world_pos;
        v.base_volume = base_vol;
        v.min_dist = min_d;
        v.max_dist = max_d;

        active_voices_.push_back(v);
        ++dispatched_voices_;
    }

    const WavData* get_or_load_wav(const std::string& path) {
        if (path.empty()) return nullptr;

        std::string clean = path;
        auto it = wav_cache_.find(clean);
        if (it != wav_cache_.end()) {
            return &it->second;
        }

        std::string base = get_clean_basename(path);
        it = wav_cache_.find(base);
        if (it != wav_cache_.end()) {
            return &it->second;
        }

        // Try direct file paths
        const std::string search_paths[] = {
            clean,
            "assets/" + clean,
            "assets/sound/" + base,
            "/storage/emulated/0/Documents/AoM-Native-Port/" + clean,
            "/storage/emulated/0/Documents/AoM-Native-Port/assets/sound/" + base,
            "/storage/emulated/0/Documents/AoM/sound/" + base,
            "/storage/emulated/0/Documents/AoM/sound/ui/" + base,
            "/storage/emulated/0/Download/Games/PC/Age of Mythology/Age of Mythology/sound/" + base
        };

        WavData loaded;
        for (const auto& p : search_paths) {
            if (load_wav_file(p, loaded)) {
                auto inserted = wav_cache_.emplace(clean, std::move(loaded));
                wav_cache_.emplace(base, inserted.first->second);
                return &inserted.first->second;
            }
        }

        // Try asset manager
        if (asset_mgr_) {
            if (load_wav_asset(asset_mgr_, clean, loaded) || load_wav_asset(asset_mgr_, "sound/" + base, loaded)) {
                auto inserted = wav_cache_.emplace(clean, std::move(loaded));
                wav_cache_.emplace(base, inserted.first->second);
                return &inserted.first->second;
            }
        }

        // Try sound archive BAR
        if (sound_bar_) {
            std::vector<uint8_t> data = sound_bar_->extract(clean);
            if (data.empty()) {
                data = sound_bar_->extract(base);
            }
            if (!data.empty() && parse_wav_memory(data.data(), data.size(), loaded)) {
                auto inserted = wav_cache_.emplace(clean, std::move(loaded));
                wav_cache_.emplace(base, inserted.first->second);
                return &inserted.first->second;
            }
        }

        return nullptr;
    }

    void preload_sounds() {
        const std::string click_paths[] = {
            "assets/click.wav",
            "/storage/emulated/0/Documents/AoM-Native-Port/assets/click.wav",
            "/storage/emulated/0/Documents/AoM/sound/ui/click.wav",
            "/storage/emulated/0/Download/Games/PC/Age of Mythology/Age of Mythology/sound/ui/click.wav"
        };
        for (const auto& p : click_paths) {
            if (load_wav_file(p, click_pcm_)) {
                LOGI("Preloaded click sound from: %s", p.c_str());
                break;
            }
        }
        if (click_pcm_.samples.empty() && asset_mgr_) {
            load_wav_asset(asset_mgr_, "click.wav", click_pcm_);
        }
        if (!click_pcm_.samples.empty()) {
            wav_cache_.emplace("click.wav", click_pcm_);
            wav_cache_.emplace("assets/click.wav", click_pcm_);
        }

        const std::string big_paths[] = {
            "assets/bigbutton.wav",
            "/storage/emulated/0/Documents/AoM-Native-Port/assets/bigbutton.wav",
            "/storage/emulated/0/Documents/AoM/sound/ui/bigbutton.wav",
            "/storage/emulated/0/Download/Games/PC/Age of Mythology/Age of Mythology/sound/ui/bigbutton.wav"
        };
        for (const auto& p : big_paths) {
            if (load_wav_file(p, big_button_pcm_)) {
                LOGI("Preloaded bigbutton sound from: %s", p.c_str());
                break;
            }
        }
        if (big_button_pcm_.samples.empty() && asset_mgr_) {
            load_wav_asset(asset_mgr_, "bigbutton.wav", big_button_pcm_);
        }
        if (!big_button_pcm_.samples.empty()) {
            wav_cache_.emplace("bigbutton.wav", big_button_pcm_);
            wav_cache_.emplace("assets/bigbutton.wav", big_button_pcm_);
        }
    }

    static void bq_callback(SLAndroidSimpleBufferQueueItf bq, void* context) {
        auto* self = static_cast<OpenSLEngine*>(context);
        self->on_buffer_queue(bq);
    }

    void on_buffer_queue(SLAndroidSimpleBufferQueueItf bq) {
        int16_t* target = (current_buffer_idx_ == 0) ? buffer_a_.data() : buffer_b_.data();
        current_buffer_idx_ = (current_buffer_idx_ == 0) ? 1 : 0;

        mix_audio_frames(target, BUFFER_FRAMES);

        (*bq)->Enqueue(bq, target, static_cast<SLuint32>(BUFFER_FRAMES * 2 * sizeof(int16_t)));
    }

    void init_mixer_player() {
        if (!engine_ || !output_mix_) return;

        SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {
            SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2
        };
        SLDataFormat_PCM format_pcm = {
            SL_DATAFORMAT_PCM,
            2, // Stereo output
            MASTER_SAMPLE_RATE * 1000,
            SL_PCMSAMPLEFORMAT_FIXED_16,
            SL_PCMSAMPLEFORMAT_FIXED_16,
            SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
            SL_BYTEORDER_LITTLEENDIAN
        };
        SLDataSource audio_src = { &loc_bufq, &format_pcm };

        SLDataLocator_OutputMix loc_outmix = { SL_DATALOCATOR_OUTPUTMIX, output_mix_ };
        SLDataSink audio_snk = { &loc_outmix, nullptr };

        const SLInterfaceID ids[2] = { SL_IID_BUFFERQUEUE, SL_IID_VOLUME };
        const SLboolean req[2] = { SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE };

        SLresult res = (*engine_)->CreateAudioPlayer(engine_, &sfx_player_obj_, &audio_src, &audio_snk, 2, ids, req);
        if (res == SL_RESULT_SUCCESS && sfx_player_obj_) {
            (*sfx_player_obj_)->Realize(sfx_player_obj_, SL_BOOLEAN_FALSE);
            (*sfx_player_obj_)->GetInterface(sfx_player_obj_, SL_IID_PLAY, &sfx_play_);
            (*sfx_player_obj_)->GetInterface(sfx_player_obj_, SL_IID_BUFFERQUEUE, &sfx_queue_);
            (*sfx_player_obj_)->GetInterface(sfx_player_obj_, SL_IID_VOLUME, &sfx_volume_itf_);

            if (sfx_queue_) {
                (*sfx_queue_)->RegisterCallback(sfx_queue_, bq_callback, this);

                // Prime the queue with the first two silent buffers
                mix_audio_frames(buffer_a_.data(), BUFFER_FRAMES);
                (*sfx_queue_)->Enqueue(sfx_queue_, buffer_a_.data(), static_cast<SLuint32>(BUFFER_FRAMES * 2 * sizeof(int16_t)));
                mix_audio_frames(buffer_b_.data(), BUFFER_FRAMES);
                (*sfx_queue_)->Enqueue(sfx_queue_, buffer_b_.data(), static_cast<SLuint32>(BUFFER_FRAMES * 2 * sizeof(int16_t)));
            }

            if (sfx_play_) {
                (*sfx_play_)->SetPlayState(sfx_play_, SL_PLAYSTATE_PLAYING);
            }
        }
    }

    void destroy_music_player() {
        if (music_play_) {
            (*music_play_)->SetPlayState(music_play_, SL_PLAYSTATE_STOPPED);
            music_play_ = nullptr;
        }
        music_seek_ = nullptr;
        music_volume_itf_ = nullptr;
        if (music_player_obj_) {
            (*music_player_obj_)->Destroy(music_player_obj_);
            music_player_obj_ = nullptr;
        }
    }

    void destroy_mixer_player() {
        if (sfx_play_) {
            (*sfx_play_)->SetPlayState(sfx_play_, SL_PLAYSTATE_STOPPED);
            sfx_play_ = nullptr;
        }
        sfx_queue_ = nullptr;
        sfx_volume_itf_ = nullptr;
        if (sfx_player_obj_) {
            (*sfx_player_obj_)->Destroy(sfx_player_obj_);
            sfx_player_obj_ = nullptr;
        }
    }

    AAssetManager* asset_mgr_ = nullptr;
    BarArchive* sound_bar_ = nullptr;

    SLObjectItf engine_obj_ = nullptr;
    SLEngineItf engine_ = nullptr;
    SLObjectItf output_mix_ = nullptr;

    // Music
    SLObjectItf music_player_obj_ = nullptr;
    SLPlayItf music_play_ = nullptr;
    SLSeekItf music_seek_ = nullptr;
    SLVolumeItf music_volume_itf_ = nullptr;
    std::string current_music_path_;

    // Master SFX BufferQueue & Mixer
    SLObjectItf sfx_player_obj_ = nullptr;
    SLPlayItf sfx_play_ = nullptr;
    SLAndroidSimpleBufferQueueItf sfx_queue_ = nullptr;
    SLVolumeItf sfx_volume_itf_ = nullptr;

    bool has_opensl_device_ = false;
    int current_buffer_idx_ = 0;
    std::vector<int16_t> buffer_a_;
    std::vector<int16_t> buffer_b_;

    // 3D Spatial Listener Orientation
    glm::vec3 listener_pos_{0.0f, 0.0f, 0.0f};
    glm::vec3 listener_forward_{0.0f, 0.0f, 1.0f};
    glm::vec3 listener_right_{1.0f, 0.0f, 0.0f};

    // Thread-safe voice collection
    mutable std::mutex audio_mutex_;
    std::vector<Voice> active_voices_;
    size_t dispatched_voices_ = 0;  ///< Contador cumulativo de sons disparados
    std::unordered_map<std::string, WavData> wav_cache_;

    WavData click_pcm_;
    WavData big_button_pcm_;

    float music_volume_ = 0.75f;
    float sfx_volume_ = 0.90f;
    bool music_enabled_ = true;
    bool sfx_enabled_ = true;
};

std::unique_ptr<AudioEngine> create_audio_engine() {
    return std::make_unique<OpenSLEngine>();
}

} // namespace aom
