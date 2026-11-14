#include "ui/ui_layout.h"
#include "ui/button_set.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <sstream>
#include <set>
#include "engine/logger.h"

#define LOG_TAG "AoM_UILayout"
#define LOGI(...) AOM_LOGI(LOG_TAG, __VA_ARGS__)
#define LOGW(...) AOM_LOGW(LOG_TAG, __VA_ARGS__)
#define LOGE(...) AOM_LOGE(LOG_TAG, __VA_ARGS__)

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

/// Remove todos os blocos iniciais no formato $$<número>$$ do texto do XMB.
/// Ex: $$19825$$$$19825$$Learn To Play -> Learn To Play
/// Colapsa espaços à esquerda (o XMB usa espaços para alinhar texto dentro dos $$ID$$).
static std::string strip_string_ids(std::string s) {
    while (s.size() >= 4 && s[0] == '$' && s[1] == '$') {
        size_t end = s.find("$$", 2);
        if (end == std::string::npos) break;
        s.erase(0, end + 2);
    }
    size_t p = s.find_first_not_of(" \t\r\n");
    if (p == std::string::npos) return "";
    size_t q = s.find_last_not_of(" \t\r\n");
    return s.substr(p, q - p + 1);
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

/// Remove espaços em branco e quebras de linha do início e fim da string
std::string trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
        ++start;
    }
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        --end;
    }
    return s.substr(start, end - start);
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
    // Formato ARGB (igual ao restante do engine, ex: gles_renderer.cpp draw_ui_quad)
    return (static_cast<uint32_t>(alpha) << 24) |
           (static_cast<uint32_t>(r) << 16) |
           (static_cast<uint32_t>(g) << 8) |
           static_cast<uint32_t>(b);
}

