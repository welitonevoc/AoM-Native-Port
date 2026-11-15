#include "ui/ui.h"
#include "renderer/renderer.h"
#include "audio/audio.h"
extern "C" {
#include "stb_image.h"
}

#include <algorithm>
#include <cstdio>
#include <cmath>

#include "engine/logger.h"
#define LOG_TAG "AoM_UI"
#define LOGI(...) AOM_LOGI(LOG_TAG, __VA_ARGS__)
#define LOGE(...) AOM_LOGE(LOG_TAG, __VA_ARGS__)

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
    scroll_lists_.clear();
    radio_sets_.clear();
    lists_.clear();
    // Limpar teclado virtual também
    virtual_keyboard_.visible = false;
    virtual_keyboard_.target_edit_box = nullptr;
    // Não reserve(64): layouts com mais de 64 elementos realocam o vetor e
    // invalidam ponteiros armazenados (ex: hud_skip_btn_). Deixar o vector
    // gerenciar a capacidade sozinho; o chamador deve usar os ponteiros
    // retornados por add_button() imediatamente após a criação.
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

    // PASS 1: Render background panels first (z-order inferior)
    // São botões com normal_tex == INVALID_TEXTURE e label vazio
    for (const auto& btn : buttons_) {
        if (!btn.visible) continue;
        if (btn.label.empty() && btn.normal_tex == INVALID_TEXTURE && btn.size.x > 50.0f && btn.size.y > 50.0f) {
            // Provavelmente um painel de fundo
            if (btn.current_color != 0x00000000) {
                renderer_->draw_ui_quad(btn.pos, btn.size, INVALID_TEXTURE, btn.current_color);
            }
        }
    }

    // PASS 2: Render animated elements
    for (const auto& anim : animations_) {
        if (anim.tex != INVALID_TEXTURE) {
            renderer_->draw_ui_quad(anim.pos, anim.size, anim.tex, 0xFFFFFFFF);
        }
    }

    // PASS 3: Render buttons (interactive elements)
    for (const auto& btn : buttons_) {
        if (!btn.visible) continue;
        // Skip background panels (already rendered in pass 1)
        if (btn.label.empty() && btn.normal_tex == INVALID_TEXTURE && btn.size.x > 50.0f && btn.size.y > 50.0f) {
            continue;
        }
        // Determine texture to draw based on state
        TextureHandle cur_tex = btn.normal_tex;
        if (btn.current_color == btn.pressed_color && btn.pressed_tex != INVALID_TEXTURE) {
            cur_tex = btn.pressed_tex;
        } else if (btn.is_hovered && btn.rollover_tex != INVALID_TEXTURE) {
            cur_tex = btn.rollover_tex;
        } else if (btn.is_hovered && btn.glow_tex != INVALID_TEXTURE) {
            cur_tex = btn.glow_tex;
        }

        // Draw button background quad (UV sub-rect, 9-slice or standard)
        if (cur_tex != INVALID_TEXTURE) {
            if (btn.has_uv) {
                renderer_->draw_ui_quad_uv(btn.pos, btn.size, cur_tex, btn.uv_min, btn.uv_max, btn.current_color);
            } else if (btn.border_left > 0.0f || btn.border_right > 0.0f ||
                btn.border_top > 0.0f || btn.border_bottom > 0.0f) {
                renderer_->draw_ui_quad_nineslice(btn.pos, btn.size, cur_tex,
                                                  btn.tex_width, btn.tex_height,
                                                  btn.border_left, btn.border_right,
                                                  btn.border_top, btn.border_bottom,
                                                  btn.current_color);
            } else {
                renderer_->draw_ui_quad(btn.pos, btn.size, cur_tex, btn.current_color);
            }
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
            float text_width = renderer_->measure_ui_text(btn.label, btn.text_size);
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
        if (!lbl.visible) continue;
        if (lbl.shadow) {
            renderer_->draw_ui_text(lbl.text, {lbl.pos.x + 1.5f, lbl.pos.y + 1.5f}, lbl.size, 0xFF000000);
        }
        renderer_->draw_ui_text(lbl.text, lbl.pos, lbl.size, lbl.color);
    }

    // Render ScrollLists (Dropdowns)
    for (const auto& sl : scroll_lists_) {
        if (!sl.visible) continue;
        // Main dropdown button background
        uint32_t bg_color = sl.bg_tex != INVALID_TEXTURE ? 0xFFFFFFFF : 0xFF221810;
        if (sl.bg_tex != INVALID_TEXTURE) {
            renderer_->draw_ui_quad(sl.pos, sl.size, sl.bg_tex, bg_color);
            if (sl.top_cap_tex != INVALID_TEXTURE) {
                renderer_->draw_ui_quad(sl.pos, {sl.size.x, 8.0f}, sl.top_cap_tex, 0xFFFFFFFF);
            }
            if (sl.bottom_cap_tex != INVALID_TEXTURE) {
                renderer_->draw_ui_quad({sl.pos.x, sl.pos.y + sl.size.y - 8.0f}, {sl.size.x, 8.0f}, sl.bottom_cap_tex, 0xFFFFFFFF);
            }
        } else {
            renderer_->draw_ui_quad(sl.pos, sl.size, INVALID_TEXTURE, bg_color);
        }
        // Border
        renderer_->draw_ui_quad({sl.pos.x, sl.pos.y}, {sl.size.x, 2.0f}, INVALID_TEXTURE, 0xFFFFD000);
        renderer_->draw_ui_quad({sl.pos.x, sl.pos.y + sl.size.y - 2.0f}, {sl.size.x, 2.0f}, INVALID_TEXTURE, 0xFFFFD000);
        renderer_->draw_ui_quad({sl.pos.x, sl.pos.y}, {2.0f, sl.size.y}, INVALID_TEXTURE, 0xFFFFD000);
        renderer_->draw_ui_quad({sl.pos.x + sl.size.x - 2.0f, sl.pos.y}, {2.0f, sl.size.y}, INVALID_TEXTURE, 0xFFFFD000);

        // Selected item text
        if (!sl.items.empty() && sl.selected_index < static_cast<int>(sl.items.size())) {
            const std::string& sel_text = sl.items[sl.selected_index];
            float text_x = sl.pos.x + 8.0f;
            float text_y = sl.pos.y + (sl.size.y - sl.text_size) * 0.5f;
            renderer_->draw_ui_text(sel_text, {text_x + 1.0f, text_y + 1.0f}, sl.text_size, 0xFF000000);
            renderer_->draw_ui_text(sel_text, {text_x, text_y}, sl.text_size, sl.text_color);
        }

        // Dropdown arrow indicator
        float arrow_x = sl.pos.x + sl.size.x - 20.0f;
        float arrow_y = sl.pos.y + (sl.size.y - 12.0f) * 0.5f;
        renderer_->draw_ui_quad({arrow_x, arrow_y}, {12.0f, 12.0f}, INVALID_TEXTURE, 0xFFFFD000);

        // Expanded items (with scissor clipping to prevent overflow)
        if (sl.expanded) {
            float item_y = sl.pos.y + sl.size.y;
            float expanded_height = static_cast<float>(sl.items.size()) * sl.item_height;
            
            // Enable scissor test to clip dropdown items to the expanded area
            renderer_->set_ui_scissor(
                static_cast<int>(sl.pos.x),
                static_cast<int>(item_y),
                static_cast<int>(sl.size.x),
                static_cast<int>(expanded_height + 2.0f), // +2 for bottom border
                true
            );
            
            for (size_t i = 0; i < sl.items.size(); ++i) {
                float iy = item_y + static_cast<float>(i) * sl.item_height;
                // Item background
                uint32_t item_bg = (static_cast<int>(i) == sl.selected_index) ? 0xFF332211 : 0xFF1A1208;
                renderer_->draw_ui_quad({sl.pos.x, iy}, {sl.size.x, sl.item_height}, INVALID_TEXTURE, item_bg);
                // Item text
                float text_x = sl.pos.x + 8.0f;
                float text_y = iy + (sl.item_height - sl.text_size) * 0.5f;
                uint32_t tc = (i < sl.item_colors.size()) ? sl.item_colors[i] : sl.text_color;
                renderer_->draw_ui_text(sl.items[i], {text_x + 1.0f, text_y + 1.0f}, sl.text_size, 0xFF000000);
                renderer_->draw_ui_text(sl.items[i], {text_x, text_y}, sl.text_size, tc);
                // Separator line
                if (i + 1 < sl.items.size()) {
                    renderer_->draw_ui_quad({sl.pos.x + 4.0f, iy + sl.item_height - 1.0f}, {sl.size.x - 8.0f, 1.0f}, INVALID_TEXTURE, 0xFF443322);
                }
            }
            // Bottom border of expanded list
            float last_y = item_y + static_cast<float>(sl.items.size()) * sl.item_height;
            renderer_->draw_ui_quad({sl.pos.x, last_y}, {sl.size.x, 2.0f}, INVALID_TEXTURE, 0xFFFFD000);
            
            // Disable scissor test
            renderer_->set_ui_scissor(0, 0, 0, 0, false);
        }
    }

    // Render RadioSets
    for (const auto& rs : radio_sets_) {
        if (!rs.visible) continue;
        float option_y = rs.pos.y;
        for (size_t i = 0; i < rs.options.size(); ++i) {
            float iy = option_y + static_cast<float>(i) * rs.option_height;
            // Radio button circle
            float radio_x = rs.pos.x + 8.0f;
            float radio_y = iy + (rs.option_height - 20.0f) * 0.5f;
            float radio_size = 20.0f;
            if (rs.unchecked_tex != INVALID_TEXTURE && rs.checked_tex != INVALID_TEXTURE) {
                TextureHandle rtex = (static_cast<int>(i) == rs.selected_index) ? rs.checked_tex : rs.unchecked_tex;
                renderer_->draw_ui_quad({radio_x, radio_y}, {radio_size, radio_size}, rtex, 0xFFFFFFFF);
            } else {
                // Fallback: draw circles
                uint32_t ring_color = 0xFFFFD000;
                renderer_->draw_ui_quad({radio_x, radio_y}, {radio_size, radio_size}, INVALID_TEXTURE, ring_color);
                renderer_->draw_ui_quad({radio_x + 2.0f, radio_y + 2.0f}, {radio_size - 4.0f, radio_size - 4.0f}, INVALID_TEXTURE, 0xFF1A1208);
                if (static_cast<int>(i) == rs.selected_index) {
                    renderer_->draw_ui_quad({radio_x + 5.0f, radio_y + 5.0f}, {radio_size - 10.0f, radio_size - 10.0f}, INVALID_TEXTURE, 0xFFFFD000);
                }
            }
            // Option label
            float text_x = radio_x + radio_size + 10.0f;
            float text_y = iy + (rs.option_height - rs.text_size) * 0.5f;
            renderer_->draw_ui_text(rs.options[i], {text_x + 1.0f, text_y + 1.0f}, rs.text_size, 0xFF000000);
            renderer_->draw_ui_text(rs.options[i], {text_x, text_y}, rs.text_size, rs.text_color);
        }
    }

    // Render Lists (XMB list)
    for (const auto& lst : lists_) {
        if (!lst.visible) continue;
        // Background
        if (lst.bg_tex != INVALID_TEXTURE) {
            renderer_->draw_ui_quad(lst.pos, lst.size, lst.bg_tex, 0xFFFFFFFF);
        } else {
            renderer_->draw_ui_quad(lst.pos, lst.size, INVALID_TEXTURE, 0xFF1A1208);
        }
        // Border
        renderer_->draw_ui_quad({lst.pos.x, lst.pos.y}, {lst.size.x, 1.0f}, INVALID_TEXTURE, 0xFFFFD000);
        renderer_->draw_ui_quad({lst.pos.x, lst.pos.y + lst.size.y - 1.0f}, {lst.size.x, 1.0f}, INVALID_TEXTURE, 0xFFFFD000);
        renderer_->draw_ui_quad({lst.pos.x, lst.pos.y}, {1.0f, lst.size.y}, INVALID_TEXTURE, 0xFFFFD000);
        renderer_->draw_ui_quad({lst.pos.x + lst.size.x - 1.0f, lst.pos.y}, {1.0f, lst.size.y}, INVALID_TEXTURE, 0xFFFFD000);

        // Items
        for (size_t i = 0; i < lst.items.size(); ++i) {
            float iy = lst.pos.y + static_cast<float>(i) * lst.item_height;
            if (iy + lst.item_height > lst.pos.y + lst.size.y) break; // Clip to list bounds

            // Selection highlight
            if (static_cast<int>(i) == lst.selected_index && lst.sel_tex != INVALID_TEXTURE) {
                renderer_->draw_ui_quad({lst.pos.x, iy}, {lst.size.x, lst.item_height}, lst.sel_tex, 0xFFFFFFFF);
            } else if (static_cast<int>(i) == lst.selected_index) {
                renderer_->draw_ui_quad({lst.pos.x, iy}, {lst.size.x, lst.item_height}, INVALID_TEXTURE, 0xFF332211);
            }

            // Item text
            float text_x = lst.pos.x + 8.0f;
            float text_y = iy + (lst.item_height - lst.text_size) * 0.5f;
            uint32_t tc = (i < lst.item_colors.size()) ? lst.item_colors[i] : lst.text_color;
            renderer_->draw_ui_text(lst.items[i], {text_x + 1.0f, text_y + 1.0f}, lst.text_size, 0xFF000000);
            renderer_->draw_ui_text(lst.items[i], {text_x, text_y}, lst.text_size, tc);
        }
    }

    // Render Virtual Keyboard (topmost z-order)
    if (virtual_keyboard_.visible) {
        render_virtual_keyboard();
    }

    renderer_->end_ui();
}

