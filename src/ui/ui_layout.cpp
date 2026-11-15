#include "ui/ui_layout.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <sstream>
#include <android/log.h>

#define LOG_TAG "AoM_UILayout"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace aom {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

std::string to_lower(std::string_view s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return out;
}

/// Faz split de uma string por espaços, retornando floats
std::vector<float> parse_floats(const std::string& str) {
    std::vector<float> out;
    std::istringstream iss(str);
    float v;
    while (iss >> v) {
        out.push_back(v);
    }
    return out;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// UILayoutManager
// ---------------------------------------------------------------------------

glm::vec4 UILayoutManager::convert_size1024(const std::string& size1024,
                                              int screen_w, int screen_h) {
    // size1024 no AoM = "left top right bottom" (coordenadas absolutas em base 1024x768)
    // NÃO é "x y w h" - precisamos calcular w = right - left, h = bottom - top
    auto vals = parse_floats(size1024);
    float left = 0, top = 0, right = 0, bottom = 0;
    if (vals.size() >= 1) left = vals[0];
    if (vals.size() >= 2) top = vals[1];
    if (vals.size() >= 3) right = vals[2];
    if (vals.size() >= 4) bottom = vals[3];

    float w_1024 = std::max(0.0f, right - left);
    float h_1024 = std::max(0.0f, bottom - top);

    // Converte de base 1024x768 para a resolução atual
    float sx = static_cast<float>(screen_w) / kRefWidth;
    float sy = static_cast<float>(screen_h) / kRefHeight;

    return glm::vec4(left * sx, top * sy, w_1024 * sx, h_1024 * sy);
}

glm::vec3 UILayoutManager::parse_color(const std::string& color_str) {
    auto vals = parse_floats(color_str);
    if (vals.size() >= 3) {
        return glm::vec3(vals[0], vals[1], vals[2]);
    }
    return glm::vec3(160, 160, 160); // cinza padrão do AoM
}

uint32_t UILayoutManager::parse_color_rgba(const std::string& color_str, uint8_t alpha) {
    auto c = parse_color(color_str);
    uint8_t r = static_cast<uint8_t>(std::clamp(c.x, 0.0f, 255.0f));
    uint8_t g = static_cast<uint8_t>(std::clamp(c.y, 0.0f, 255.0f));
    uint8_t b = static_cast<uint8_t>(std::clamp(c.z, 0.0f, 255.0f));
    return (static_cast<uint32_t>(r) << 24) |
           (static_cast<uint32_t>(g) << 16) |
           (static_cast<uint32_t>(b) << 8) |
           static_cast<uint32_t>(alpha);
}

UILayoutResult UILayoutManager::load_layout(BarArchive& bar, const std::string& xmb_name) {
    UILayoutResult result;
    result.layout_name = xmb_name;

    const BarEntry* entry = bar.find(xmb_name);
    if (!entry) {
        result.error_message = "Entrada '" + xmb_name + "' não encontrada no BAR";
        LOGE("%s", result.error_message.c_str());
        return result;
    }

    auto data = bar.extract(*entry);
    if (data.empty()) {
        result.error_message = "Falha ao extrair '" + xmb_name + "'";
        LOGE("%s", result.error_message.c_str());
        return result;
    }

    return load_layout_from_data(xmb_name, data);
}

UILayoutResult UILayoutManager::load_layout_from_data(const std::string& layout_name,
                                                       const std::vector<uint8_t>& data) {
    UILayoutResult result;
    result.layout_name = layout_name;

    auto root = XmbParser::parse(data);
    if (!root) {
        result.error_message = "Falha ao fazer parse do XMB: " + layout_name;
        LOGE("%s", result.error_message.c_str());
        return result;
    }

    // O root deve ser <layout>
    if (root->name != "layout") {
        LOGW("Root element is '<%s>', expected '<layout>' for %s",
             root->name.c_str(), layout_name.c_str());
        // Ainda tentamos processar os children
    }

    // Precisamos da resolução de tela para converter size1024.
    // Como não temos acesso direto à tela aqui, usamos a referência 1024x768
    // e a conversão será ajustada pelo chamador via apply_layout_to_ui.
    // Por enquanto, armazenamos com tela de referência (conversão 1:1).
    const int ref_w = kRefWidth;
    const int ref_h = kRefHeight;

    for (const auto& child : root->children) {
        if (!child) continue;
        UIGadgetDef gd;
        parse_gadget(child.get(), ref_w, ref_h, gd);
        result.root_gadgets.push_back(std::move(gd));
    }

    result.success = true;
    layouts_[layout_name] = result;
    LOGI("UI layout '%s' carregado: %zu gadgets raiz",
         layout_name.c_str(), result.root_gadgets.size());

    return result;
}

void UILayoutManager::parse_gadget(const XmbElement* elem, int screen_w, int screen_h,
                                    UIGadgetDef& out) {
    if (!elem) return;

    out.name = elem->get_attribute("name");
    out.type = elem->get_attribute("type");
    if (out.type.empty()) out.type = "gadget";

    // size1024 -> rect em pixels (formato AoM: left top right bottom)
    if (elem->has_attribute("size1024")) {
        auto rect = convert_size1024(elem->get_attribute("size1024"), screen_w, screen_h);
        out.rect.x = rect.x;
        out.rect.y = rect.y;
        out.rect.w = rect.z;
        out.rect.h = rect.w;
    }

    // Texto
    out.text = elem->text;

    // Cores
    if (elem->has_attribute("textcolor")) {
        out.text_color = parse_color(elem->get_attribute("textcolor"));
    }
    if (elem->has_attribute("backgroundcolor")) {
        out.bg_color = parse_color(elem->get_attribute("backgroundcolor"));
    }

    // Fonte
    out.text_font = elem->get_attribute("textfont");
    if (elem->has_attribute("textfontsize")) {
        try {
            out.text_size = std::stof(elem->get_attribute("textfontsize"));
        } catch (...) {
            out.text_size = 18.0f;
        }
    }

    // Textura de fundo
    out.bg_texture = elem->get_attribute("background");

    // Flags
    out.has_bevel = elem->has_attribute("bevel");
    out.close_button = elem->has_attribute("closebutton");
    out.title_bar = elem->get_attribute("titlebar");
    out.hidden = elem->has_attribute("hidden");

    // has_background: true se o gadget tem background/textura definida no XMB
    // (usado para saber se devemos renderizar painel de fundo)
    out.has_background = elem->has_attribute("background") ||
                         elem->has_attribute("backgroundcolor") ||
                         elem->has_attribute("bevel");

    // Ação do botão (ex: "start_game", "open_options", "open_credits", "quit_app")
    out.action = elem->get_attribute("action");

    // Outros atributos (armazena para referência)
    for (const auto& attr : elem->attributes) {
        out.extra_attrs[attr.name] = attr.value;
    }

    // Gadgets filhos (hierarquia)
    for (const auto& child : elem->children) {
        if (!child) continue;
        UIGadgetDef child_def;
        parse_gadget(child.get(), screen_w, screen_h, child_def);
        out.children.push_back(std::move(child_def));
    }
}

size_t apply_layout_to_ui(const UILayoutResult& layout, UIManager* ui,
                           int screen_w, int screen_h,
                           const std::unordered_map<std::string, std::function<void()>>& action_callbacks) {
    if (!ui) return 0;

    size_t created = 0;

    // ============================================================
    // PASS 1: Criar painéis de fundo (backgrounds) - NÃO interativos
    // Estes são gadgets tipo "gadget" com has_background=true
    // Devem ser renderizados PRIMEIRO (z-order inferior)
    // ============================================================
    std::function<void(const UIGadgetDef&, float, float)> apply_backgrounds =
        [&](const UIGadgetDef& gd, float offset_x, float offset_y) {
            if (gd.hidden) return;

            float x = offset_x + gd.rect.x;
            float y = offset_y + gd.rect.y;
            float w = gd.rect.w;
            float h = gd.rect.h;

            float sx = static_cast<float>(screen_w) / UILayoutManager::kRefWidth;
            float sy = static_cast<float>(screen_h) / UILayoutManager::kRefHeight;
            x = gd.rect.x * sx;
            y = gd.rect.y * sy;
            w = gd.rect.w * sx;
            h = gd.rect.h * sy;

            // Background panels: type="gadget" COM has_background=true
            // Criamos como botão NÃO interativo (callback vazio), cor transparente
            // para servir apenas como painel visual de fundo
            if (gd.type == "gadget" && gd.has_background) {
                uint32_t color = 0x00000000; // Totalmente transparente - usa textura do XMB se houver
                if (!gd.extra_attrs.empty() && gd.extra_attrs.count("bgcolor")) {
                    color = UILayoutManager::parse_color_rgba(gd.extra_attrs.at("bgcolor"), 200);
                } else if (!gd.extra_attrs.empty() && gd.extra_attrs.count("backgroundcolor")) {
                    color = UILayoutManager::parse_color_rgba(gd.extra_attrs.at("backgroundcolor"), 200);
                }

                // Callback vazio = não interativo
                std::function<void()> empty_callback = []() {};

                auto* btn = ui->add_button(
                    {x, y}, {w, h},
                    empty_callback,
                    color,
                    0x00000000); // pressed color também transparente
                if (btn) {
                    btn->label = ""; // Sem texto
                    btn->text_size = 0;
                    // IMPORTANTE: background panels não usam texturas padrão de botão
                    // O UIManager deve renderizar apenas a cor/quad
                    ++created;
                }
            }

            // Recursão para filhos (backgrounds aninhados)
            for (const auto& child : gd.children) {
                apply_backgrounds(child, x, y);
            }
        };

    // ============================================================
    // PASS 2: Criar elementos interativos (botões, textos, sliders)
    // Renderizados DEPOIS dos backgrounds (z-order superior)
    // ============================================================
    std::function<void(const UIGadgetDef&, float, float)> apply_interactive =
        [&](const UIGadgetDef& gd, float offset_x, float offset_y) {
            if (gd.hidden) return;

            float x = offset_x + gd.rect.x;
            float y = offset_y + gd.rect.y;
            float w = gd.rect.w;
            float h = gd.rect.h;

            float sx = static_cast<float>(screen_w) / UILayoutManager::kRefWidth;
            float sy = static_cast<float>(screen_h) / UILayoutManager::kRefHeight;
            x = gd.rect.x * sx;
            y = gd.rect.y * sy;
            w = gd.rect.w * sx;
            h = gd.rect.h * sy;

            float scaled_text_size = std::max(14.0f, gd.text_size * sy);

            // Botões interativos (type="button" ou "statebutton")
            if (gd.type == "button" || gd.type == "statebutton") {
                uint32_t color = UILayoutManager::parse_color_rgba("", 255);
                if (!gd.extra_attrs.empty() && gd.extra_attrs.count("textcolor")) {
                    color = UILayoutManager::parse_color_rgba(gd.extra_attrs.at("textcolor"));
                }

                std::function<void()> btn_callback = [gd]() {
                    LOGI("[UI Layout] Botão pressionado: %s", gd.name.c_str());
                };
                if (!gd.action.empty()) {
                    auto it = action_callbacks.find(gd.action);
                    if (it != action_callbacks.end()) {
                        btn_callback = it->second;
                    }
                }

                auto* btn = ui->add_button(
                    {x, y}, {w, h},
                    btn_callback,
                    color,
                    0xFFCCCCCC);
                if (btn) {
                    btn->label = gd.text;
                    btn->text_color = color;
                    btn->text_size = scaled_text_size;
                    // Texturas padrão (button_main_tex_, button_clicked_tex_) aplicadas automaticamente
                    ++created;
                }
            }
            // Textos
            else if (gd.type == "textBox" || gd.type == "textField") {
                uint32_t color = UILayoutManager::parse_color_rgba("", 255);
                if (!gd.text.empty() || gd.extra_attrs.count("textcolor")) {
                    color = UILayoutManager::parse_color_rgba(
                        gd.extra_attrs.count("textcolor")
                            ? gd.extra_attrs.at("textcolor")
                            : "");
                }
                ui->add_label(gd.text, {x, y}, scaled_text_size, color);
                ++created;
            }
            // Sliders
            else if (gd.type == "slider") {
                ui->add_slider({x, y}, {w, h}, gd.text, 0.5f, [](float) {});
                ++created;
            }
            // Containers lógicos invisíveis (gadget sem background) - apenas posiciona filhos
            else if (gd.type == "gadget" && !gd.has_background) {
                // Não cria elemento visual, apenas passa offset para filhos
            }

            // Recursão para filhos interativos
            for (const auto& child : gd.children) {
                apply_interactive(child, x, y);
            }
        };

    // Executa as duas passagens em ordem
    for (const auto& root_gadget : layout.root_gadgets) {
        apply_backgrounds(root_gadget, 0.0f, 0.0f);
    }
    for (const auto& root_gadget : layout.root_gadgets) {
        apply_interactive(root_gadget, 0.0f, 0.0f);
    }

    LOGI("Layout '%s' aplicado ao UI: %zu elementos criados (backgrounds + interativos)",
         layout.layout_name.c_str(), created);
    return created;
}

} // namespace aom
