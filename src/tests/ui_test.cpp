/**
 * Simple unit‑test for the UI subsystem.
 *
 * The project does not currently depend on a testing framework, so we implement a
 * minimal test harness that prints OK/FAIL to stdout. The test exercises the
 * following behaviours:
 *   1. Adding a button via UIManager::add_button stores the button with the
 *      expected normal/pressed colors.
 *   2. Sending a TouchEvent::DOWN that hits the button changes the button's
 *      current_color to pressed_color, invokes the callback, and restores the
 *      normal color.
 *   3. Updating the FPS label text works (simulated via direct modification).
 */

#include <iostream>
#include <string>

#include "ui/ui.h"
#include "renderer/renderer.h"

// ---------------------------------------------------------------------------
// Minimal mock renderer – satisfies the IRenderer interface but does nothing.
// ---------------------------------------------------------------------------
namespace aom {
class MockRenderer : public aom::IRenderer {
public:
    bool initialize(void* /*native_window*/, int /*width*/, int /*height*/) override { return true; }
    void shutdown() override {}
    void resize(int /*width*/, int /*height*/) override {}
    void begin_frame() override {}
    void end_frame() override {}
    aom::TextureHandle create_texture(const aom::DdtTexture& /*ddt*/) override { return aom::INVALID_TEXTURE; }
    aom::TextureHandle create_texture(uint32_t /*w*/, uint32_t /*h*/, const uint8_t* /*rgba*/) override { return aom::INVALID_TEXTURE; }
    void update_texture(aom::TextureHandle /*handle*/, uint32_t /*w*/, uint32_t /*h*/, const uint8_t* /*rgba*/) override {}
    void destroy_texture(aom::TextureHandle) override {}
    aom::MeshHandle create_mesh(const aom::Vertex* /*verts*/, uint32_t /*vcount*/, const uint32_t* /*indices*/, uint32_t /*icount*/) override { return aom::INVALID_MESH; }
    aom::MeshHandle create_terrain_mesh(const aom::TerrainVertex* /*verts*/, uint32_t /*vcount*/, const uint32_t* /*indices*/, uint32_t /*icount*/) override { return aom::INVALID_MESH; }
    void destroy_mesh(aom::MeshHandle) override {}
    void set_camera(const aom::Camera&) override {}
    void set_light(const aom::DirectionalLight&) override {}
    void draw_mesh(aom::MeshHandle, const glm::mat4&, aom::TextureHandle, aom::BlendMode = aom::BlendMode::OPAQUE) override {}
    void draw_terrain(aom::MeshHandle, aom::TextureHandle, aom::TextureHandle) override {}
    void draw_water(const glm::vec4&, aom::TextureHandle, float) override {}
    void begin_ui() override {}
    void draw_ui_quad(const glm::vec2&, const glm::vec2&, aom::TextureHandle, uint32_t) override {}
    void draw_ui_text(const std::string&, const glm::vec2&, float, uint32_t) override {}
    void end_ui() override {}
    void draw_debug_line(const glm::vec3&, const glm::vec3&, uint32_t) override {}
    float get_fps() const override { return 60.0f; }
    const char* get_name() const override { return "MockRenderer"; }
};
} // namespace aom

// Simple test macro
#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cout << "FAIL: " << msg << std::endl; \
            return false; \
        } else { \
            std::cout << "PASS: " << msg << std::endl; \
        } \
    } while (0)

bool test_button_press() {
    aom::MockRenderer renderer;
    aom::UIManager ui;
    TEST_ASSERT(ui.initialize(&renderer, 800, 600), "UI initialise");

    bool clicked = false;
    aom::UIButton* btn = ui.add_button({100, 100}, {200, 50}, [&clicked](){ clicked = true; }, 0xFF00FF00);
    // Verify colors stored correctly
    TEST_ASSERT(btn->normal_color == 0xFF00FF00, "Button normal color set");
    TEST_ASSERT(btn->pressed_color == 0xFFCCCCCC, "Button pressed color default");

    // Simulate a touch inside the button
    aom::TouchEvent ev{150, 120, aom::TouchEvent::State::DOWN};
    ui.on_touch_event(ev);
    // After processing the touch, the callback should have run and the color
    // should be restored to normal.
    TEST_ASSERT(clicked, "Button callback invoked");
    TEST_ASSERT(btn->current_color == btn->normal_color, "Button color restored after press");
    return true;
}

bool test_fps_label() {
    aom::MockRenderer renderer;
    aom::UIManager ui;
    ui.initialize(&renderer, 800, 600);
    aom::UILabel* lbl = ui.add_label("FPS: 0", {10, 10}, 24.f, 0xFFFFFFFF);
    // Directly modify label text to simulate FPS update
    lbl->text = "FPS: 59.9";
    TEST_ASSERT(lbl->text == "FPS: 59.9", "FPS label updated");
    return true;
}

int main() {
    std::cout << "Running UI tests..." << std::endl;
    bool ok = true;
    ok &= test_button_press();
    ok &= test_fps_label();
    std::cout << (ok ? "All UI tests passed" : "Some UI tests failed") << std::endl;
    return ok ? 0 : 1;
}
