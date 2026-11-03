#include "video/video_player.h"
#include "anim/anim_script.h"
#include "anim/animation_system.h"
#include "assets/bar_archive.h"
#include <iostream>
#include <cassert>
#include <cmath>

using namespace aom;
using namespace aom::video;
using namespace aom::anim;

static bool g_all_passed = true;

static bool check(bool condition, const char* message) {
    std::cout << (condition ? "PASS: " : "FAIL: ") << message << '\n';
    if (!condition) g_all_passed = false;
    return condition;
}

void test_bik_file_headers_and_database() {
    std::cout << "\n--- Testing BIK Video File Headers & Metadata ---" << std::endl;
    const std::string avi_dir = "/storage/emulated/0/Download/Games/PC/Age of Mythology/Age of Mythology/avi/";

    // 1. Verify all 6 cinematics against kAllCinematics
    for (const auto& meta : kAllCinematics) {
        std::string full_path = avi_dir + std::string(meta.original_bik);
        BikHeader header;
        bool ok = VideoPlayer::parse_bik_header(full_path, header);
        std::cout << "Inspecting " << meta.original_bik << "... " << (ok ? "FOUND" : "NOT FOUND") << std::endl;
        check(ok, ("BIK header parsed: " + std::string(meta.original_bik)).c_str());
        if (ok) {
            check(header.is_valid(), "Magic is 'BIKi'");
            check(header.width == 640 && header.height == 480, "Resolution is 640x480");
            check(header.num_frames == meta.num_frames, "Frame count matches database");
            check(std::abs(header.fps() - meta.fps) < 0.05f, "FPS matches database");
            check(header.num_audio_tracks == meta.audio_tracks, "Audio track count matches database");
        }
    }
}

void test_playlist_parsing_and_playback() {
    std::cout << "\n--- Testing Playlist Parsing & VideoPlayer State Machine ---" << std::endl;
    const std::string avi_dir = "/storage/emulated/0/Download/Games/PC/Age of Mythology/Age of Mythology/avi/";

    // 1. Parse playlist.aom
    auto files = VideoPlayer::parse_playlist_file(avi_dir + "playlist.aom");
    check(files.size() == 2, "playlist.aom has 2 entries");
    if (files.size() >= 2) {
        check(files[0] == "avi/AOM LOGO.bik", "First entry is AOM LOGO.bik");
        check(files[1] == "avi/introcg.bik", "Second entry is introcg.bik");
    }

    // 2. VideoPlayer state machine simulation
    VideoPlayer player;
    check(player.state() == VideoState::STOPPED, "Initial player state is STOPPED");

    bool started = false;
    std::string started_file;
    player.on_video_started = [&](const std::string& f) {
        started = true;
        started_file = f;
    };

    player.play_cinematic(CinematicId::AOM_LOGO);
    check(player.is_playing(), "Player state is PLAYING");
    check(started && !started_file.empty(), "on_video_started callback fired");
    check(player.total_duration() > 30.0f, "Duration is ~36s");

    player.update(10.0f);
    check(player.current_time() >= 9.9f, "Current time advanced");
    check(player.current_frame() > 0, "Current frame advanced");

    player.pause();
    check(player.state() == VideoState::PAUSED, "Player paused");
    player.resume();
    check(player.is_playing(), "Player resumed");

    // Skip to finish
    bool finished = false;
    player.on_video_finished = [&](const std::string&) { finished = true; };
    player.skip();
    check(finished, "Video finished on skip");
    check(player.state() == VideoState::COMPLETED, "Player state is COMPLETED");
}

void test_animation_script_parsing() {
    std::cout << "\n--- Testing Animation Script Grammar Parsing ---" << std::endl;
    BarArchive anim_bar("/storage/emulated/0/Download/Games/PC/Age of Mythology/Age of Mythology/anim/anim.bar");

    // Extract and parse hoplite_anim.txt
    std::vector<uint8_t> hoplite_bytes = anim_bar.extract("hoplite_anim.txt");
    bool found = !hoplite_bytes.empty();
    check(found, "Extracted hoplite_anim.txt from anim.bar");

    AnimScript script;
    std::string script_str(reinterpret_cast<const char*>(hoplite_bytes.data()), hoplite_bytes.size());
    bool parsed = AnimScriptParser::parse(script_str, script);
    check(parsed, "Parsed hoplite_anim.txt grammar successfully");

    check(!script.imports.empty(), "Found imported definition scripts");
    check(script.get_state("Idle") != nullptr, "Found 'Idle' animation state");
    check(script.get_state("Walk") != nullptr || script.get_state("run") != nullptr, "Found 'Walk'/'run' animation state");
    check(script.get_state("Attack") != nullptr, "Found 'Attack' animation state");
    check(script.get_state("Death") != nullptr, "Found 'Death' animation state");

    const auto* attack_state = script.get_state("Attack");
    if (attack_state) {
        check(!attack_state->visuals.empty(), "Attack state selects visual model");
        bool has_attack_tag = false;
        for (const auto& tag : attack_state->tags) {
            if (tag.name == "Attack") {
                has_attack_tag = true;
                check(tag.time_fraction > 0.4f && tag.time_fraction < 0.7f, "Attack impact tag at ~0.49 fraction");
            }
        }
        check(has_attack_tag, "Found event tag 'Attack' in Attack state");
    }
}

