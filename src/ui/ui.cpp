#include "ui/ui.h"
#include "renderer/renderer.h"
#include "audio/audio.h"
extern "C" {
#include "stb_image.h"
}

#include <algorithm>
#include <cstdio>
#include <cmath>

namespace aom {

bool UIManager::initialize(IRenderer* renderer, int width, int height) {
    if (!renderer) return false;
    renderer_ = renderer;
    width_ = width;
    height_ = height;
    return true;
}

void UIManager::shutdown() {
    clear();
    renderer_ = nullptr;
    audio_ = nullptr;
}

void UIManager::clear() {
    buttons_.clear();
    labels_.clear();
    animations_.clear();
    buttons_.reserve(64);
    labels_.reserve(64);
}

void UIManager::update(float dt) {
    for (auto& anim : animations_) {
        if (anim.frame_count <= 1) continue;
        anim.elapsed += dt;
        if (anim.elapsed >= anim.frame_time) {
            anim.elapsed -= anim.frame_time;
            anim.current_frame = (anim.current_frame + 1) % anim.frame_count;
        }
    }
}

void UIManager::render() {
    if (!renderer_) return;

    renderer_->begin_ui();

    // Render animated elements
    for (const auto& anim : animations_) {
        if (anim.tex != INVALID_TEXTURE) {
            renderer_->draw_ui_quad(anim.pos, anim.size, anim.tex, 0xFFFFFFFF);
        }
    }

    // Render buttons
    for (const auto& btn : buttons_) {
        // Determine texture to draw
        TextureHandle cur_tex = btn.normal_tex;
        if (btn.current_color == btn.pressed_color && btn.pressed_tex != INVALID_TEXTURE) {
            cur_tex = btn.pressed_tex;
        }

        // Draw button background quad
        if (cur_tex != INVALID_TEXTURE) {
            renderer_->draw_ui_quad(btn.pos, btn.size, cur_tex, btn.current_color);
        } else if ((btn.current_color >> 24) > 0) {
            renderer_->draw_ui_quad(btn.pos, btn.size, INVALID_TEXTURE, btn.current_color);
        }

        // Icon if present
        if (btn.icon != INVALID_TEXTURE) {
            glm::vec2 iconSize = btn.size * 0.4f;
            glm::vec2 iconPos = btn.pos + glm::vec2(8.0f, (btn.size.y - iconSize.y) * 0.5f);
            renderer_->draw_ui_quad(iconPos, iconSize, btn.icon, 0xFFFFFFFF);
        }

        // Checkbox rendering
        if (btn.is_checkbox) {
            float box_size = btn.size.y * 0.6f;
            glm::vec2 box_pos = btn.pos + glm::vec2(10.0f, (btn.size.y - box_size) * 0.5f);
            // Outer stone/gold border
            renderer_->draw_ui_quad(box_pos, {box_size, box_size}, INVALID_TEXTURE, 0xFF886622);
            // Inner background
            renderer_->draw_ui_quad(box_pos + glm::vec2(2.0f), {box_size - 4.0f, box_size - 4.0f}, INVALID_TEXTURE, 0xFF221100);
            if (btn.is_checked) {
                // Gold checkmark fill
                renderer_->draw_ui_quad(box_pos + glm::vec2(5.0f), {box_size - 10.0f, box_size - 10.0f}, INVALID_TEXTURE, 0xFFFFD000);
            }

            // Checkbox label
            if (!btn.label.empty()) {
                float text_x = box_pos.x + box_size + 12.0f;
                float text_y = btn.pos.y + (btn.size.y - btn.text_size) * 0.5f;
                // Drop shadow
                renderer_->draw_ui_text(btn.label, {text_x + 1.5f, text_y + 1.5f}, btn.text_size, 0xFF000000);
                renderer_->draw_ui_text(btn.label, {text_x, text_y}, btn.text_size, btn.text_color);
            }
            continue;
        }

        // Slider rendering
        if (btn.is_slider) {
            // Label above or on the left
            float label_x = btn.pos.x + 8.0f;
            float label_y = btn.pos.y + 4.0f;
            if (!btn.label.empty()) {
                renderer_->draw_ui_text(btn.label, {label_x + 1.0f, label_y + 1.0f}, btn.text_size * 0.9f, 0xFF000000);
                renderer_->draw_ui_text(btn.label, {label_x, label_y}, btn.text_size * 0.9f, btn.text_color);
            }

            // Slider track
            const float trackHeight = 6.0f;
            float track_y = btn.pos.y + btn.size.y - 14.0f;
            glm::vec2 trackPos = glm::vec2(btn.pos.x + 10.0f, track_y);
            glm::vec2 trackSize = glm::vec2(btn.size.x - 70.0f, trackHeight);
            renderer_->draw_ui_quad(trackPos, trackSize, INVALID_TEXTURE, 0xFF443311);

            // Filled portion of the track (AoM gold)
            glm::vec2 fillSize = glm::vec2(trackSize.x * btn.slider_value, trackHeight);
            renderer_->draw_ui_quad(trackPos, fillSize, INVALID_TEXTURE, 0xFFFFB800);

            // Knob
            float knob_w = 14.0f;
            float knob_h = 18.0f;
            glm::vec2 knobPos = glm::vec2(trackPos.x + btn.slider_value * trackSize.x - knob_w * 0.5f, track_y - (knob_h - trackHeight) * 0.5f);
            renderer_->draw_ui_quad(knobPos, {knob_w, knob_h}, INVALID_TEXTURE, 0xFFFFE066);

            // Value text (e.g. 75%)
            char valBuf[16];
            std::snprintf(valBuf, sizeof(valBuf), "%.0f%%", btn.slider_value * 100.0f);
            float val_x = btn.pos.x + btn.size.x - 52.0f;
            float val_y = track_y - 6.0f;
            renderer_->draw_ui_text(valBuf, {val_x + 1.0f, val_y + 1.0f}, 14.0f, 0xFF000000);
            renderer_->draw_ui_text(valBuf, {val_x, val_y}, 14.0f, 0xFFFFD000);
            continue;
        }

        // Standard Button Text Label
        if (!btn.label.empty()) {
            float text_y = btn.pos.y + (btn.size.y - btn.text_size) * 0.5f;
            // Approximate character width for centering
            float approx_char_w = btn.text_size * 0.55f;
            float text_width = btn.label.size() * approx_char_w;
            float text_x = btn.pos.x + (btn.size.x - text_width) * 0.5f;
            if (btn.icon != INVALID_TEXTURE) {
                text_x = btn.pos.x + btn.size.y * 0.4f + 16.0f;
            }

            // High-contrast outline and drop shadow for authentic AoM carved gold look
            renderer_->draw_ui_text(btn.label, {text_x - 1.0f, text_y}, btn.text_size, 0xFF181005);
            renderer_->draw_ui_text(btn.label, {text_x + 1.0f, text_y}, btn.text_size, 0xFF181005);
            renderer_->draw_ui_text(btn.label, {text_x, text_y - 1.0f}, btn.text_size, 0xFF181005);
            renderer_->draw_ui_text(btn.label, {text_x, text_y + 1.5f}, btn.text_size, 0xFF181005);
            renderer_->draw_ui_text(btn.label, {text_x + 1.5f, text_y + 2.0f}, btn.text_size, 0xDD000000);
            // Crisp authentic golden yellow text
            renderer_->draw_ui_text(btn.label, {text_x, text_y}, btn.text_size, btn.text_color);
        }
    }

    // Render labels
    for (const auto& lbl : labels_) {
        if (lbl.shadow) {
            renderer_->draw_ui_text(lbl.text, {lbl.pos.x + 1.5f, lbl.pos.y + 1.5f}, lbl.size, 0xFF000000);
        }
        renderer_->draw_ui_text(lbl.text, lbl.pos, lbl.size, lbl.color);
    }

    renderer_->end_ui();
}

bool UIManager::handle_touch(const TouchEvent& ev) {
    if (ev.state == TouchEvent::State::DOWN) {
        for (auto& btn : buttons_) {
            if (hit_test(btn, ev.x, ev.y)) {
                btn.current_color = btn.pressed_color;

                if (btn.is_checkbox) {
                    btn.is_checked = !btn.is_checked;
                    if (audio_) audio_->play_click();
                    if (btn.onToggle) btn.onToggle(btn.is_checked);
                    btn.current_color = btn.normal_color;
                    return true;
                }

                if (btn.is_slider) {
                    float left = btn.pos.x + 10.0f;
                    float right = btn.pos.x + btn.size.x - 60.0f;
                    float clampedX = std::clamp(ev.x, left, right);
                    float t = (right > left) ? (clampedX - left) / (right - left) : 0.0f;
                    btn.slider_value = t;
                    if (audio_) audio_->play_click();
                    if (btn.onSliderChange) btn.onSliderChange(t);
                    return true;
                }

                // Standard button click sound
                if (audio_) {
                    if (btn.is_big_button) audio_->play_big_button();
                    else audio_->play_click();
                }

                if (btn.onClick) {
                    btn.onClick();
                }
                btn.current_color = btn.normal_color;
                return true;
            }
        }
    } else if (ev.state == TouchEvent::State::MOVE) {
        for (auto& btn : buttons_) {
            if (btn.is_slider && hit_test(btn, ev.x, ev.y)) {
                float left = btn.pos.x + 10.0f;
                float right = btn.pos.x + btn.size.x - 60.0f;
                float clampedX = std::clamp(ev.x, left, right);
                float t = (right > left) ? (clampedX - left) / (right - left) : 0.0f;
                btn.slider_value = t;
                if (btn.onSliderChange) {
                    btn.onSliderChange(t);
                }
                return true;
            }
        }
    } else if (ev.state == TouchEvent::State::UP) {
        for (auto& btn : buttons_) {
            btn.current_color = btn.normal_color;
        }
    }
    return false;
}

void UIManager::on_touch_event(const TouchEvent& ev) {
    handle_touch(ev);
}

UIAnimation* UIManager::add_animation(const glm::vec2& pos,
                                     const glm::vec2& size,
                                     TextureHandle tex,
                                     int frame_count,
                                     float frame_time) {
    UIAnimation anim;
    anim.pos = pos;
    anim.size = size;
    anim.tex = tex;
    anim.frame_count = frame_count;
    anim.frame_time = frame_time;
    animations_.push_back(anim);
    return &animations_.back();
}

UIButton* UIManager::add_button(const glm::vec2& pos,
                               const glm::vec2& size,
                               const std::string& label,
                               const std::function<void()>& cb,
                               uint32_t normal_color,
                               uint32_t pressed_color) {
    UIButton btn;
    btn.pos = pos;
    btn.size = size;
    btn.normal_color = normal_color;
    btn.pressed_color = pressed_color;
    btn.current_color = normal_color;
    btn.onClick = cb;
    btn.label = label;
    btn.normal_tex = default_btn_normal_;
    btn.pressed_tex = default_btn_pressed_;
    btn.is_big_button = (size.y >= 38.0f);
    buttons_.push_back(btn);
    return &buttons_.back();
}

UIButton* UIManager::add_button(const glm::vec2& pos,
                               const glm::vec2& size,
                               const std::function<void()>& cb,
                               uint32_t normal_color,
                               uint32_t pressed_color) {
    return add_button(pos, size, "", cb, normal_color, pressed_color);
}

UIButton* UIManager::add_button(const glm::vec2& pos,
                               const glm::vec2& size,
                               const std::function<void()>& cb,
                               uint32_t normal_color,
                               const std::string& icon_path) {
    TextureHandle tex = INVALID_TEXTURE;
    if (!icon_path.empty() && renderer_) {
        int w = 0, h = 0, comp = 0;
        unsigned char* data = stbi_load(icon_path.c_str(), &w, &h, &comp, 4);
        if (data) {
            tex = renderer_->create_texture(static_cast<uint32_t>(w), static_cast<uint32_t>(h), data);
            stbi_image_free(data);
        }
    }
    UIButton btn;
    btn.pos = pos;
    btn.size = size;
    btn.normal_color = normal_color;
    btn.pressed_color = 0xFFCCCCCC;
    btn.current_color = normal_color;
    btn.onClick = cb;
    btn.icon = tex;
    btn.normal_tex = default_btn_normal_;
    btn.pressed_tex = default_btn_pressed_;
    buttons_.push_back(btn);
    return &buttons_.back();
}

UIButton* UIManager::add_slider(const glm::vec2& pos,
                               const glm::vec2& size,
                               const std::string& label,
                               float initial_value,
                               const std::function<void(float)>& onChange) {
    UIButton btn;
    btn.pos = pos;
    btn.size = size;
    btn.label = label;
    btn.is_slider = true;
    btn.slider_value = std::clamp(initial_value, 0.0f, 1.0f);
    btn.onSliderChange = onChange;
    btn.normal_color = 0xAA221105; // Dark stone translucent
    btn.pressed_color = 0xAA332211;
    btn.current_color = btn.normal_color;
    buttons_.push_back(btn);
    return &buttons_.back();
}

UIButton* UIManager::add_checkbox(const glm::vec2& pos,
                                 const glm::vec2& size,
                                 const std::string& label,
                                 bool initial_state,
                                 const std::function<void(bool)>& onToggle) {
    UIButton btn;
    btn.pos = pos;
    btn.size = size;
    btn.label = label;
    btn.is_checkbox = true;
    btn.is_checked = initial_state;
    btn.onToggle = onToggle;
    btn.normal_color = 0x00000000; // transparent
    btn.pressed_color = 0x00000000;
    btn.current_color = btn.normal_color;
    buttons_.push_back(btn);
    return &buttons_.back();
}

UILabel* UIManager::add_label(const std::string& txt,
                             const glm::vec2& pos,
                             float size,
                             uint32_t color) {
    UILabel lbl;
    lbl.text = txt;
    lbl.pos = pos;
    lbl.size = size;
    lbl.color = color;
    labels_.push_back(lbl);
    return &labels_.back();
}

bool UIManager::hit_test(const UIButton& btn, float x, float y) {
    return x >= btn.pos.x && x <= (btn.pos.x + btn.size.x) &&
           y >= btn.pos.y && y <= (btn.pos.y + btn.size.y);
}

} // namespace aom
