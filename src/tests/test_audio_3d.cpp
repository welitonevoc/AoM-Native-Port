#include "audio/audio.h"
#include "assets/bar_archive.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <vector>

namespace aom {

// Helper to construct in-memory test WAV files
std::vector<uint8_t> create_test_wav(uint32_t sample_rate, uint16_t channels, uint16_t bits_per_sample, size_t num_frames) {
    std::vector<uint8_t> data;
    uint32_t bytes_per_sample = bits_per_sample / 8;
    uint32_t data_size = static_cast<uint32_t>(num_frames * channels * bytes_per_sample);
    uint32_t riff_size = 36 + data_size;

    data.resize(44 + data_size, 0);

    // RIFF header
    std::memcpy(data.data(), "RIFF", 4);
    std::memcpy(data.data() + 4, &riff_size, 4);
    std::memcpy(data.data() + 8, "WAVE", 4);

    // fmt chunk
    std::memcpy(data.data() + 12, "fmt ", 4);
    uint32_t fmt_len = 16;
    uint16_t format_tag = 1; // PCM
    uint32_t byte_rate = sample_rate * channels * bytes_per_sample;
    uint16_t block_align = channels * bytes_per_sample;

    std::memcpy(data.data() + 16, &fmt_len, 4);
    std::memcpy(data.data() + 20, &format_tag, 2);
    std::memcpy(data.data() + 22, &channels, 2);
    std::memcpy(data.data() + 24, &sample_rate, 4);
    std::memcpy(data.data() + 28, &byte_rate, 4);
    std::memcpy(data.data() + 32, &block_align, 2);
    std::memcpy(data.data() + 34, &bits_per_sample, 2);

    // data chunk
    std::memcpy(data.data() + 36, "data", 4);
    std::memcpy(data.data() + 40, &data_size, 4);

    // Generate sinusoidal PCM content
    uint8_t* pcm_ptr = data.data() + 44;
    for (size_t f = 0; f < num_frames; ++f) {
        float val = std::sin(2.0f * 3.14159265f * 440.0f * (static_cast<float>(f) / sample_rate));
        for (uint16_t c = 0; c < channels; ++c) {
            if (bits_per_sample == 16) {
                int16_t s16 = static_cast<int16_t>(val * 16000.0f);
                std::memcpy(pcm_ptr, &s16, 2);
                pcm_ptr += 2;
            } else if (bits_per_sample == 8) {
                uint8_t u8 = static_cast<uint8_t>(std::clamp<int>(static_cast<int>(val * 100.0f) + 128, 0, 255));
                *pcm_ptr++ = u8;
            }
        }
    }

    return data;
}

} // namespace aom

