#include "ui/ui.h"
#include <algorithm>
#include <iostream>

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
        UIButton& button = buttons_[i];
        
        if (!button.interactive) continue;
        
        bool is_touching = touch.x >= button.pos.x &&
                          touch.x <= button.pos.x + button.size.x &&
                          touch.y >= button.pos.y &&
                          touch.y <= button.pos.y + button.size.y;
        
        if (is_touching && touch.state == TouchEvent::State::DOWN) {
            button.current_color = button.pressed_color;
            button.is_selected = true;
            selected_button_index_ = static_cast<int>(i);
        } else if (is_touching && touch.state == TouchEvent::State::UP) {
            if (button.is_selected) {
                button.action();
            }
            button.is_selected = false;
        }
    }
}

void MenuSystem::handle_input(const MenuAction& action) {
    if (!is_active_) return;
    
    switch (action) {
        case MenuAction::UP:
            if (selected_button_index_ > 0) {
                buttons_[selected_button_index_].is_selected = false;
                selected_button_index_--;
                buttons_[selected_button_index_].is_selected = true;
            }
            break;
            
        case MenuAction::DOWN:
            if (selected_button_index_ < (int)buttons_.size() - 1) {
                buttons_[selected_button_index_].is_selected = false;
                selected_button_index_++;
                buttons_[selected_button_index_].is_selected = true;
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
        button.is_selected = false;
        button.is_highlights = false;
    }
}

void MenuSystem::add_button(const std::string& id, const std::string& label, 
                           const std::function<void()>& action,
                           const glm::vec2& position, const glm::vec2& size,
                           TextureHandle normal_tex, TextureHandle pressed_tex) {
    UIButton button;
    button.menu_id = id;
    button.label = label;
    button.action = action;
    button.pos = position;
    button.size = size;
    button.normal_tex = normal_tex;
    button.pressed_tex = pressed_tex;
    button.is_selected = false;
    button.is_highlights = false;
    
    if (normal_tex == INVALID_TEXTURE) {
        button.normal_color = 0xFF2A5EFF;
        button.pressed_color = 0xFF1E4A8DFF;
    }
    
    buttons_.push_back(button);
}

void MenuSystem::remove_button(const std::string& id) {
    auto it = std::remove_if(buttons_.begin(), buttons_.end(),
                            [&id](const UIButton& btn) { return btn.menu_id == id; });
    buttons_.erase(it, buttons_.end());
}

void MenuSystem::clear_buttons() {
    buttons_.clear();
    selected_button_index_ = 0;
}

void MenuSystem::load_menu_assets() {
    if (!renderer_) return;
    
    main_menu_bg_tex_ = renderer_->load_texture("assets/main_menu.png");
    button_tex_ = renderer_->load_texture("assets/button_main.png");
    button_pressed_tex_ = renderer_->load_texture("assets/button_clicked.png");
    
    assets_loaded_ = true;
}

void MenuSystem::update_button_states(float dt) {
    animation_time_ += dt;
    
    for (auto& button : buttons_) {
        if (button.is_selected) {
            float pulse = 1.0f + 0.1f * std::sin(animation_time_ * 2.0f);
            button.size = glm::vec2(280 * pulse, 44 * pulse);
            button.pos = glm::vec2(340 - button.size.x / 2, 
                                   300 - button.size.y / 2);
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
    if (main_menu_bg_tex_) {
        renderer_->draw_texture(main_menu_bg_tex_, {0, 0});
    } else {
        renderer_->draw_rect({0, 0, 800, 600}, 0xFF000000FF);
    }
}

void MenuSystem::render_menu_buttons() {
    for (size_t i = 0; i < buttons_.size(); ++i) {
        UIButton& button = buttons_[i];
        
        TextureHandle current_tex = button.is_selected ? 
                                   (button.pressed_tex != INVALID_TEXTURE ? 
                                    button.pressed_tex : button.normal_tex) : 
                                   button.normal_tex;
        
        if (current_tex != INVALID_TEXTURE) {
            renderer_->draw_texture(current_tex, button.pos, button.size);
        } else {
            renderer_->draw_rect({
                button.pos.x, button.pos.y, 
                button.size.x, button.size.y
            }, button.current_color);
        }
        
        if (!button.label.empty() && button.is_big_button) {
            glm::vec2 text_pos = button.pos + glm::vec2(10, 10);
            renderer_->draw_text("fonts/defcon.ttf", button.label, 
                               text_pos, button.text_color, 
                               button.text_size);
        }
    }
}

void MenuSystem::render_selection_indicator() {
    if (selected_button_index_ >= 0 && 
        selected_button_index_ < (int)buttons_.size()) {
        
        UIButton& button = buttons_[selected_button_index_];
        
        glm::vec2 cursor_pos = button.pos - glm::vec2(20, 0);
        renderer_->draw_rect({
            cursor_pos.x, cursor_pos.y, 10, button.size.y
        }, 0xFFFFFF00);
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