void test_brg_animation_frames_and_morphing() {
    std::cout << "\n--- Testing Multi-Frame BRG ASET & Vertex Morphing ---" << std::endl;
    BarArchive models_bar("/storage/emulated/0/Download/Games/PC/Age of Mythology/Age of Mythology/models/models.bar");

    std::vector<uint8_t> brg_bytes = models_bar.extract("infantry g hoplite_walka.brg");
    bool found = !brg_bytes.empty();
    check(found, "Extracted infantry g hoplite_walka.brg from models.bar");

    BrgAnimationClip clip;
    bool ok = AnimationSystem::parse_brg_clip(brg_bytes.data(), brg_bytes.size(), "HopliteWalk", clip);
    check(ok, "Parsed multi-frame BRG animation clip");
    check(clip.aset.num_frames == 10, "Clip has exactly 10 frames in ASET");
    check(clip.frames.size() == 10, "Extracted 10 MESI frame meshes");
    check(clip.num_vertices == 197, "Hoplite mesh has 197 vertices per frame");
    check(clip.num_faces == 140, "Hoplite mesh has 140 faces");
    check(clip.duration() > 1.0f, "Duration is ~1.42s");

    // Test Vertex Interpolation (Morphing)
    std::vector<glm::vec3> p_start, n_start;
    clip.evaluate(0.0f, p_start, n_start);
    check(p_start.size() == 197, "Evaluated start frame vertices");

    std::vector<glm::vec3> p_mid, n_mid;
    clip.evaluate(clip.duration() * 0.5f, p_mid, n_mid);
    check(p_mid.size() == 197, "Evaluated midpoint frame vertices");

    // Check that vertices actually moved
    float diff = glm::length(p_mid[0] - p_start[0]);
    std::cout << "  Vertex 0 displacement between t=0 and t=mid: " << diff << std::endl;
    check(diff > 0.01f, "Vertices undergo dynamic spatial displacement during animation");
}

void test_unit_animation_state_machine_and_tags() {
    std::cout << "\n--- Testing UnitAnimationState & Tag Dispatch ---" << std::endl;
    BarArchive anim_bar("/storage/emulated/0/Download/Games/PC/Age of Mythology/Age of Mythology/anim/anim.bar");
    BarArchive models_bar("/storage/emulated/0/Download/Games/PC/Age of Mythology/Age of Mythology/models/models.bar");

    std::vector<uint8_t> hoplite_txt = anim_bar.extract("hoplite_anim.txt");
    std::vector<uint8_t> hoplite_brg = models_bar.extract("infantry g hoplite_attacka.brg");

    AnimScript script;
    AnimScriptParser::parse(std::string(reinterpret_cast<const char*>(hoplite_txt.data()), hoplite_txt.size()), script);

    BrgAnimationClip clip;
    AnimationSystem::parse_brg_clip(hoplite_brg.data(), hoplite_brg.size(), "HopliteAttack", clip);

    UnitAnimationState state;
    state.set_script(&script);
    state.set_clip(&clip);

    bool attack_tag_fired = false;
    state.play("Attack", false);
    check(state.is_playing(), "Animation state is playing");

    // Advance time until attack tag fires
    for (int i = 0; i < 20; ++i) {
        state.update(0.05f, [&](const std::string& tag_name) {
            if (tag_name == "Attack") {
                attack_tag_fired = true;
            }
        });
    }
    check(attack_tag_fired, "Animation event tag 'Attack' dispatched at exact keyframe timestamp");
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << " AoM Animation & Video REA Test Suite   " << std::endl;
    std::cout << "========================================" << std::endl;

    test_bik_file_headers_and_database();
    test_playlist_parsing_and_playback();
    test_animation_script_parsing();
    test_brg_animation_frames_and_morphing();
    test_unit_animation_state_machine_and_tags();

    std::cout << "\n========================================" << std::endl;
    if (g_all_passed) {
        std::cout << " ALL ANIM & VIDEO TESTS PASSED! (100%) " << std::endl;
    } else {
        std::cout << " SOME ANIM/VIDEO TESTS FAILED!         " << std::endl;
    }
    std::cout << "========================================" << std::endl;
    return g_all_passed ? 0 : 1;
}
