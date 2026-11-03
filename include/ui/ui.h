#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <glm/glm.hpp>

#include "renderer/renderer.h"

namespace aom {

class AudioEngine;

struct TouchEvent {
    enum class State { DOWN, MOVE, UP };
    float x;
    float y;
    State state;
};

struct UIButton {
    glm::vec2 pos;
    glm::vec2 size;
    uint32_t normal_color = 0xFFFFFFFF;
    uint32_t pressed_color = 0xFFCCCCCC;
    uint32_t current_color = 0xFFFFFFFF;
    std::function<void()> onClick;
    std::string label;
    uint32_t text_color = 0xFFFFD000; // AoM authentic gold (#FFD000)
    float text_size = 20.0f;
    TextureHandle normal_tex = INVALID_TEXTURE;
    TextureHandle pressed_tex = INVALID_TEXTURE;
    TextureHandle icon = INVALID_TEXTURE;
    bool is_big_button = false;

    // Slider fields
    bool is_slider = false;
    float slider_value = 0.0f;
    std::function<void(float)> onSliderChange;

    // Checkbox fields
    bool is_checkbox = false;
    bool is_checked = false;
    std::function<void(bool)> onToggle;
};

struct UILabel {
    std::string text;
    glm::vec2 pos;
    float size = 18.0f;
    uint32_t color = 0xFFFFD000;
    bool shadow = true;
};

struct UIAnimation {
    glm::vec2 pos;
    glm::vec2 size;
    TextureHandle tex = 0;
    int frame_count = 1;
    float frame_time = 0.1f;
    float elapsed = 0.0f;
    int current_frame = 0;
};

class UIManager {
public:
    UIManager() = default;
    ~UIManager() = default;

    UIManager(const UIManager&) = delete;
    UIManager& operator=(const UIManager&) = delete;

    bool initialize(IRenderer* renderer, int width, int height);
    void shutdown();
    void update(float dt);
    void render();
    void clear();

    void set_audio_engine(AudioEngine* audio) { audio_ = audio; }
    void set_default_button_textures(TextureHandle normal, TextureHandle pressed) {
        default_btn_normal_ = normal;
        default_btn_pressed_ = pressed;
    }

    void on_touch_event(const TouchEvent& ev);
    bool handle_touch(const TouchEvent& ev);

    UIAnimation* add_animation(const glm::vec2& pos,
                              const glm::vec2& size,
                              TextureHandle tex,
                              int frame_count = 1,
                              float frame_time = 0.1f);

    /** Add a standard button */
    UIButton* add_button(const glm::vec2& pos,
                         const glm::vec2& size,
                         const std::string& label,
                         const std::function<void()>& cb,
                         uint32_t normal_color = 0xFFFFFFFF,
                         uint32_t pressed_color = 0xFFCCCCCC);

    /** Add a simple button without text */
    UIButton* add_button(const glm::vec2& pos,
                         const glm::vec2& size,
                         const std::function<void()>& cb,
                         uint32_t normal_color = 0xFFFFFFFF,
                         uint32_t pressed_color = 0xFFCCCCCC);

    /** Add a button with an icon */
    UIButton* add_button(const glm::vec2& pos,
                         const glm::vec2& size,
                         const std::function<void()>& cb,
                         uint32_t normal_color,
                         const std::string& icon_path);

    /** Add a slider control */
    UIButton* add_slider(const glm::vec2& pos,
                         const glm::vec2& size,
                         const std::string& label,
                         float initial_value,
                         const std::function<void(float)>& onChange);

    /** Add a checkbox toggle */
    UIButton* add_checkbox(const glm::vec2& pos,
                           const glm::vec2& size,
                           const std::string& label,
                           bool initial_state,
                           const std::function<void(bool)>& onToggle);

    UILabel* add_label(const std::string& txt,
                       const glm::vec2& pos,
                       float size = 18.0f,
                       uint32_t color = 0xFFFFD000);

    [[nodiscard]] const std::vector<UIButton>& buttons() const { return buttons_; }
    [[nodiscard]] const std::vector<UILabel>& labels() const { return labels_; }
    [[nodiscard]] const std::vector<UIAnimation>& animations() const { return animations_; }

private:
    IRenderer* renderer_ = nullptr;
    AudioEngine* audio_ = nullptr;
    int width_ = 0;
    int height_ = 0;

    TextureHandle default_btn_normal_ = INVALID_TEXTURE;
    TextureHandle default_btn_pressed_ = INVALID_TEXTURE;

    std::vector<UIButton> buttons_;
    std::vector<UILabel> labels_;
    std::vector<UIAnimation> animations_;

    static bool hit_test(const UIButton& btn, float x, float y);
};

} // namespace aom