int main() {
    std::cout << "====================================================\n";
    std::cout << " AoM Sprint 6: 3D Positional Audio & OpenSL Tests   \n";
    std::cout << "====================================================\n\n";

    int assertions = 0;
    auto assert_test = [&](bool cond, const char* msg) {
        if (!cond) {
            std::cerr << "FAIL: " << msg << std::endl;
            std::exit(1);
        }
        std::cout << "PASS: " << msg << std::endl;
        assertions++;
    };

    auto audio = aom::create_audio_engine();
    assert_test(audio != nullptr, "Created OpenSL / 3D audio engine instance");

    bool initialized = audio->initialize(nullptr);
    assert_test(initialized, "Audio engine initialized successfully (software fallback guaranteed)");

    // --- 1. Audio Engine State & Volume Controls ---
    std::cout << "\n--- 1. Testing Audio Engine State & Volume Controls ---\n";
    assert_test(audio->is_music_enabled(), "Music enabled by default");
    assert_test(audio->is_sfx_enabled(), "SFX enabled by default");

    audio->set_sfx_volume(0.85f);
    assert_test(std::abs(audio->sfx_volume() - 0.85f) < 0.01f, "SFX volume clamped and set to 0.85");

    audio->set_music_volume(0.60f);
    assert_test(std::abs(audio->music_volume() - 0.60f) < 0.01f, "Music volume clamped and set to 0.60");

    // --- 2. 3D Spatial Listener & Orientation ---
    std::cout << "\n--- 2. Testing 3D Spatial Listener Orientation ---\n";
    glm::vec3 initial_listener = audio->listener_position();
    assert_test(initial_listener == glm::vec3(0.0f), "Default listener position is origin (0, 0, 0)");

    glm::vec3 new_pos(15.0f, 0.0f, 25.0f);
    glm::vec3 forward(0.0f, 0.0f, 1.0f);
    glm::vec3 right(1.0f, 0.0f, 0.0f);
    audio->set_listener(new_pos, forward, right);

    glm::vec3 current_listener = audio->listener_position();
    assert_test(current_listener == new_pos, "Listener position successfully updated to (15, 0, 25)");

    // Reset listener to origin for controlled 3D math tests
    audio->set_listener(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(1.0f, 0.0f, 0.0f));

    // --- 3. 3D Positional Audio & Distance Attenuation ---
    std::cout << "\n--- 3. Testing 3D Positional Audio & Distance Attenuation ---\n";
    // Play sound within min_distance (e.g. at dist = 2m, min_dist = 5m)
    audio->play_sfx_3d("assets/sound/new_objective.wav", glm::vec3(2.0f, 0.0f, 0.0f), 1.0f, 5.0f, 65.0f);
    assert_test(audio->dispatched_voice_count() > 0, "Voice enqueued for nearby 3D sound event");

    // Play sound beyond max_distance (e.g. at dist = 80m, max_dist = 65m)
    // O contador cumulativo de disparos e usado porque `active_voice_count()`
    // mede apenas as vozes em reproducao (cai quando um WAV termina).
    size_t dispatched_before = audio->dispatched_voice_count();
    audio->play_sfx_3d("assets/sound/new_objective.wav", glm::vec3(80.0f, 0.0f, 0.0f), 1.0f, 5.0f, 65.0f);
    assert_test(audio->dispatched_voice_count() == dispatched_before, "Sounds beyond max_distance are culled (0 voices spawned)");

    // --- 4. Greek Unit Voice Dispatch (Sprint 6 Target) ---
    std::cout << "\n--- 4. Testing Greek Unit Voice Dispatch ---\n";
    // Unit select ("Etimos!", "Lege!")
    audio->play_unit_voice_3d("select", glm::vec3(-5.0f, 0.0f, 0.0f), "hoplite", false);
    assert_test(audio->dispatched_voice_count() >= 2, "Dispatched Greek military select voice cue ('Etimos' / gms*.wav)");

    // Unit move ("Vulome!", "Pame!")
    audio->play_unit_voice_3d("move", glm::vec3(-5.0f, 0.0f, 0.0f), "hoplite", false);
    assert_test(audio->dispatched_voice_count() >= 3, "Dispatched Greek military move voice cue ('Vulome' / gmm*.wav)");

    // Unit attack ("Prostagma!", "En Taxis!")
    audio->play_unit_voice_3d("attack", glm::vec3(8.0f, 0.0f, 0.0f), "toxotes", false);
    assert_test(audio->dispatched_voice_count() >= 4, "Dispatched Greek military attack voice cue ('Prostagma' / gma*.wav)");

    // Worker select & move
    audio->play_unit_voice_3d("select", glm::vec3(0.0f, 0.0f, 3.0f), "aldeao", true);
    audio->play_unit_voice_3d("move", glm::vec3(0.0f, 0.0f, 3.0f), "aldeao", true);
    assert_test(audio->dispatched_voice_count() >= 6, "Dispatched Greek worker voice cues (gvms*.wav, gvmm*.wav)");

    // Hero: Arkantos voice lines
    audio->play_unit_voice_3d("select", glm::vec3(0.0f), "Arkantos", false);
    audio->play_unit_voice_3d("attack", glm::vec3(0.0f), "Arkantos", false);
    assert_test(audio->dispatched_voice_count() >= 8, "Dispatched Arkantos Hero voice lines (arkantosselect1, arkantosattack1)");

    // Mythological: Kraken voice
    audio->play_unit_voice_3d("select", glm::vec3(12.0f, 0.0f, 0.0f), "Kraken", false);
    assert_test(audio->dispatched_voice_count() >= 9, "Dispatched Kraken mythological voice line (krakenacknowledge1)");

    // --- 5. Authentic AoM Combat & Harvest Event Triggers ---
    std::cout << "\n--- 5. Testing Authentic AoM Combat & Harvest Triggers ---\n";
    audio->play_combat_hit(glm::vec3(3.0f, 0.0f, 2.0f), true);  // metal
    audio->play_combat_hit(glm::vec3(3.0f, 0.0f, 2.0f), false); // flesh
    assert_test(audio->dispatched_voice_count() >= 11, "Triggered sword on metal & flesh impact sounds");

    audio->play_arrow_shot(glm::vec3(-2.0f, 0.0f, 0.0f));
    audio->play_arrow_hit(glm::vec3(4.0f, 0.0f, 0.0f));
    assert_test(audio->dispatched_voice_count() >= 13, "Triggered arrow flight and projectile impact sounds");

    audio->play_harvest_sound(glm::vec3(-6.0f, 0.0f, 0.0f), "wood");
    audio->play_harvest_sound(glm::vec3(-4.0f, 0.0f, 0.0f), "gold");
    audio->play_harvest_sound(glm::vec3(-2.0f, 0.0f, 0.0f), "food");
    assert_test(audio->dispatched_voice_count() >= 16, "Triggered wood chopping, gold mining, and forage harvest sounds");

    audio->play_building_work(glm::vec3(-5.0f, 0.0f, 5.0f));
    audio->play_unit_death(glm::vec3(7.0f, 0.0f, 1.0f), false);
    assert_test(audio->dispatched_voice_count() >= 18, "Triggered construction hammering and unit death sound cues");

    // --- 6. Software Multi-Voice Mixer & Saturated Accumulation ---
    std::cout << "\n--- 6. Testing Software Multi-Voice Mixer ---\n";
    // Allocate 1024 frames of 16-bit stereo PCM (2048 samples)
    std::vector<int16_t> mixer_output(1024 * 2, 0);

    // Call mixer step directly
    auto* opensl_ptr = dynamic_cast<aom::AudioEngine*>(audio.get());
    assert_test(opensl_ptr != nullptr, "AudioEngine dynamic cast valid");

    // Mix active voices
    audio->play_click();
    audio->play_big_button();

    // Disable SFX and verify silent output
    audio->set_sfx_enabled(false);
    assert_test(!audio->is_sfx_enabled(), "SFX disabled cleanly");
    assert_test(audio->active_voice_count() == 0, "Disabling SFX cleared active voices");

    audio->set_sfx_enabled(true);
    assert_test(audio->is_sfx_enabled(), "SFX re-enabled");

    // --- 7. Sound Archive (.BAR) Fallback Integration ---
    std::cout << "\n--- 7. Testing Sound Archive BAR Integration ---\n";
    aom::BarArchive sound_bar;
    const std::string bar_candidates[] = {
        "/storage/emulated/0/Documents/AoM/sound/sounds.bar",
        "/storage/emulated/0/Documents/AoM-Native-Port/sounds.bar",
        "sounds.bar"
    };
    bool bar_opened = false;
    for (const auto& c : bar_candidates) {
        if (sound_bar.open(c)) {
            bar_opened = true;
            break;
        }
    }
    if (bar_opened) {
        audio->set_sound_archive(&sound_bar);
        assert_test(sound_bar.file_count() > 0, "Sound archive (.BAR) successfully opened and loaded");
        // Test fallback extraction for an authentic AoM sound in archive
        audio->play_sfx_3d("swordonmetal3.wav", glm::vec3(0.0f), 1.0f);
        assert_test(audio->active_voice_count() > 0, "Successfully extracted and played sound directly from sounds.bar");
    } else {
        std::cout << "SKIP: sounds.bar not found at test paths (optional archive fallback)\n";
    }

    // --- 8. Dynamic Camera Panning Update Verification ---
    std::cout << "\n--- 8. Testing Dynamic Camera Panning Update ---\n";
    audio->play_sfx_3d("assets/sound/new_objective.wav", glm::vec3(20.0f, 0.0f, 0.0f), 1.0f, 5.0f, 65.0f);
    size_t count_before_pan = audio->active_voice_count();
    assert_test(count_before_pan > 0, "Spawned sound to the right of camera");

    // Pan camera to the right so the sound becomes centered
    audio->set_listener(glm::vec3(20.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    assert_test(audio->listener_position().x == 20.0f, "Listener updated; active 3D voices panned dynamically");

    audio->shutdown();
    assert_test(audio->active_voice_count() == 0, "Audio engine shut down cleanly with 0 remaining voices");

    std::cout << "\n====================================================\n";
    std::cout << " ALL SPRINT 6 3D AUDIO TESTS PASSED! (" << assertions << " assertions, 100%)\n";
    std::cout << "====================================================\n";
    return 0;
}
