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
    bool interactive = true;  // se false, o botão não recebe toques (painel de fundo)
    bool visible = true;      // visibilidade para submenus do AoM (gadgetReal/gadgetUnReal)
    std::string tag;          // nome do gadget próprio (ex: "LearnToPlaySubButton")
    std::string group_tag;    // tag do container pai (ex: "MainMenuOptions", "LearnSubMenu")

    // 9-slice borders (in pixels). When > 0, the button texture is drawn
    // as 9 slices: 4 corners fixed, 4 edges stretched in one axis,
    // center stretched in both. Set all to 0 for full-stretch behavior.
    float border_left = 0.0f;
    float border_right = 0.0f;
    float border_top = 0.0f;
    float border_bottom = 0.0f;

    // Texture dimensions (filled by engine when loading via texture_resolver)
    uint32_t tex_width = 0;
    uint32_t tex_height = 0;

    // UV coordinates for texture atlases
    glm::vec2 uv_min{0.0f, 0.0f};
    glm::vec2 uv_max{1.0f, 1.0f};
    bool has_uv = false;

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
    bool visible = true;
    std::string tag;
    std::string group_tag;
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

    /** Add a solid-color background quad (no text, no interaction) */
    UIButton* add_background_quad(const glm::vec2& pos,
                                  const glm::vec2& size,
                                  TextureHandle tex = INVALID_TEXTURE,
                                  uint32_t color = 0xFFFFFFFF);

    void set_group_visible(const std::string& tag, bool visible);

    [[nodiscard]] const std::vector<UIButton>& buttons() const { return buttons_; }
    [[nodiscard]] const std::vector<UILabel>& labels() const { return labels_; }
    [[nodiscard]] const std::vector<UIAnimation>& animations() const { return animations_; }
    std::vector<UIButton>& buttons_mut() { return buttons_; }
    std::vector<UILabel>& labels_mut() { return labels_; }

    // Resolve textos no formato $$ID$$texto_fallback (ex: $$19825$$Learn To Play).
    // Extrai o ID entre os primeiros $$ e o segundo $$, tenta traduzir, senão mostra o fallback.
    template <typename TranslateFn>
    void resolve_text(TranslateFn&& tr_fn) {
        auto resolve_one = [&](std::string& s) {
            if (s.empty()) return;
            while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n')) {
                s.pop_back();
            }
            if (s.size() >= 4 && s[0] == '$' && s[1] == '$') {
                std::string last_id;
                size_t p = 0;
                while (p + 2 < s.size() && s[p] == '$' && s[p + 1] == '$') {
                    size_t end = s.find("$$", p + 2);
                    if (end == std::string::npos) break;
                    last_id = s.substr(p + 2, end - (p + 2));
                    p = end + 2;
                }
                std::string fallback = s.substr(p);
                while (!fallback.empty() && (fallback.front() == ' ' || fallback.front() == '\t')) fallback.erase(fallback.begin());
                while (!fallback.empty() && (fallback.back() == ' ' || fallback.back() == '\t')) fallback.pop_back();

                std::string translated;
                if (!last_id.empty()) {
                    translated = tr_fn(last_id);
                }
                if (translated.empty() || translated == last_id) {
                    if (!fallback.empty()) {
                        translated = tr_fn(fallback);
                    }
                }
                if (!translated.empty() && translated != last_id && translated != fallback) {
                    s = translated;
                } else if (!fallback.empty()) {
                    s = fallback;
                }
                return;
            }
            std::string translated = tr_fn(s);
            if (!translated.empty() && translated != s) {
                s = translated;
            }
        };
        for (auto& btn : buttons_) {
            resolve_one(btn.label);
        }
        for (auto& lbl : labels_) {
            resolve_one(lbl.text);
        }
    }

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
