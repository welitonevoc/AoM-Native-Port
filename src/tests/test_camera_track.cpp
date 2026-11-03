#include "renderer/camera_track.h"
#include <cassert>
#include <iostream>
#include <cmath>

using namespace aom;

void test_supascreen_snapshot() {
    std::cout << "[TEST] Loading supascreen.cam snapshot..." << std::endl;
    CameraTrack track;
    bool ok = track.load_from_file("/data/user/0/com.agy/files/usr/tmp/cameratracks_analysis/supascreen.cam");
    assert(ok && "Failed to load supascreen.cam");
    assert(track.is_snapshot() && "Track should be marked as snapshot");

    const auto& snap = track.snapshot();
    std::cout << "  Version: " << snap.version << std::endl;
    std::cout << "  Position: (" << snap.position.x << ", " << snap.position.y << ", " << snap.position.z << ")" << std::endl;
    std::cout << "  Forward:  (" << snap.forward.x << ", " << snap.forward.y << ", " << snap.forward.z << ")" << std::endl;
    std::cout << "  FOV:      " << snap.fov << " deg" << std::endl;

    assert(std::abs(snap.position.x - (-650.6010f)) < 0.01f);
    assert(std::abs(snap.position.y - (819.8646f)) < 0.01f);
    assert(std::abs(snap.position.z - (-666.1693f)) < 0.01f);
    assert(std::abs(snap.fov - 50.0f) < 0.01f);

    Camera cam = track.evaluate(0.0f);
    assert(std::abs(cam.position.x - snap.position.x) < 0.001f);
    std::cout << "  PASS: supascreen snapshot parsed with 100% precision." << std::endl;
}

void test_tcb_spline_track() {
    std::cout << "[TEST] Testing TCB Spline Multi-Waypoint CameraTrack..." << std::endl;
    CameraTrack track("CinematicFlyby");

    CameraWaypoint w0;
    w0.time = 0.0f;
    w0.position = glm::vec3(0.0f, 10.0f, 20.0f);
    w0.target = glm::vec3(0.0f, 0.0f, 0.0f);
    w0.up = glm::vec3(0.0f, 1.0f, 0.0f);
    w0.fov = 45.0f;
    w0.tension = 0.2f;
    w0.continuity = 0.0f;
    w0.bias = 0.0f;

    CameraWaypoint w1;
    w1.time = 5.0f;
    w1.position = glm::vec3(30.0f, 15.0f, 0.0f);
    w1.target = glm::vec3(10.0f, 2.0f, 5.0f);
    w1.up = glm::vec3(0.0f, 1.0f, 0.0f);
    w1.fov = 55.0f;
    w1.tension = -0.1f;
    w1.continuity = 0.0f;
    w1.bias = 0.3f;

    CameraWaypoint w2;
    w2.time = 10.0f;
    w2.position = glm::vec3(0.0f, 8.0f, -25.0f);
    w2.target = glm::vec3(0.0f, 0.0f, 0.0f);
    w2.up = glm::vec3(0.0f, 1.0f, 0.0f);
    w2.fov = 45.0f;

    track.add_waypoint(w0);
    track.add_waypoint(w1);
    track.add_waypoint(w2);

    assert(track.waypoint_count() == 3);
    assert(std::abs(track.duration() - 10.0f) < 0.001f);

    // Serialization & roundtrip
    std::vector<uint8_t> bytes = track.save_to_memory();
    assert(bytes.size() > 20);
    assert(bytes[0] == 'C' && bytes[1] == 'T');

    CameraTrack roundtrip;
    bool ok = roundtrip.load_from_memory(bytes);
    assert(ok);
    assert(roundtrip.waypoint_count() == 3);
    assert(std::abs(roundtrip.duration() - 10.0f) < 0.001f);
    std::cout << "  PASS: 'CT' binary format serialization roundtrip verified." << std::endl;

    // Spline evaluation
    Camera c_start = track.evaluate(0.0f);
    assert(std::abs(c_start.position.x - 0.0f) < 0.001f);
    assert(std::abs(c_start.position.y - 10.0f) < 0.001f);

    Camera c_mid = track.evaluate(2.5f);
    std::cout << "  Interpolated pos at t=2.5s: (" << c_mid.position.x << ", " << c_mid.position.y << ", " << c_mid.position.z << ")" << std::endl;
    assert(c_mid.position.x > 0.0f && c_mid.position.x < 30.0f);

    // Playback simulation
    Camera play_cam;
    track.play();
    assert(track.is_playing());
    track.update(2.5f, play_cam);
    assert(std::abs(play_cam.position.x - c_mid.position.x) < 0.001f);
    track.update(8.0f, play_cam);
    assert(track.progress() >= 1.0f);
    std::cout << "  PASS: Playback state machine and TCB interpolation working smoothly." << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << " AoM CameraTrack & .cam REA Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;

    test_supascreen_snapshot();
    test_tcb_spline_track();

    std::cout << "ALL CAMERA TRACK TESTS PASSED!" << std::endl;
    return 0;
}