uint32_t UILayoutManager::parse_color_rgba(const glm::vec3& color, uint8_t alpha) {
    uint8_t r = static_cast<uint8_t>(std::clamp(color.x, 0.0f, 255.0f));
    uint8_t g = static_cast<uint8_t>(std::clamp(color.y, 0.0f, 255.0f));
    uint8_t b = static_cast<uint8_t>(std::clamp(color.z, 0.0f, 255.0f));
    // Formato ARGB (igual ao restante do engine, ex: gles_renderer.cpp draw_ui_quad)
    return (static_cast<uint32_t>(alpha) << 24) |
           (static_cast<uint32_t>(r) << 16) |
           (static_cast<uint32_t>(g) << 8) |
           static_cast<uint32_t>(b);
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

    LOGI("[parse] <%s type='%s'>", out.name.c_str(), out.type.c_str());

    // size1024 -> rect em pixels (formato AoM: left top right bottom)
    if (elem->has_attribute("size1024")) {
        std::string sz = elem->get_attribute("size1024");
        LOGI("[parse]   size1024 raw='%s'", sz.c_str());
        auto rect = convert_size1024(sz, screen_w, screen_h);
        out.rect.x = rect.x;
        out.rect.y = rect.y;
        out.rect.w = rect.z;
        out.rect.h = rect.w;
    }

    // Texto
    out.text = elem->text;

    // Cores (atributos do nó pai)
    if (elem->has_attribute("textcolor")) {
        out.text_color = parse_color(elem->get_attribute("textcolor"));
    }
    if (elem->has_attribute("backgroundcolor")) {
        out.bg_color = parse_color(elem->get_attribute("backgroundcolor"));
        out.bg_color_set = true;
    }

    // ── Leitura de Elementos Filhos ──
    // O XMB do AoM guarda propriedades em nós filhos (<text>, <textcolor>,
    // <background>, <backgroundcolor>, <size1024>), não apenas em atributos.
    // Log do texto direto do elemento (entre as tags de abertura/fechamento).
    std::string elem_text = trim(elem->text);
    if (!elem_text.empty()) {
        LOGI("[XMB] Gadget '%s' tipo='%s' texto_direto='%s'",
             out.name.c_str(), out.type.c_str(), elem_text.c_str());
    }
    for (const auto& child : elem->children) {
        if (!child) continue;

        std::string child_text = trim(child->text);

        LOGI("[XMB] Gadget '%s' tem filho <%s> text='%s'",
             out.name.c_str(), child->name.c_str(), child_text.c_str());

        if (child->name == "text") {
            out.text = child_text;
        } else if (child->name == "textcolor") {
            out.text_color = parse_color(child_text);
            out.extra_attrs["textcolor"] = child_text;
        } else if (child->name == "backgroundcolor") {
            out.bg_color = parse_color(child_text);
            out.extra_attrs["backgroundcolor"] = child_text;
            out.bg_color_set = true;
        } else if (child->name == "background") {
            out.bg_texture = child_text;
        } else if (child->name == "stateentry") {
            // stateentry background="ui\ui pregame button"
            // Captura todos os estados [0]=normal, 1=clicked, 2=rollover, 3=glow
            std::string bg = child->get_attribute("background");
            out.state_backgrounds.push_back(bg);
            if (out.state_backgrounds.size() == 1 && !bg.empty()) {
                out.bg_texture = bg;  // estado normal = fundo padrão do botão
                LOGI("[XMB] Gadget '%s' stateentry background='%s' (estado normal)",
                     out.name.c_str(), bg.c_str());
            }
        } else if (child->name == "command") {
            out.command = child_text;
        } else if (child->name == "size1024") {
            auto rect = convert_size1024(child_text, screen_w, screen_h);
            out.rect.x = rect.x;
            out.rect.y = rect.y;
            out.rect.w = rect.z;
            out.rect.h = rect.w;
        }
    }

    // Se o texto ainda estiver vazio, tenta atributo (fallback)
    if (out.text.empty()) {
        out.text = elem->get_attribute("text");
    }

    // texturecoords -> uv_min / uv_max (formato AoM: u0 v0 u1 v1)
    if (elem->has_attribute("texturecoords")) {
        auto coords = parse_floats(elem->get_attribute("texturecoords"));
        if (coords.size() >= 4) {
            out.texture_coords = glm::vec4(coords[0], coords[1], coords[2], coords[3]);
            out.has_texture_coords = true;
        }
    }

    // z-order
    if (elem->has_attribute("z")) {
        try {
            out.z = std::stoi(elem->get_attribute("z"));
        } catch (...) {
            out.z = 0;
        }
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

    // Textura de fundo: só usa o atributo se o filho <background> não já definiu
    if (out.bg_texture.empty()) {
        out.bg_texture = elem->get_attribute("background");
    }

    // Flags
    out.has_bevel = elem->has_attribute("bevel");
    out.close_button = elem->has_attribute("closebutton");
    out.title_bar = elem->get_attribute("titlebar");
    out.hidden = elem->has_attribute("hidden");

    // has_background: true se há background (atributo ou filho) ou cor de fundo
    out.has_background = elem->has_attribute("background") ||
                         elem->has_attribute("backgroundcolor") ||
                         elem->has_attribute("bevel") ||
                         !out.bg_texture.empty() ||
                         (elem->has_attribute("backgroundcolor"));

    // Ação do botão (ex: "start_game", "open_options", "open_credits", "quit_app")
    out.action = elem->get_attribute("action");

    // Comando nativo do AoM via atributo se não veio como filho <command>
    if (out.command.empty() && elem->has_attribute("command")) {
        out.command = elem->get_attribute("command");
    }

    // Outros atributos (armazena para referência)
    for (const auto& attr : elem->attributes) {
        out.extra_attrs[attr.name] = attr.value;
    }

    // ── LOG [GADGET]: diagnóstico para identificar o gadget azul/retângulo ──
    LOGI("[GADGET] name='%s' type='%s' rect=(%.0f,%.0f,%.0f,%.0f) bg_set=%d bg=(%.0f,%.0f,%.0f) tex='%s' text='%s' n_children=%zu",
         out.name.c_str(), out.type.c_str(),
         out.rect.x, out.rect.y, out.rect.w, out.rect.h,
         out.bg_color_set,
         out.bg_color.x, out.bg_color.y, out.bg_color.z,
         out.bg_texture.c_str(),
         out.text.empty() ? "-" : out.text.c_str(),
         out.children.size());

    // Gadgets filhos (hierarquia) — pula nós que são apenas propriedades
    // (não são gadgets visuais: texto, cor, bg, tamanho, estados do botão e comando de ação)
    static const std::set<std::string> kPropNodes = {"text", "textcolor",
                                                     "background", "backgroundcolor",
                                                     "size1024", "stateentry", "command"};
    for (const auto& child : elem->children) {
        if (!child) continue;
        if (kPropNodes.count(child->name)) continue;
        UIGadgetDef child_def;
        parse_gadget(child.get(), screen_w, screen_h, child_def);
        out.children.push_back(std::move(child_def));
    }
}

size_t apply_layout_to_ui(const UILayoutResult& layout, UIManager* ui,
                           int screen_w, int screen_h,
                           const std::unordered_map<std::string, std::function<void()>>& action_callbacks,
                           TextureResolverFn texture_resolver,
                           const ButtonSetManager* button_sets) {
    if (!ui) return 0;

    size_t created = 0;

    // ============================================================
    // PASS 1: Criar painéis de fundo (backgrounds) - NÃO interativos
    // Estes são gadgets tipo "gadget" com has_background=true
    // Devem ser renderizados PRIMEIRO (z-order inferior)
    // ============================================================
    const float scale = static_cast<float>(screen_h) / UILayoutManager::kRefHeight;
    const float offset_center_x = std::max(0.0f, (static_cast<float>(screen_w) - UILayoutManager::kRefWidth * scale) * 0.5f);

    std::function<void(const UIGadgetDef&, const std::string&, bool)> apply_backgrounds =
        [&](const UIGadgetDef& gd, const std::string& parent_tag, bool parent_hidden) {
            bool is_hidden = parent_hidden || gd.hidden;
            std::string current_tag = gd.name.empty() ? parent_tag : gd.name;

            // size1024 no AoM é coordenada absoluta de tela em base 1024x768
            float x = offset_center_x + gd.rect.x * scale;
            float y = gd.rect.y * scale;
            float w = gd.rect.w * scale;
            float h = gd.rect.h * scale;

            // Background panels: type="gadget" COM has_background=true
            // Criamos como elemento NÃO interativo - apenas painel visual de fundo
            if (gd.type == "gadget" && gd.has_background) {
                // Tenta resolver a textura primeiro se houver
                TextureHandle tex_handle = INVALID_TEXTURE;
                if (!gd.bg_texture.empty() && texture_resolver) {
                    ResolvedTexture rt = texture_resolver(gd.bg_texture);
                    tex_handle = rt.handle;
                    if (tex_handle != INVALID_TEXTURE) {
                        LOGI("UI Layout: background textura '%s' resolvida (handle=%u)",
                             gd.bg_texture.c_str(), rt.handle);
                    } else {
                        LOGW("UI Layout: background textura não encontrada: %s", gd.bg_texture.c_str());
                    }
                }

                // SÓ desenha painel de fundo se:
                // a) Teve textura resolvida com sucesso, OU
                // b) Não tem textura, mas tem cor explícita definida no XMB que não seja azul/preto placeholder
                uint32_t panel_color = 0x00000000;
                if (tex_handle != INVALID_TEXTURE) {
                    panel_color = 0xFFFFFFFF; // Opaco para exibir textura
                } else if (gd.bg_texture.empty() && gd.bg_color_set) {
                    if (gd.bg_color != glm::vec3(0, 0, 255) && gd.bg_color != glm::vec3(0, 0, 0)) {
                        panel_color = UILayoutManager::parse_color_rgba(gd.bg_color);
                    }
                }

                // Se não tem textura nem cor válida, NÃO cria o quad (permanece transparente)
                if (panel_color != 0x00000000 || tex_handle != INVALID_TEXTURE) {
                    auto* btn = ui->add_button(
                        {x, y}, {w, h},
                        []() {}, // callback vazio (não interativo)
                        panel_color,
                        0x00000000  // pressed color também transparente
                    );
                    if (btn) {
                        btn->interactive = false; // painel de fundo não recebe toque
                        btn->visible = !is_hidden;
                        btn->tag = gd.name.empty() ? parent_tag : gd.name;
                        btn->group_tag = parent_tag;
                        btn->label = "";
                        btn->text_size = 0;
                        btn->normal_tex = tex_handle;
                        btn->pressed_tex = tex_handle;
                        btn->current_color = panel_color;
                        if (gd.has_texture_coords) {
                            btn->has_uv = true;
                            btn->uv_min = glm::vec2(gd.texture_coords.x, 1.0f - gd.texture_coords.w);
                            btn->uv_max = glm::vec2(gd.texture_coords.z, 1.0f - gd.texture_coords.y);
                        }
                        ++created;
                    }
                }
            }

            // Recursão para filhos (size1024 é absoluto em base 1024x768)
            for (const auto& child : gd.children) {
                apply_backgrounds(child, current_tag, is_hidden);
            }
        };

    // ============================================================
    // PASS 2: Criar elementos interativos (botões, textos, sliders)
    // Renderizados DEPOIS dos backgrounds (z-order superior)
    // ============================================================
    std::function<void(const UIGadgetDef&, const std::string&, bool)> apply_interactive =
        [&](const UIGadgetDef& gd, const std::string& parent_tag, bool parent_hidden) {
            bool is_hidden = parent_hidden || gd.hidden;
            std::string current_tag = gd.name.empty() ? parent_tag : gd.name;

            // size1024 no AoM é coordenada absoluta de tela em base 1024x768
            float x = offset_center_x + gd.rect.x * scale;
            float y = gd.rect.y * scale;
            float w = gd.rect.w * scale;
            float h = gd.rect.h * scale;

            float scaled_text_size = std::max(14.0f, gd.text_size * scale);

            // Botões interativos (type="button" ou "statebutton", case-insensitive)
            const std::string type_lower = to_lower(gd.type);
            if (type_lower == "button" || type_lower == "statebutton") {
                float btn_w = w;
                float btn_h = h;
                float btn_x = x;
                float btn_y = y;

                // Resolver texto: suporta formato $$ID$$ para strings localizadas
                std::string resolved_text = gd.text;
                if (!resolved_text.empty() && resolved_text.size() >= 4 &&
                    resolved_text.substr(0, 2) == "$$" && resolved_text.substr(resolved_text.size() - 2) == "$$") {
                    std::string id = resolved_text.substr(2, resolved_text.size() - 4);
                }

                uint32_t fill_color = 0xFFFFFFFF;
                if (!gd.bg_texture.empty()) {
                    fill_color = 0xFFFFFFFF; // textura: branco (tint)
                } else if (gd.bg_color_set) {
                    if (gd.bg_color == glm::vec3(0, 0, 255) || gd.bg_color == glm::vec3(0, 0, 0)) {
                        fill_color = 0xAA221105; // Pedra escura AoM (translucida)
                    } else {
                        fill_color = UILayoutManager::parse_color_rgba(gd.bg_color);
                    }
                } else if (gd.extra_attrs.count("backgroundcolor")) {
                    fill_color = UILayoutManager::parse_color_rgba(gd.extra_attrs.at("backgroundcolor"));
                } else {
                    fill_color = 0xAA221105; // Pedra escura AoM (translucida)
                }

                uint32_t text_color = 0xFFFFD000; // ouro AoM (#FFD000)
                if (!gd.extra_attrs.empty() && gd.extra_attrs.count("textcolor")) {
                    text_color = UILayoutManager::parse_color_rgba(gd.extra_attrs.at("textcolor"));
                } else if (gd.text_color != glm::vec3(0, 0, 0)) {
                    text_color = UILayoutManager::parse_color_rgba(gd.text_color);
                }

                std::function<void()> btn_callback = [ui, action_callbacks, cmd = gd.command, name = gd.name]() {
                    LOGI("[UI Layout] Executando comando para '%s': %s", name.c_str(), cmd.c_str());

                    // 1. Processar alternância nativa de submenus (gadgetReal / gadgetUnReal)
                    auto parse_gadget_names = [](const std::string& s, const std::string& prefix) -> std::vector<std::string> {
                        std::vector<std::string> res;
                        size_t p = 0;
                        while ((p = s.find(prefix, p)) != std::string::npos) {
                            size_t start = p + prefix.size();
                            size_t end = s.find(')', start);
                            if (end != std::string::npos) {
                                std::string a = s.substr(start, end - start);
                                while (!a.empty() && (a.front() == '"' || a.front() == ' ' || a.front() == '\t')) a.erase(a.begin());
                                while (!a.empty() && (a.back() == '"' || a.back() == ' ' || a.back() == '\t')) a.pop_back();
                                if (!a.empty()) res.push_back(a);
                                p = end + 1;
                            } else break;
                        }
                        return res;
                    };

                    if (ui) {
                        for (const auto& t : parse_gadget_names(cmd, "gadgetReal(")) {
                            LOGI("[UI Layout] gadgetReal -> %s", t.c_str());
                            ui->set_group_visible(t, true);
                        }
                        for (const auto& t : parse_gadget_names(cmd, "gadgetUnReal(")) {
                            LOGI("[UI Layout] gadgetUnReal -> %s", t.c_str());
                            ui->set_group_visible(t, false);
                        }
                    }

                    // 2. Comandos de jogo e transições
                    if (cmd.find("exit(") != std::string::npos || cmd.find("quit") != std::string::npos) {
                        auto a = action_callbacks.find("quit_app");
                        if (a != action_callbacks.end()) a->second();
                    } else if (cmd.find("modeEnter(\"SinglePlayerSetup\")") != std::string::npos ||
                               cmd.find("startRandomGame") != std::string::npos) {
                        auto a = action_callbacks.find("start_game");
                        if (a != action_callbacks.end()) a->second();
                    } else if (cmd.find("campaignStart") != std::string::npos ||
                               cmd.find("modeEnter(\"Campaign\")") != std::string::npos) {
                        auto a = action_callbacks.find("open_campaign");
                        if (a != action_callbacks.end()) a->second();
                    } else if (cmd.find("subModeEnter(\"PreGame\",\"Options\")") != std::string::npos ||
                               cmd.find("open_options") != std::string::npos) {
                        auto a = action_callbacks.find("open_options");
                        if (a != action_callbacks.end()) a->second();
                    } else if (cmd.find("uiOpenSavedGameBrowser") != std::string::npos) {
                        auto a = action_callbacks.find("uiOpenSavedGameBrowser");
                        if (a != action_callbacks.end()) a->second();
                    } else if (cmd.find("uiOpenRecordGameBrowser") != std::string::npos) {
                        auto a = action_callbacks.find("uiOpenSavedGameBrowser");
                        if (a != action_callbacks.end()) a->second();
                    } else if (cmd.find("doMPSetup") != std::string::npos) {
                        auto a = action_callbacks.find("open_multiplayer");
                        if (a != action_callbacks.end()) a->second();
                    } else if (cmd.find("gadgetReal(\"MainMenuOptions\")") != std::string::npos) {
                        auto a = action_callbacks.find("return_to_main_menu");
                        if (a != action_callbacks.end()) a->second();
                    }
                };

                LOGI("[apply] button label='%s' pos=(%d,%d) size=(%d,%d) color=%08X text_color=%08X bg='%s' hidden=%d",
                     gd.text.c_str(), (int)x, (int)y, (int)w, (int)h, fill_color, text_color,
                     gd.bg_texture.c_str(), (int)is_hidden);

                auto* btn = ui->add_button(
                    {btn_x, btn_y}, {btn_w, btn_h},
                    btn_callback,
                    fill_color,
                    0xFFCCCCCC);
                if (btn) {
                    btn->visible = !is_hidden;
                    btn->tag = gd.name.empty() ? parent_tag : gd.name;
                    btn->group_tag = parent_tag;
                    if (gd.has_texture_coords) {
                        btn->has_uv = true;
                        btn->uv_min = glm::vec2(gd.texture_coords.x, 1.0f - gd.texture_coords.w);
                        btn->uv_max = glm::vec2(gd.texture_coords.z, 1.0f - gd.texture_coords.y);
                    }
                    btn->label = gd.text; // Mantém ID e fallback para resolução via ui->resolve_text
                    btn->text_color = text_color;   // texto ouro, NÃO cinza
                    btn->text_size = scaled_text_size;
                    // 1) Resolver texturas a partir de ButtonSet (usebuttonset ou padrão do AoM)
                    std::string btn_set_name;
                    if (gd.extra_attrs.count("usebuttonset")) {
                        btn_set_name = gd.extra_attrs.at("usebuttonset");
                    } else if (button_sets && gd.bg_texture.empty() && !gd.has_texture_coords) {
                        if (button_sets->has_set("buttonLong")) {
                            btn_set_name = "buttonLong";
                        }
                    }

                    if (button_sets && !btn_set_name.empty() && texture_resolver) {
                        const auto* bs = button_sets->get_set(btn_set_name);
                        if (bs) {
                            if (!bs->normal_tex.empty()) {
                                ResolvedTexture rt = texture_resolver(bs->normal_tex);
                                if (rt.handle != INVALID_TEXTURE) {
                                    btn->normal_tex = rt.handle;
                                    btn->tex_width = rt.width;
                                    btn->tex_height = rt.height;
                                }
                            }
                            if (!bs->depressed_tex.empty()) {
                                ResolvedTexture rt = texture_resolver(bs->depressed_tex);
                                if (rt.handle != INVALID_TEXTURE) {
                                    btn->pressed_tex = rt.handle;
                                } else {
                                    btn->pressed_tex = btn->normal_tex;
                                }
                            } else {
                                btn->pressed_tex = btn->normal_tex;
                            }
                            if (bs->has_texture_coords && !btn->has_uv) {
                                btn->has_uv = true;
                                btn->uv_min = glm::vec2(bs->texture_coords.x, 1.0f - bs->texture_coords.w);
                                btn->uv_max = glm::vec2(bs->texture_coords.z, 1.0f - bs->texture_coords.y);
                            }
                            if (btn->tex_width > 0 && btn->tex_height > 0) {
                                float bw = static_cast<float>(btn->tex_width) * 0.10f;
                                float bh = static_cast<float>(btn->tex_height) * 0.10f;
                                btn->border_left = btn->border_right = std::max(8.0f, bw);
                                btn->border_top = btn->border_bottom = std::max(8.0f, bh);
                            }
                        }
                    }

                    // 2) Se tem gd.bg_texture (ou veio de stateentry), resolver textura autêntica do BAR
                    if (!gd.bg_texture.empty() && texture_resolver) {
                        ResolvedTexture rt = texture_resolver(gd.bg_texture);
                        if (rt.handle != INVALID_TEXTURE) {
                            btn->normal_tex = rt.handle;
                            btn->pressed_tex = rt.handle;
                            btn->tex_width = rt.width;
                            btn->tex_height = rt.height;

                            // Se houver estado clicado explícito nos state_backgrounds[1]
                            if (gd.state_backgrounds.size() > 1 && !gd.state_backgrounds[1].empty()) {
                                ResolvedTexture rt_clk = texture_resolver(gd.state_backgrounds[1]);
                                if (rt_clk.handle != INVALID_TEXTURE) {
                                    btn->pressed_tex = rt_clk.handle;
                                }
                            }

                            if (!btn->has_uv) {
                                // Definir borders de 9-slice apenas se não tiver UV direta
                                float bw = static_cast<float>(rt.width) * 0.10f;
                                float bh = static_cast<float>(rt.height) * 0.10f;
                                if (bw < 8.0f) bw = 8.0f;
                                if (bh < 8.0f) bh = 8.0f;
                                btn->border_left = bw;
                                btn->border_right = bw;
                                btn->border_top = bh;
                                btn->border_bottom = bh;
                            } else {
                                btn->border_left = btn->border_right = 0.0f;
                                btn->border_top = btn->border_bottom = 0.0f;
                            }
                        } else {
                            // ← FALLBACK defensivo: textura falhou, NÃO deixa retângulo branco
                            // Usa pedra escura AoM com texto ouro para que o botão seja perfeitamente legível
                            btn->normal_color = 0xAA221105;
                            btn->pressed_color = 0xAA332211;
                            btn->current_color = btn->normal_color;
                            btn->text_color = 0xFFFFD000;
                            btn->normal_tex = INVALID_TEXTURE;
                            btn->pressed_tex = INVALID_TEXTURE;
                            LOGW("UI Layout: textura nao encontrada: %s", gd.bg_texture.c_str());
                        }
                    }
                    ++created;
                }
            }
            // Textos
            else if (gd.type == "textBox" || gd.type == "textField") {
                // Pular textos de diálogos ocultos
                if (!gd.hidden) {
                    uint32_t color = UILayoutManager::parse_color_rgba("", 255);
                    if (!gd.text.empty() || gd.extra_attrs.count("textcolor")) {
                        color = UILayoutManager::parse_color_rgba(
                            gd.extra_attrs.count("textcolor")
                                ? gd.extra_attrs.at("textcolor")
                                : "");
                    } else if (gd.text_color != glm::vec3(0, 0, 0)) {
                        color = UILayoutManager::parse_color_rgba(gd.text_color);
                    }

                    // Resolver texto: suporta formato $$ID$$
                    std::string resolved_text = gd.text;
                    if (!resolved_text.empty() && resolved_text.size() >= 4 &&
                        resolved_text.substr(0, 2) == "$$" && resolved_text.substr(resolved_text.size() - 2) == "$$") {
                        // ID será resolvido pelo Engine via tr()
                    }

                    auto* lbl = ui->add_label(resolved_text, {x, y}, scaled_text_size, color);
                    if (lbl) {
                        lbl->visible = !is_hidden;
                        lbl->tag = gd.name.empty() ? parent_tag : gd.name;
                        lbl->group_tag = parent_tag;
                    }
                    ++created;
                }
            }
            // Sliders
            else if (gd.type == "slider") {
                auto* s = ui->add_slider({x, y}, {w, h}, gd.text, 0.5f, [](float) {});
                if (s) {
                    s->visible = !is_hidden;
                    s->tag = gd.name.empty() ? parent_tag : gd.name;
                    s->group_tag = parent_tag;
                }
                ++created;
            }
            // Containers lógicos invisíveis (gadget sem background) - apenas posiciona filhos
            else if (gd.type == "gadget" && !gd.has_background) {
                // Não cria elemento visual, apenas passa offset para filhos
            }

            // Recursão para filhos interativos (size1024 é absoluto em base 1024x768)
            for (const auto& child : gd.children) {
                apply_interactive(child, current_tag, is_hidden);
            }
        };

    // Executa as duas passagens em ordem
    for (const auto& root_gadget : layout.root_gadgets) {
        apply_backgrounds(root_gadget, "", false);
    }
    for (const auto& root_gadget : layout.root_gadgets) {
        apply_interactive(root_gadget, "", false);
    }

    LOGI("Layout '%s' aplicado ao UI: %zu elementos criados (backgrounds + interativos)",
         layout.layout_name.c_str(), created);
    return created;
}

} // namespace aom
