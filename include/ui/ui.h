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
    TextureHandle rollover_tex = INVALID_TEXTURE;  // stateentry[2] - mouse over
    TextureHandle glow_tex = INVALID_TEXTURE;      // stateentry[3] - focus/glow
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

    // Hover/rollover state (for stateButton rollover/glow)
    bool is_hovered = false;

    // EditBox fields (XMB editBox)
    bool is_edit_box = false;
    std::string placeholder;          // Texto placeholder quando vazio
    std::function<void(const std::string&)> onTextInput;  // Callback quando texto digitado
    size_t max_length = 256;          // Comprimento máximo do texto
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

/// ScrollList / Dropdown widget (XMB scrollList)
struct UIScrollList {
    glm::vec2 pos;
    glm::vec2 size;
    std::vector<std::string> items;          // Lista de itens (ex: "640 x 480", "800 x 600")
    std::vector<uint32_t> item_colors;       // Cor por item (opcional)
    int selected_index = 0;                  // Item atualmente selecionado
    bool expanded = false;                   // Se o dropdown está aberto
    float item_height = 24.0f;               // Altura de cada item
    TextureHandle bg_tex = INVALID_TEXTURE;  // Textura de fundo (ui\ui scroll middle tile)
    TextureHandle top_cap_tex = INVALID_TEXTURE;  // Top cap
    TextureHandle bottom_cap_tex = INVALID_TEXTURE; // Bottom cap
    uint32_t text_color = 0xFFFFD000;
    float text_size = 16.0f;
    uint32_t tex_width = 0;                  // Largura da textura de fundo
    uint32_t tex_height = 0;                 // Altura da textura de fundo
    std::function<void(int)> onSelectionChange;  // Callback com novo índice
    bool visible = true;
    std::string tag;
    std::string group_tag;
};

/// RadioSet widget (XMB radioset) - grupo de radio buttons mutuamente exclusivos
struct UIRadioSet {
    glm::vec2 pos;
    glm::vec2 size;
    std::vector<std::string> options;        // Labels das opções (ex: "High", "Low")
    int selected_index = 0;                  // Índice da opção selecionada
    float option_height = 36.0f;             // Altura de cada opção
    TextureHandle checked_tex = INVALID_TEXTURE;    // ui\ui button radial 32x32 clicked
    TextureHandle unchecked_tex = INVALID_TEXTURE;  // ui\ui button radial 32x32
    uint32_t text_color = 0xFFFFD000;
    float text_size = 18.0f;
    std::function<void(int)> onSelectionChange;  // Callback com novo índice
    bool visible = true;
    std::string tag;
    std::string group_tag;
};

/// List widget (XMB list) - lista de itens selecionáveis (ex: lista de jogos LAN)
struct UIList {
    glm::vec2 pos;
    glm::vec2 size;
    std::vector<std::string> items;          // Itens da lista
    std::vector<uint32_t> item_colors;       // Cor por item
    int selected_index = -1;                 // Item selecionado (-1 = nenhum)
    float item_height = 24.0f;               // Altura de cada item
    TextureHandle bg_tex = INVALID_TEXTURE;  // Textura de fundo
    TextureHandle sel_tex = INVALID_TEXTURE; // Textura de seleção (highlight)
    uint32_t text_color = 0xFFFFD000;
    float text_size = 16.0f;
    std::function<void(int)> onSelectionChange;  // Callback com novo índice
    bool visible = true;
    std::string tag;
    std::string group_tag;
};

/// Virtual Keyboard (on-screen) para entrada de texto em editBox
struct UIVirtualKeyboard {
    enum Layout { LAYOUT_ALPHA, LAYOUT_NUMERIC, LAYOUT_SYMBOLS };
    glm::vec2 pos;
    glm::vec2 size;
    Layout layout = LAYOUT_ALPHA;
    bool shift = false;
    bool visible = false;
    UIButton* target_edit_box = nullptr;  // EditBox que está recebendo input
    std::function<void(const std::string&)> onKeyPress;  // Callback: caractere pressionado
    std::function<void()> onBackspace;     // Callback: backspace
    std::function<void()> onEnter;         // Callback: enter/fechar
    std::function<void()> onLayoutChange;  // Callback: trocar layout
    std::string tag;
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

