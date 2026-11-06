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
    // size1024 = "x y w h" em coordenadas base 1024x768
    auto vals = parse_floats(size1024);
    float x = 0, y = 0, w = 0, h = 0;
    if (vals.size() >= 1) x = vals[0];
    if (vals.size() >= 2) y = vals[1];
    if (vals.size() >= 3) w = vals[2];
    if (vals.size() >= 4) h = vals[3];

    // Converte de base 1024x768 para a resolução atual
    float sx = static_cast<float>(screen_w) / kRefWidth;
    float sy = static_cast<float>(screen_h) / kRefHeight;

    return glm::vec4(x * sx, y * sy, w * sx, h * sy);
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

    // size1024 -> rect em pixels
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
                           int screen_w, int screen_h) {
    if (!ui) return 0;

    size_t created = 0;

    // Função recursiva para aplicar gadgets
    std::function<void(const UIGadgetDef&, float, float)> apply_gadget =
        [&](const UIGadgetDef& gd, float offset_x, float offset_y) {
            float x = offset_x + gd.rect.x;
            float y = offset_y + gd.rect.y;
            float w = gd.rect.w;
            float h = gd.rect.h;

            // Ajusta coordenadas para a tela real (convert_size1024 já usa
            // kRefWidth/kRefHeight; aqui reaplicamos para screen_w/screen_h
            // caso o layout tenha sido carregado com resolução de referência)
            float sx = static_cast<float>(screen_w) / UILayoutManager::kRefWidth;
            float sy = static_cast<float>(screen_h) / UILayoutManager::kRefHeight;
            x = gd.rect.x * sx;
            y = gd.rect.y * sy;
            w = gd.rect.w * sx;
            h = gd.rect.h * sy;

            if (gd.type == "button" || gd.type == "gadget") {
                // Cria botão
                uint32_t color = UILayoutManager::parse_color_rgba(
                    "", 255); // default branco
                if (!gd.extra_attrs.empty() && gd.extra_attrs.count("textcolor")) {
                    color = UILayoutManager::parse_color_rgba(
                        gd.extra_attrs.at("textcolor"));
                }

                auto* btn = ui_->add_button(
                    {x, y}, {w, h},
                    [gd]() {
                        LOGI("[UI Layout] Botão pressionado: %s", gd.name.c_str());
                    },
                    color,
                    0xFFCCCCCC);
                if (btn) {
                    btn->label = gd.text;
                    btn->text_color = color;
                    btn->text_size = gd.text_size;
                    ++created;
                }
            }
            else if (gd.type == "textBox" || gd.type == "textField") {
                // Cria label de texto
                uint32_t color = UILayoutManager::parse_color_rgba(
                    "", 255);
                if (!gd.text.empty() || gd.extra_attrs.count("textcolor")) {
                    color = UILayoutManager::parse_color_rgba(
                        gd.extra_attrs.count("textcolor")
                            ? gd.extra_attrs.at("textcolor")
                            : "");
                }

                ui_->add_label(gd.text, {x, y}, gd.text_size, color);
                ++created;
            }
            else if (gd.type == "slider") {
                // Slider (usa add_slider com callback vazio por enquanto)
                ui_->add_slider({x, y}, {w, h}, gd.text, 0.5f,
                                [](float) {});
                ++created;
            }

            // Aplica filhos recursivamente
            for (const auto& child : gd.children) {
                apply_gadget(child, x, y);
            }
        };

    for (const auto& root_gadget : layout.root_gadgets) {
        apply_gadget(root_gadget, 0.0f, 0.0f);
    }

    LOGI("Layout '%s' aplicado ao UI: %zu elementos criados",
         layout.layout_name.c_str(), created);
    return created;
}

} // namespace aom
