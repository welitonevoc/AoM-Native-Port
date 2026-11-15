#include "audio/audio.h"
#include "data/sound_def.h"
#include <iostream>
#include <cassert>
#include <cmath>
#include <memory>

using namespace aom;

static bool g_all_passed = true;

static bool check(bool condition, const char* message) {
    std::cout << (condition ? "PASS: " : "FAIL: ") << message << '\n';
    if (!condition) g_all_passed = false;
    return condition;
}

static void test_audio_engine_creation() {
    std::cout << "\n--- Testing Audio Engine Creation ---" << std::endl;
    auto audio = create_audio_engine();
    check(audio != nullptr, "create_audio_engine returned non-null");
    check(audio->initialize(nullptr), "Audio engine initialized (software fallback)");
    std::cout << "Audio engine created and initialized" << std::endl;
}

static void test_music_volume_control() {
    std::cout << "\n--- Testing Music Volume Control ---" << std::endl;
    auto audio = create_audio_engine();
    audio->initialize(nullptr);

    audio->set_music_volume(0.5f);
    check(std::abs(audio->music_volume() - 0.5f) < 0.001f, "Music volume set to 0.5");

    audio->set_music_volume(1.5f);
    check(std::abs(audio->music_volume() - 1.0f) < 0.001f, "Music volume clamped to 1.0");

    audio->set_music_volume(-0.5f);
    check(std::abs(audio->music_volume() - 0.0f) < 0.001f, "Music volume clamped to 0.0");
}

static void test_sfx_volume_control() {
    std::cout << "\n--- Testing SFX Volume Control ---" << std::endl;
    auto audio = create_audio_engine();
    audio->initialize(nullptr);

    audio->set_sfx_volume(0.7f);
    check(std::abs(audio->sfx_volume() - 0.7f) < 0.001f, "SFX volume set to 0.7");

    audio->set_sfx_enabled(false);
    check(!audio->is_sfx_enabled(), "SFX disabled");

    audio->set_sfx_enabled(true);
    check(audio->is_sfx_enabled(), "SFX re-enabled");
}

static void test_music_enabled_control() {
    std::cout << "\n--- Testing Music Enabled Control ---" << std::endl;
    auto audio = create_audio_engine();
    audio->initialize(nullptr);

    check(audio->is_music_enabled(), "Music enabled by default");

    audio->set_music_enabled(false);
    check(!audio->is_music_enabled(), "Music disabled");

    audio->set_music_enabled(true);
    check(audio->is_music_enabled(), "Music re-enabled");
}

static void test_sfx_simultaneous_voices() {
    std::cout << "\n--- Testing Simultaneous SFX Voices ---" << std::endl;
    auto audio = create_audio_engine();
    audio->initialize(nullptr);

    audio->play_click();
    audio->play_click();
    audio->play_click();
    audio->play_click();
    audio->play_click();

    size_t active = audio->active_voice_count();
    std::cout << "Active voices after 5 clicks: " << active << std::endl;
    check(active >= 1, "At least one voice active after multiple clicks");

    size_t dispatched = audio->dispatched_voice_count();
    check(dispatched >= 5, "At least 5 voices dispatched cumulatively");
}

static void test_audio_3d_positional() {
    std::cout << "\n--- Testing 3D Positional Audio ---" << std::endl;
    auto audio = create_audio_engine();
    audio->initialize(nullptr);

    audio->set_listener(glm::vec3(0,0,0), glm::vec3(0,0,1), glm::vec3(1,0,0));
    auto pos = audio->listener_position();
    check(std::abs(pos.x) < 0.001f && std::abs(pos.y) < 0.001f && std::abs(pos.z) < 0.001f,
          "Listener position set to origin");

    audio->play_sfx_3d("click.wav", glm::vec3(2,0,0), 1.0f, 1.0f, 60.0f);
    size_t active = audio->active_voice_count();
    check(active >= 1, "3D SFX spawned a voice near listener");

    audio->play_sfx_3d("click.wav", glm::vec3(100,0,0), 1.0f, 1.0f, 60.0f);
    check(true, "3D SFX far away handled without crash");
}

static void test_sound_bank_definitions() {
    std::cout << "\n--- Testing Sound Bank Definitions (SoundDefManager) ---" << std::endl;
    SoundDefManager mgr;

    check(mgr.soundset_count() == 0, "SoundDefManager starts empty (loaded by engine from sounds.bar)");

    SoundSetDef empty_set;
    empty_set.name = "Empty";
    const std::string* picked = mgr.pick_sound(empty_set, 42);
    check(picked == nullptr, "pick_sound on empty set returns nullptr");

    std::cout << "Sound bank definitions structure valid" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << " P5.5 — Áudio Ampliado Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;

    test_audio_engine_creation();
    test_music_volume_control();
    test_sfx_volume_control();
    test_music_enabled_control();
    test_sfx_simultaneous_voices();
    test_audio_3d_positional();
    test_sound_bank_definitions();

    std::cout << "\n========================================" << std::endl;
    if (g_all_passed) {
        std::cout << " ALL AUDIO TESTS PASSED! (100%)" << std::endl;
    } else {
        std::cout << " SOME AUDIO TESTS FAILED!" << std::endl;
    }
    std::cout << "========================================" << std::endl;

    return g_all_passed ? 0 : 1;
}