    /** Add a scroll list / dropdown (XMB scrollList) */
    UIScrollList* add_scroll_list(const glm::vec2& pos,
                                  const glm::vec2& size,
                                  const std::vector<std::string>& items,
                                  int initial_selection,
                                  const std::function<void(int)>& onSelectionChange);

    /** Add a radio button set (XMB radioset) */
    UIRadioSet* add_radio_set(const glm::vec2& pos,
                              const glm::vec2& size,
                              const std::vector<std::string>& options,
                              int initial_selection,
                              const std::function<void(int)>& onSelectionChange);

    /** Add a list widget (XMB list) */
    UIList* add_list(const glm::vec2& pos,
                     const glm::vec2& size,
                     const std::vector<std::string>& items,
                     int initial_selection,
                     const std::function<void(int)>& onSelectionChange);

    void set_group_visible(const std::string& tag, bool visible);

    // EditBox IME focus callback
    using EditBoxFocusCallback = std::function<void(UIButton*, bool)>;  // btn, focused
    void set_edit_box_focus_callback(EditBoxFocusCallback cb) { on_edit_box_focus_ = std::move(cb); }

    // Virtual Keyboard (on-screen)
    UIVirtualKeyboard* get_virtual_keyboard() { return &virtual_keyboard_; }
    void show_virtual_keyboard(UIButton* target);
    void hide_virtual_keyboard();
    bool handle_virtual_keyboard_touch(const TouchEvent& ev);
    void render_virtual_keyboard();

    [[nodiscard]] const std::vector<UIButton>& buttons() const { return buttons_; }
    [[nodiscard]] const std::vector<UILabel>& labels() const { return labels_; }
    [[nodiscard]] const std::vector<UIAnimation>& animations() const { return animations_; }
    [[nodiscard]] const std::vector<UIScrollList>& scroll_lists() const { return scroll_lists_; }
    [[nodiscard]] const std::vector<UIRadioSet>& radio_sets() const { return radio_sets_; }
    [[nodiscard]] const std::vector<UIList>& lists() const { return lists_; }
    std::vector<UIButton>& buttons_mut() { return buttons_; }
    std::vector<UILabel>& labels_mut() { return labels_; }
    std::vector<UIScrollList>& scroll_lists_mut() { return scroll_lists_; }
    std::vector<UIRadioSet>& radio_sets_mut() { return radio_sets_; }
    std::vector<UIList>& lists_mut() { return lists_; }

    // Find widgets by tag (for XMB callback value reading)
    UIButton* find_button_by_tag(const std::string& tag);
    UIScrollList* find_scroll_list_by_tag(const std::string& tag);
    UIRadioSet* find_radio_set_by_tag(const std::string& tag);
    UIList* find_list_by_tag(const std::string& tag);

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
        for (auto& sl : scroll_lists_) {
            for (auto& item : sl.items) resolve_one(item);
        }
        for (auto& rs : radio_sets_) {
            for (auto& opt : rs.options) resolve_one(opt);
        }
        for (auto& lst : lists_) {
            for (auto& item : lst.items) resolve_one(item);
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
    std::vector<UIScrollList> scroll_lists_;
    std::vector<UIRadioSet> radio_sets_;
    std::vector<UIList> lists_;

    EditBoxFocusCallback on_edit_box_focus_;
    UIVirtualKeyboard virtual_keyboard_;

    static bool hit_test(const UIButton& btn, float x, float y);
    static bool hit_test_scroll_list(const UIScrollList& sl, float x, float y);
    static bool hit_test_radio_set(const UIRadioSet& rs, float x, float y);
    static bool hit_test_list(const UIList& lst, float x, float y);
};

} // namespace aom