bool UIManager::handle_touch(const TouchEvent& ev) {
    if (ev.state == TouchEvent::State::DOWN) {
        LOGI("[GADGET] Touch DOWN em (%.1f, %.1f)", ev.x, ev.y);

        // 1. ScrollLists (dropdowns) - z-order superior
        for (auto it = scroll_lists_.rbegin(); it != scroll_lists_.rend(); ++it) {
            auto& sl = *it;
            if (!sl.visible) continue;
            if (hit_test_scroll_list(sl, ev.x, ev.y)) {
                LOGI("[GADGET] Hit scroll_list: selected=%d expanded=%d", sl.selected_index, sl.expanded);
                if (sl.expanded) {
                    // Clique em um item expandido
                    float item_y = sl.pos.y + sl.size.y;
                    for (size_t i = 0; i < sl.items.size(); ++i) {
                        float iy = item_y + static_cast<float>(i) * sl.item_height;
                        if (ev.x >= sl.pos.x && ev.x <= (sl.pos.x + sl.size.x) &&
                            ev.y >= iy && ev.y <= (iy + sl.item_height)) {
                            if (static_cast<int>(i) != sl.selected_index) {
                                sl.selected_index = static_cast<int>(i);
                                if (audio_) audio_->play_click();
                                if (sl.onSelectionChange) sl.onSelectionChange(sl.selected_index);
                            }
                            sl.expanded = false;
                            return true;
                        }
                    }
                    // Clique fora da lista expandida = fecha
                    sl.expanded = false;
                    return true;
                } else {
                    // Clique no botão principal = abre/fecha
                    sl.expanded = !sl.expanded;
                    if (audio_) audio_->play_click();
                    return true;
                }
            }
        }

        // 2. RadioSets
        for (auto it = radio_sets_.rbegin(); it != radio_sets_.rend(); ++it) {
            auto& rs = *it;
            if (!rs.visible) continue;
            if (hit_test_radio_set(rs, ev.x, ev.y)) {
                LOGI("[GADGET] Hit radio_set: selected=%d", rs.selected_index);
                float option_y = rs.pos.y;
                for (size_t i = 0; i < rs.options.size(); ++i) {
                    float iy = option_y + static_cast<float>(i) * rs.option_height;
                    if (ev.x >= rs.pos.x && ev.x <= (rs.pos.x + rs.size.x) &&
                        ev.y >= iy && ev.y <= (iy + rs.option_height)) {
                        if (static_cast<int>(i) != rs.selected_index) {
                            rs.selected_index = static_cast<int>(i);
                            if (audio_) audio_->play_click();
                            if (rs.onSelectionChange) rs.onSelectionChange(rs.selected_index);
                        }
                        return true;
                    }
                }
            }
        }

        // 2.5. Lists (XMB list)
        for (auto it = lists_.rbegin(); it != lists_.rend(); ++it) {
            auto& lst = *it;
            if (!lst.visible) continue;
            if (hit_test_list(lst, ev.x, ev.y)) {
                LOGI("[GADGET] Hit list: selected=%d", lst.selected_index);
                // Calcular qual item foi clicado
                float rel_y = ev.y - lst.pos.y;
                int item_index = static_cast<int>(rel_y / lst.item_height);
                if (item_index >= 0 && item_index < static_cast<int>(lst.items.size())) {
                    if (item_index != lst.selected_index) {
                        lst.selected_index = item_index;
                        if (audio_) audio_->play_click();
                        if (lst.onSelectionChange) lst.onSelectionChange(lst.selected_index);
                    }
                }
                return true;
            }
        }

        // 3. Buttons (existing logic)
        bool hit_edit_box = false;
        for (auto it = buttons_.rbegin(); it != buttons_.rend(); ++it) {
            auto& btn = *it;
            if (!btn.visible || !btn.interactive || !hit_test(btn, ev.x, ev.y)) continue;

            LOGI("[GADGET] Hit button: label='%s' tag='%s' pos=(%.1f,%.1f) size=(%.1f,%.1f) cb=%d editBox=%d",
                 btn.label.c_str(), btn.tag.c_str(), btn.pos.x, btn.pos.y, btn.size.x, btn.size.y,
                 btn.onClick ? 1 : 0, btn.is_edit_box ? 1 : 0);

            btn.current_color = btn.pressed_color;

            // EditBox focus handling
            if (btn.is_edit_box) {
                hit_edit_box = true;
                if (on_edit_box_focus_) {
                    on_edit_box_focus_(&btn, true);  // Focus gained
                }
            }
        }

        // Blur: se clicou em algo que NÃO é editBox, esconde teclado virtual
        if (!hit_edit_box && virtual_keyboard_.visible) {
            hide_virtual_keyboard();
        }

        // Second pass for actual button handling (checkbox, slider, click)
        for (auto it = buttons_.rbegin(); it != buttons_.rend(); ++it) {
            auto& btn = *it;
            if (!btn.visible || !btn.interactive || !hit_test(btn, ev.x, ev.y)) continue;

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

            auto cb = btn.onClick;
            btn.current_color = btn.normal_color;
            if (cb) {
                cb();
            }
            return true;
        }
    } else if (ev.state == TouchEvent::State::MOVE) {
        // Update hover state for all interactive buttons (for rollover/glow textures)
        for (auto& btn : buttons_) {
            if (!btn.visible || !btn.interactive) continue;
            btn.is_hovered = hit_test(btn, ev.x, ev.y);
        }

        // Slider drag
        for (auto it = buttons_.rbegin(); it != buttons_.rend(); ++it) {
            auto& btn = *it;
            if (!btn.interactive || !btn.is_slider) continue;
            if (!hit_test(btn, ev.x, ev.y)) continue;
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
    } else if (ev.state == TouchEvent::State::UP) {
        for (auto& btn : buttons_) {
            btn.current_color = btn.normal_color;
            btn.is_hovered = false;
        }
    }
    return false;
}

void UIManager::set_group_visible(const std::string& tag, bool visible) {
    size_t count = 0;
    for (auto& btn : buttons_) {
        if (btn.tag == tag || btn.group_tag == tag) {
            btn.visible = visible;
            ++count;
        }
    }
    for (auto& lbl : labels_) {
        if (lbl.tag == tag || lbl.group_tag == tag) {
            lbl.visible = visible;
            ++count;
        }
    }
    for (auto& sl : scroll_lists_) {
        if (sl.tag == tag || sl.group_tag == tag) {
            sl.visible = visible;
            ++count;
        }
    }
    for (auto& rs : radio_sets_) {
        if (rs.tag == tag || rs.group_tag == tag) {
            rs.visible = visible;
            ++count;
        }
    }
    for (auto& lst : lists_) {
        if (lst.tag == tag || lst.group_tag == tag) {
            lst.visible = visible;
            ++count;
        }
    }
    LOGI("[GADGET] set_group_visible('%s', visible=%d) afetou %zu elementos",
         tag.c_str(), (int)visible, count);
}

void UIManager::on_touch_event(const TouchEvent& ev) {
    // Handle virtual keyboard first (topmost z-order)
    if (virtual_keyboard_.visible && handle_virtual_keyboard_touch(ev)) {
        return;
    }
    handle_touch(ev);
}

// ============================================================
// Virtual Keyboard Implementation
// ============================================================

void UIManager::show_virtual_keyboard(UIButton* target) {
    if (!target || !target->is_edit_box) return;
    
    const float vw = static_cast<float>(width_);
    const float vh = static_cast<float>(height_);
    
    virtual_keyboard_.target_edit_box = target;
    virtual_keyboard_.visible = true;
    virtual_keyboard_.layout = UIVirtualKeyboard::LAYOUT_ALPHA;
    virtual_keyboard_.shift = false;
    
    // Position at bottom of screen
    virtual_keyboard_.pos = {0.0f, vh * 0.5f};
    virtual_keyboard_.size = {vw, vh * 0.5f};
    
    // Callbacks
    virtual_keyboard_.onKeyPress = [this, target](const std::string& ch) {
        if (target->label.size() + ch.size() <= target->max_length) {
            target->label += ch;
            if (target->onTextInput) target->onTextInput(target->label);
        }
    };
    virtual_keyboard_.onBackspace = [this, target]() {
        if (!target->label.empty()) {
            target->label.pop_back();
            if (target->onTextInput) target->onTextInput(target->label);
        }
    };
    virtual_keyboard_.onEnter = [this]() {
        hide_virtual_keyboard();
    };
    virtual_keyboard_.onLayoutChange = [this]() {
        virtual_keyboard_.layout = static_cast<UIVirtualKeyboard::Layout>(
            (static_cast<int>(virtual_keyboard_.layout) + 1) % 3
        );
    };
    
    LOGI("[VKB] Virtual keyboard shown for editBox: %s", target->tag.c_str());
}

void UIManager::hide_virtual_keyboard() {
    if (!virtual_keyboard_.visible) return;
    LOGI("[VKB] Virtual keyboard hidden");
    virtual_keyboard_.visible = false;
    virtual_keyboard_.target_edit_box = nullptr;
}

bool UIManager::handle_virtual_keyboard_touch(const TouchEvent& ev) {
    if (!virtual_keyboard_.visible || ev.state != TouchEvent::State::DOWN) return false;
    
    const float vk_x = virtual_keyboard_.pos.x;
    const float vk_y = virtual_keyboard_.pos.y;
    const float vk_w = virtual_keyboard_.size.x;
    const float vk_h = virtual_keyboard_.size.y;
    
    // Check if touch is within keyboard area
    if (ev.x < vk_x || ev.x > vk_x + vk_w || ev.y < vk_y || ev.y > vk_y + vk_h) {
        hide_virtual_keyboard();
        return false; // Touch outside keyboard - hide keyboard and let touch fall through
    }
    
    // Keyboard layout dimensions
    const int rows = 4;
    const int cols_alpha = 10;
    const int cols_numeric = 10;
    const int cols_symbols = 12;
    
    const float key_w = vk_w / 12.0f;
    const float key_h = vk_h / 5.0f;
    
    int col = static_cast<int>((ev.x - vk_x) / key_w);
    int row = static_cast<int>((ev.y - vk_y) / key_h);
    
    col = std::clamp(col, 0, 11);
    row = std::clamp(row, 0, 4);
    
    // Define keyboard layouts
    const std::vector<std::vector<std::string>> alpha_keys = {
        {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "⌫", "123"},
        {"A", "S", "D", "F", "G", "H", "J", "K", "L", "⏎", "⇧", "#+="},
        {"Z", "X", "C", "V", "B", "N", "M", ",", ".", "?", " ", " "},
        {" ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " "}
    };
    
    const std::vector<std::vector<std::string>> numeric_keys = {
        {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "⌫", "ABC"},
        {"@", "#", "$", "%", "&", "*", "(", ")", "-", "+", "⏎", "#+="},
        {"=", "{", "}", "[", "]", "\\", "|", "/", ":", ";", " ", " "},
        {" ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " "}
    };
    
    const std::vector<std::vector<std::string>> symbol_keys = {
        {"~", "`", "|", "•", "√", "π", "÷", "×", "∞", "°", "⌫", "ABC"},
        {"¡", "¿", "¢", "£", "¥", "€", "§", "¶", "†", "‡", "⏎", "123"},
        {"‹", "›", "«", "»", "←", "→", "↑", "↓", "↔", "↵", " ", " "},
        {" ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " "}
    };
    
    const auto& layout = [&]() -> const std::vector<std::vector<std::string>>& {
        switch (virtual_keyboard_.layout) {
            case UIVirtualKeyboard::LAYOUT_ALPHA: return alpha_keys;
            case UIVirtualKeyboard::LAYOUT_NUMERIC: return numeric_keys;
            case UIVirtualKeyboard::LAYOUT_SYMBOLS: return symbol_keys;
        }
        return alpha_keys;
    }();
    
    if (row >= 0 && row < static_cast<int>(layout.size()) && 
        col >= 0 && col < static_cast<int>(layout[row].size())) {
        const std::string& key = layout[row][col];
        
        if (key == "⌫") {
            if (virtual_keyboard_.onBackspace) virtual_keyboard_.onBackspace();
        } else if (key == "⏎") {
            if (virtual_keyboard_.onEnter) virtual_keyboard_.onEnter();
        } else if (key == "⇧") {
            virtual_keyboard_.shift = !virtual_keyboard_.shift;
        } else if (key == "123" || key == "ABC" || key == "#+=") {
            if (virtual_keyboard_.onLayoutChange) virtual_keyboard_.onLayoutChange();
        } else if (key == " ") {
            if (virtual_keyboard_.onKeyPress) virtual_keyboard_.onKeyPress(" ");
        } else if (!key.empty()) {
            std::string ch = virtual_keyboard_.shift ? key : key;
            if (!virtual_keyboard_.shift) {
                // Lowercase for alpha layout
                if (virtual_keyboard_.layout == UIVirtualKeyboard::LAYOUT_ALPHA) {
                    for (char& c : ch) c = std::tolower(c);
                }
            }
            if (virtual_keyboard_.onKeyPress) virtual_keyboard_.onKeyPress(ch);
            if (virtual_keyboard_.shift) virtual_keyboard_.shift = false; // Auto-release shift
        }
    }
    
    return true; // Consumed touch
}

void UIManager::render_virtual_keyboard() {
    if (!virtual_keyboard_.visible || !renderer_) return;
    
    const float vk_x = virtual_keyboard_.pos.x;
    const float vk_y = virtual_keyboard_.pos.y;
    const float vk_w = virtual_keyboard_.size.x;
    const float vk_h = virtual_keyboard_.size.y;
    
    // Draw keyboard background
    renderer_->draw_ui_quad({vk_x, vk_y}, {vk_w, vk_h}, INVALID_TEXTURE, 0xFF1A1208);
    
    const int rows = 4;
    const float key_w = vk_w / 12.0f;
    const float key_h = vk_h / 5.0f;
    
    const auto& layout = [&]() -> const std::vector<std::vector<std::string>>& {
        switch (virtual_keyboard_.layout) {
            case UIVirtualKeyboard::LAYOUT_ALPHA: {
                static const std::vector<std::vector<std::string>> k = {
                    {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "⌫", "123"},
                    {"A", "S", "D", "F", "G", "H", "J", "K", "L", "⏎", "⇧", "#+="},
                    {"Z", "X", "C", "V", "B", "N", "M", ",", ".", "?", " ", " "},
                    {" ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " "}
                };
                return k;
            }
            case UIVirtualKeyboard::LAYOUT_NUMERIC: {
                static const std::vector<std::vector<std::string>> k = {
                    {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "⌫", "ABC"},
                    {"@", "#", "$", "%", "&", "*", "(", ")", "-", "+", "⏎", "#+="},
                    {"=", "{", "}", "[", "]", "\\", "|", "/", ":", ";", " ", " "},
                    {" ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " "}
                };
                return k;
            }
            case UIVirtualKeyboard::LAYOUT_SYMBOLS: {
                static const std::vector<std::vector<std::string>> k = {
                    {"~", "`", "|", "•", "√", "π", "÷", "×", "∞", "°", "⌫", "ABC"},
                    {"¡", "¿", "¢", "£", "¥", "€", "§", "¶", "†", "‡", "⏎", "123"},
                    {"‹", "›", "«", "»", "←", "→", "↑", "↓", "↔", "↵", " ", " "},
                    {" ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " "}
                };
                return k;
            }
        }
        static const std::vector<std::vector<std::string>> empty;
        return empty;
    }();
    
    for (int row = 0; row < rows && row < static_cast<int>(layout.size()); ++row) {
        for (int col = 0; col < static_cast<int>(layout[row].size()); ++col) {
            const std::string& key = layout[row][col];
            if (key.empty()) continue;
            
            float kx = vk_x + col * key_w + 2.0f;
            float ky = vk_y + row * key_h + 2.0f;
            float kw = key_w - 4.0f;
            float kh = key_h - 4.0f;
            
            // Key background
            uint32_t key_color = 0xFF332211;
            if (key == "⌫" || key == "⏎" || key == "⇧" || key == "123" || key == "ABC" || key == "#+=") {
                key_color = 0xFF553311; // Special keys slightly lighter
            }
            renderer_->draw_ui_quad({kx, ky}, {kw, kh}, INVALID_TEXTURE, key_color);
            
            // Key border
            renderer_->draw_ui_quad({kx - 1, ky - 1}, {kw + 2, 1}, INVALID_TEXTURE, 0xFF886644); // Top
            renderer_->draw_ui_quad({kx - 1, ky + kh}, {kw + 2, 1}, INVALID_TEXTURE, 0xFF886644); // Bottom
            renderer_->draw_ui_quad({kx - 1, ky}, {1, kh + 2}, INVALID_TEXTURE, 0xFF886644); // Left
            renderer_->draw_ui_quad({kx + kw, ky}, {1, kh + 2}, INVALID_TEXTURE, 0xFF886644); // Right
            
            // Key label
            std::string display_key = key;
            if (!virtual_keyboard_.shift && virtual_keyboard_.layout == UIVirtualKeyboard::LAYOUT_ALPHA) {
                for (char& c : display_key) c = std::tolower(c);
            }
            
            if (renderer_) {
                renderer_->draw_ui_text(display_key, {kx + kw * 0.5f - display_key.size() * 4.0f, ky + kh * 0.5f - 6.0f}, 
                                      14.0f, 0xFFFFD000);
            }
        }
    }
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
    LOGI("[UI add_button] label='%s' normal_color=0x%08X pressed_color=0x%08X pos=(%.0f,%.0f) size=(%.0f,%.0f)",
         label.c_str(), normal_color, pressed_color, pos.x, pos.y, size.x, size.y);
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

// Retira todos os blocos iniciais no formato $$<número>$$ do texto do XMB.
// Ex: $$19825$$$$19825$$Learn To Play -> Learn To Play
// Colapsa espaços à esquerda (o XMB usa espaços para alinhar texto dentro dos $$ID$$).
static std::string strip_string_ids(std::string s) {
    while (s.size() >= 4 && s[0] == '$' && s[1] == '$') {
        size_t end = s.find("$$", 2);
        if (end == std::string::npos) break;
        s.erase(0, end + 2);
    }
    size_t p = s.find_first_not_of(' ');
    return (p == std::string::npos) ? s : s.substr(p);
}

UILabel* UIManager::add_label(const std::string& txt,
                             const glm::vec2& pos,
                             float size,
                             uint32_t color) {
    UILabel lbl;
    lbl.text = strip_string_ids(txt);
    lbl.pos = pos;
    lbl.size = size;
    lbl.color = color;
    labels_.push_back(lbl);
    return &labels_.back();
}

UIButton* UIManager::add_background_quad(const glm::vec2& pos,
                                         const glm::vec2& size,
                                         TextureHandle tex,
                                         uint32_t color) {
    // A background quad is a button with no text, no callback, solid color
    UIButton* btn = add_button(pos, size, std::function<void()>(), color, color);
    if (btn) {
        btn->label = "";
        btn->normal_tex = tex;
        btn->pressed_tex = tex;
        btn->current_color = color;
        btn->interactive = false;  // background quads don't receive touches
    }
    return btn;
}

bool UIManager::hit_test(const UIButton& btn, float x, float y) {
    return x >= btn.pos.x && x <= (btn.pos.x + btn.size.x) &&
           y >= btn.pos.y && y <= (btn.pos.y + btn.size.y);
}

bool UIManager::hit_test_scroll_list(const UIScrollList& sl, float x, float y) {
    // Hit test do botão principal do dropdown
    if (x >= sl.pos.x && x <= (sl.pos.x + sl.size.x) &&
        y >= sl.pos.y && y <= (sl.pos.y + sl.size.y)) {
        return true;
    }
    // Hit test dos itens expandidos
    if (sl.expanded) {
        float item_y = sl.pos.y + sl.size.y;
        for (size_t i = 0; i < sl.items.size(); ++i) {
            float iy = item_y + static_cast<float>(i) * sl.item_height;
            if (x >= sl.pos.x && x <= (sl.pos.x + sl.size.x) &&
                y >= iy && y <= (iy + sl.item_height)) {
                return true;
            }
        }
    }
    return false;
}

bool UIManager::hit_test_radio_set(const UIRadioSet& rs, float x, float y) {
    float option_y = rs.pos.y;
    for (size_t i = 0; i < rs.options.size(); ++i) {
        float iy = option_y + static_cast<float>(i) * rs.option_height;
        if (x >= rs.pos.x && x <= (rs.pos.x + rs.size.x) &&
            y >= iy && y <= (iy + rs.option_height)) {
            return true;
        }
    }
    return false;
}

UIScrollList* UIManager::add_scroll_list(const glm::vec2& pos,
                                         const glm::vec2& size,
                                         const std::vector<std::string>& items,
                                         int initial_selection,
                                         const std::function<void(int)>& onSelectionChange) {
    UIScrollList sl;
    sl.pos = pos;
    sl.size = size;
    sl.items = items;
    sl.selected_index = std::clamp(initial_selection, 0, static_cast<int>(items.size()) - 1);
    sl.item_height = std::max(20.0f, size.y);
    sl.onSelectionChange = onSelectionChange;
    // Default colors
    sl.item_colors.assign(items.size(), 0xFFFFD000);
    LOGI("[UI add_scroll_list] pos=(%.0f,%.0f) size=(%.0f,%.0f) items=%zu initial=%d",
         pos.x, pos.y, size.x, size.y, items.size(), sl.selected_index);
    scroll_lists_.push_back(sl);
    return &scroll_lists_.back();
}

UIRadioSet* UIManager::add_radio_set(const glm::vec2& pos,
                                     const glm::vec2& size,
                                     const std::vector<std::string>& options,
                                     int initial_selection,
                                     const std::function<void(int)>& onSelectionChange) {
    UIRadioSet rs;
    rs.pos = pos;
    rs.size = size;
    rs.options = options;
    rs.selected_index = std::clamp(initial_selection, 0, static_cast<int>(options.size()) - 1);
    rs.option_height = std::max(24.0f, size.y / std::max<size_t>(1, options.size()));
    rs.onSelectionChange = onSelectionChange;
    LOGI("[UI add_radio_set] pos=(%.0f,%.0f) size=(%.0f,%.0f) options=%zu initial=%d",
         pos.x, pos.y, size.x, size.y, options.size(), rs.selected_index);
    radio_sets_.push_back(rs);
    return &radio_sets_.back();
}

UIButton* UIManager::find_button_by_tag(const std::string& tag) {
    for (auto& btn : buttons_) {
        if (btn.tag == tag) return &btn;
    }
    return nullptr;
}

UIScrollList* UIManager::find_scroll_list_by_tag(const std::string& tag) {
    for (auto& sl : scroll_lists_) {
        if (sl.tag == tag) return &sl;
    }
    return nullptr;
}

UIRadioSet* UIManager::find_radio_set_by_tag(const std::string& tag) {
    for (auto& rs : radio_sets_) {
        if (rs.tag == tag) return &rs;
    }
    return nullptr;
}

UIList* UIManager::add_list(const glm::vec2& pos,
                            const glm::vec2& size,
                            const std::vector<std::string>& items,
                            int initial_selection,
                            const std::function<void(int)>& onSelectionChange) {
    UIList lst;
    lst.pos = pos;
    lst.size = size;
    lst.items = items;
    lst.selected_index = std::clamp(initial_selection, -1, static_cast<int>(items.size()) - 1);
    lst.item_height = std::max(20.0f, size.y / std::max<size_t>(1, items.size()));
    lst.onSelectionChange = onSelectionChange;
    lst.item_colors.assign(items.size(), 0xFFFFD000);
    LOGI("[UI add_list] pos=(%.0f,%.0f) size=(%.0f,%.0f) items=%zu initial=%d",
         pos.x, pos.y, size.x, size.y, items.size(), lst.selected_index);
    lists_.push_back(lst);
    return &lists_.back();
}

UIList* UIManager::find_list_by_tag(const std::string& tag) {
    for (auto& lst : lists_) {
        if (lst.tag == tag) return &lst;
    }
    return nullptr;
}

bool UIManager::hit_test_list(const UIList& lst, float x, float y) {
    if (x >= lst.pos.x && x <= (lst.pos.x + lst.size.x) &&
        y >= lst.pos.y && y <= (lst.pos.y + lst.size.y)) {
        return true;
    }
    return false;
}

} // namespace aom
