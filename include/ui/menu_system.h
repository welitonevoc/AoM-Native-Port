#pragma once
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <glm/glm.hpp>

namespace aom {

enum class MenuState {
    MAIN_MENU,
    SETTINGS_MENU,
    LOAD_GAME_MENU,
    CREDITS_MENU,
    EXIT_CONFIRM
};

enum class MenuAction {
    NONE,
    SELECT,
    CANCEL,
    UP,
    DOWN,
    LEFT,
    RIGHT
};

struct UIButton {
    std::string id;
    std::string label;
    std::function<void()> action;
    bool disabled = false;
    bool selected = false;
    glm::vec2 position;
    glm::vec2 size;
    uint32_t normal_color = 0xFF2A5EFF;
    uint32_t pressed_color = 0xFF1E4A8DFF;
    uint32_t current_color = 0xFFFFFFFF;
    uint32_t text_color = 0xFFFFD000;
    float text_size = 20.0f;
    TextureHandle normal_tex = INVALID_TEXTURE;
    TextureHandle pressed_tex = INVALID_TEXTURE;
    TextureHandle icon = INVALID_TEXTURE;
    bool is_big_button = false;
    bool interactive = true;
    
    std::string menu_id;
    MenuAction action_on_select = MenuAction::SELECT;
    bool is_highlights = false;
    
    float border_left = 0.0f;
    float border_right = 0.0f;
    float border_top = 0.0f;
    float border_bottom = 0.0f;
    
    uint32_t tex_width = 0;
    uint32_t tex_height = 0;
};

class MenuSystem {
public:
    MenuSystem();
    ~MenuSystem();

    void initialize(class IRenderer* renderer);
    void update(float dt);
    void render();
    void handle_input(const TouchEvent& touch);
    void handle_input(const MenuAction& action);
    
    void set_state(MenuState state);
    MenuState get_state() const { return current_state_; }
    
    void add_button(const std::string& id, const std::string& label, 
                   const std::function<void()>& action,
                   const glm::vec2& position, const glm::vec2& size,
                   TextureHandle normal_tex = INVALID_TEXTURE,
                   TextureHandle pressed_tex = INVALID_TEXTURE);
    
    void remove_button(const std::string& id);
    void clear_buttons();
    
    void set_selected_button(int index);
    int get_selected_button() const { return selected_button_index_; }
    
    bool is_active() const { return is_active_; }
    void set_active(bool active) { is_active_ = active; }
    
    void load_menu_assets();
    bool are_assets_loaded() const { return assets_loaded_; }
    
    int get_button_count() const { return static_cast<int>(buttons_.size()); }

private:
    void update_button_states(float dt);
    void render_current_menu();
    void render_main_menu();
    void render_menu_background();
    void render_menu_buttons();
    void render_selection_indicator();
    void update_current_menu_state(float dt);
    void process_menu_actions();
    
    std::vector<UIButton> buttons_;
    MenuState current_state_;
    int selected_button_index_;
    float animation_time_;
    bool assets_loaded_;
    bool is_active_;
    
    class IRenderer* renderer_;
    TextureHandle main_menu_bg_tex_;
    TextureHandle button_tex_;
    TextureHandle button_pressed_tex_;
};

} // namespace aom