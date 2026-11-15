#include "ui/ui.h"
#include "ui/menu_system.h"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>

extern "C" {
#include "stb_image.h"
}

namespace aom {

MenuSystem::MenuSystem() 
    : current_state_(MenuState::MAIN_MENU)
    , selected_button_index_(0)
    , animation_time_(0.0f)
    , assets_loaded_(false)
    , is_active_(false)
    , renderer_(nullptr) {}

MenuSystem::~MenuSystem() = default;

void MenuSystem::initialize(class IRenderer* renderer) {
    renderer_ = renderer;
    load_menu_assets();
}

void MenuSystem::update(float dt) {
    if (!is_active_) return;
    
    update_button_states(dt);
    update_current_menu_state(dt);
    process_menu_actions();
}

void MenuSystem::render() {
    if (!is_active_ || !renderer_) return;
    
    render_current_menu();
}

void MenuSystem::handle_input(const TouchEvent& touch) {
    if (!is_active_) return;
    
    for (size_t i = 0; i < buttons_.size(); ++i) {
        MenuButton& button = buttons_[i];
        
        if (!button.interactive) continue;
        
        bool is_touching = touch.x >= button.position.x &&
                          touch.x <= button.position.x + button.size.x &&
                          touch.y >= button.position.y &&
                          touch.y <= button.position.y + button.size.y;
        
        if (is_touching && touch.state == TouchEvent::State::DOWN) {
            button.current_color = button.pressed_color;
            button.selected = true;
            selected_button_index_ = static_cast<int>(i);
        } else if (is_touching && touch.state == TouchEvent::State::UP) {
            if (button.selected) {
                button.action();
            }
            button.selected = false;
        }
    }
}

void MenuSystem::handle_input(const MenuAction& action) {
    if (!is_active_) return;
    
    switch (action) {
        case MenuAction::UP:
            if (selected_button_index_ > 0) {
                buttons_[selected_button_index_].selected = false;
                selected_button_index_--;
                buttons_[selected_button_index_].selected = true;
            }
            break;
            
        case MenuAction::DOWN:
            if (selected_button_index_ < (int)buttons_.size() - 1) {
                buttons_[selected_button_index_].selected = false;
                selected_button_index_++;
                buttons_[selected_button_index_].selected = true;
            }
            break;
            
        case MenuAction::SELECT:
            if (selected_button_index_ >= 0 && 
                selected_button_index_ < (int)buttons_.size()) {
                buttons_[selected_button_index_].action();
            }
            break;
            
        case MenuAction::CANCEL:
            if (current_state_ != MenuState::MAIN_MENU) {
                set_state(MenuState::MAIN_MENU);
            }
            break;
            
        default:
            break;
    }
}

void MenuSystem::set_state(MenuState state) {
    current_state_ = state;
    selected_button_index_ = 0;
    animation_time_ = 0.0f;
    
    for (auto& button : buttons_) {
        button.selected = false;
        button.is_highlights = false;
    }
}

void MenuSystem::add_button(const std::string& id, const std::string& label, 
                           const std::function<void()>& action,
                           const glm::vec2& position, const glm::vec2& size,
                           TextureHandle normal_tex, TextureHandle pressed_tex) {
    MenuButton button;
    button.id = id;
    button.menu_id = id;
    button.label = label;
    button.action = action;
    button.position = position;
    button.size = size;
    button.normal_tex = normal_tex;
    button.pressed_tex = pressed_tex;
    button.selected = false;
    button.is_highlights = false;
    
    if (normal_tex == INVALID_TEXTURE) {
        button.normal_color = 0xFF2A5EFF;
        button.pressed_color = 0xFF1E4A8DFF;
    }
    
    buttons_.push_back(button);
}

void MenuSystem::remove_button(const std::string& id) {
    auto it = std::remove_if(buttons_.begin(), buttons_.end(),
                            [&id](const MenuButton& btn) { return btn.menu_id == id; });
    buttons_.erase(it, buttons_.end());
}

void MenuSystem::clear_buttons() {
    buttons_.clear();
    selected_button_index_ = 0;
}

// Helper: load a PNG asset and create a texture
static TextureHandle load_png_texture(IRenderer* renderer, const std::string& asset_name) {
    if (!renderer) return INVALID_TEXTURE;
    
    // Read asset bytes - this needs access to asset manager
    // For now, return INVALID_TEXTURE; the engine loads textures separately
    // and passes them via add_button() parameters
    (void)asset_name;
    return INVALID_TEXTURE;
}

void MenuSystem::load_menu_assets() {
    if (!renderer_) return;
    
    // Textures are loaded by the engine and passed via add_button() parameters
    // MenuSystem does not directly load assets from files
    assets_loaded_ = true;
}

void MenuSystem::update_button_states(float dt) {
    animation_time_ += dt;
    
    for (auto& button : buttons_) {
        if (button.selected) {
            float pulse = 1.0f + 0.1f * std::sin(animation_time_ * 2.0f);
            button.size = glm::vec2(280 * pulse, 44 * pulse);
            button.position = glm::vec2(340.0f - button.size.x / 2.0f, 
                                       300.0f - button.size.y / 2.0f);
        }
    }
}

void MenuSystem::render_current_menu() {
    if (!assets_loaded_ || !renderer_) return;
    
    render_menu_background();
    render_menu_buttons();
    render_selection_indicator();
}

void MenuSystem::render_menu_background() {
    if (main_menu_bg_tex_ != INVALID_TEXTURE) {
        renderer_->draw_ui_quad({0.0f, 0.0f}, {800.0f, 600.0f}, main_menu_bg_tex_, 0xFFFFFFFF);
    } else {
        renderer_->draw_ui_quad({0.0f, 0.0f}, {800.0f, 600.0f}, INVALID_TEXTURE, 0xFF000000FF);
    }
}

void MenuSystem::render_menu_buttons() {
    for (size_t i = 0; i < buttons_.size(); ++i) {
        MenuButton& button = buttons_[i];
        
        TextureHandle current_tex = button.selected ? 
                                   (button.pressed_tex != INVALID_TEXTURE ? 
                                    button.pressed_tex : button.normal_tex) : 
                                   button.normal_tex;
        
        if (current_tex != INVALID_TEXTURE) {
            renderer_->draw_ui_quad(button.position, button.size, current_tex, button.current_color);
        } else {
            renderer_->draw_ui_quad(button.position, button.size, INVALID_TEXTURE, button.current_color);
        }
        
        if (!button.label.empty() && button.is_big_button) {
            glm::vec2 text_pos = button.position + glm::vec2(10.0f, 10.0f);
            renderer_->draw_ui_text(button.label, text_pos, button.text_size, button.text_color);
        }
    }
}

void MenuSystem::render_selection_indicator() {
    if (selected_button_index_ >= 0 && 
        selected_button_index_ < (int)buttons_.size()) {
        
        MenuButton& button = buttons_[selected_button_index_];
        
        glm::vec2 cursor_pos = button.position - glm::vec2(20.0f, 0.0f);
        renderer_->draw_ui_quad(cursor_pos, {10.0f, button.size.y}, INVALID_TEXTURE, 0xFFFFFF00);
    }
}

void MenuSystem::update_current_menu_state(float dt) {
    switch (current_state_) {
        case MenuState::MAIN_MENU:
            break;
        default:
            break;
    }
}

void MenuSystem::process_menu_actions() {
}

} // namespace aom