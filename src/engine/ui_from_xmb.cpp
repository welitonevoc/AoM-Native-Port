/**
 *  Generate UI buttons from the parsed XMB configuration.
 *
 *  The XMB file is expected to contain a <menu> node with <item> children.
 *  Each <item> may have the following attributes:
 *    - name   : button label (text)
 *    - action : string identifier that will be logged when the button is pressed
 *    - icon   : optional path to a PNG icon (relative to the assets/ folder)
 */

#include "engine/engine.h"
#include <android/log.h>

#define LOG_TAG "AoM_UIFromXmb"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

namespace aom {

bool Engine::generate_ui_from_xmb() {
    if (!ui_ || !game_config_root_) return false;

    const XmbElement* menu_root = game_config_root_->find_child("menu");
    if (!menu_root) {
        LOGI("XMB does not contain a <menu> node – skipping UI generation");
        return false;
    }

    const float btn_w = 200.0f;
    const float btn_h = 40.0f;
    const float start_x = 10.0f;
    float start_y = 200.0f;
    const float spacing = 10.0f;
    size_t idx = 0;

    for (const auto& child : menu_root->children) {
        if (child->name != "item") continue;
        std::string label = child->get_attribute("name");
        std::string action = child->get_attribute("action");
        std::string icon_path = child->get_attribute("icon");
        if (label.empty()) continue;

        glm::vec2 pos(start_x, start_y + idx * (btn_h + spacing));

        std::function<void()> callback;
        if (action == "play_game" || action == "start_game") {
            callback = [this]() { this->start_game(); };
        } else if (action == "open_options" || action == "options") {
            callback = [this]() { this->open_options(); };
        } else if (action == "quit" || action == "exit") {
            callback = [this]() { this->quit_app(); };
        } else {
            callback = [action]() {
                LOGI("[XMB MENU] Action triggered: %s", action.c_str());
            };
        }

        ui_->add_button(pos, {btn_w, btn_h},
            std::move(callback),
            0xAA221105,  // Pedra escura AoM (não azul!)
            icon_path);
        ++idx;
    }
    return idx > 0;
}

} // namespace aom
