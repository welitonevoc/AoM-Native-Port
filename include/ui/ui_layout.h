#pragma once

/// @file ui_layout.h
/// @brief Gerenciador de layouts de UI extraídos do data.bar (XMB).
///        Consome arquivos como uitechtree.xml, uitributedlg.xml, uisound*.xml,
///        etc. e gera elementos do UIManager (botões, labels, etc.) com dados
///        autênticos do Age of Mythology.

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <glm/glm.hpp>
#include "assets/xmb_parser.h"
#include "assets/bar_archive.h"
#include "ui/ui.h"

namespace aom {

class ButtonSetManager;

/// Coordenadas de um gadget em tela (já convertidas de size1024 para pixels)
struct UIGadgetRect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

/// Definição de um gadget de UI carregado do XMB
struct UIGadgetDef {
    std::string name;
    std::string type;           ///< gadget, textBox, textField, slider, button, dropdown, etc.
    UIGadgetRect rect;          ///< Posição e tamanho em pixels
    std::string text;           ///< Conteúdo de texto
    std::string action;         ///< Nome da ação (ex: "start_game", "open_options", "open_credits")
    glm::vec3 text_color{0, 0, 0};      ///< Cor do texto (RGB 0-255)
    float text_size = 18.0f;    ///< Tamanho da fonte
    std::string text_font;      ///< Nome da fonte
    glm::vec3 bg_color{160, 160, 160};  ///< Cor de fundo (RGB 0-255)
    bool bg_color_set = false;          ///< Se backgroundcolor foi definida no XMB
    std::string bg_texture;     ///< Caminho da textura de fundo (ex: "ui\ui_light_gray")
    std::vector<std::string> state_backgrounds;  ///< Texturas de <stateentry> [0]=normal,1=clicked,2=rollover,3=glow
    bool has_bevel = false;
    bool close_button = false;
    std::string title_bar;
    bool hidden = false;        ///< Se true, gadget e filhos não são renderizados
    bool has_background = false;///< Se true, gadget possui background/textura definido no XMB
    glm::vec4 texture_coords{0.0f, 0.0f, 1.0f, 1.0f}; ///< UVs [u0, v0, u1, v1]
    bool has_texture_coords = false;
    std::string command;        ///< Comando nativo AoM (<command> ou atributo)
    int z = 0;                  ///< Z-order do gadget
    std::vector<UIGadgetDef> children;  ///< Gadgets aninhados
    std::unordered_map<std::string, std::string> extra_attrs; ///< Outros atributos
};

/// Resultado do carregamento de um layout
struct UILayoutResult {
    bool success = false;
    std::string layout_name;
    std::vector<UIGadgetDef> root_gadgets;
    std::string error_message;
};

/// Gerenciador de layouts de UI do data.bar
class UILayoutManager {
public:
    UILayoutManager() = default;
    ~UILayoutManager() = default;

    /// Carrega um layout XMB a partir de um arquivo no data.bar
    /// @param bar  Archive de dados aberto (data.bar)
    /// @param xmb_name  Nome do arquivo XMB (ex: "uitechtree.xmb")
    /// @return Resultado do parse
    UILayoutResult load_layout(BarArchive& bar, const std::string& xmb_name);

    /// Carrega um layout XMB a partir de dados brutos (já extraídos)
    UILayoutResult load_layout_from_data(const std::string& layout_name,
                                          const std::vector<uint8_t>& data);

    /// Retorna um layout carregado por nome
    [[nodiscard]] const UILayoutResult* get_layout(const std::string& name) const {
        auto it = layouts_.find(name);
        if (it != layouts_.end()) return &it->second;
        return nullptr;
    }

    /// Converte coordenadas size1024 (base 1024x768) para pixels reais
    [[nodiscard]] static glm::vec4 convert_size1024(const std::string& size1024,
                                                     int screen_w, int screen_h);

    /// Converte uma string de cor "r g b" para glm::vec3
    [[nodiscard]] static glm::vec3 parse_color(const std::string& color_str);

    /// Converte uma string de cor para uint32_t RGBA (0xRRGGBBAA)
    [[nodiscard]] static uint32_t parse_color_rgba(const std::string& color_str,
                                                    uint8_t alpha = 255);

    /// Converte uma cor glm::vec3 (RGB 0-255) para uint32_t ARGB (0xAARRGGBB)
    [[nodiscard]] static uint32_t parse_color_rgba(const glm::vec3& color,
                                                    uint8_t alpha = 255);

private:
    void parse_gadget(const XmbElement* elem, int screen_w, int screen_h,
                       UIGadgetDef& out);

    std::unordered_map<std::string, UILayoutResult> layouts_;

public:
    // Resolução de referência do AoM (tamanho base para size1024)
    static constexpr int kRefWidth = 1024;
    static constexpr int kRefHeight = 768;
};

/// Resultado da resolução de uma textura (handle + dimensões)
struct ResolvedTexture {
    TextureHandle handle = INVALID_TEXTURE;
    uint32_t width = 0;
    uint32_t height = 0;
};

/// Função que resolve um caminho de textura (ex: "ui\\ui_light_gray")
/// para um TextureHandle e suas dimensões em pixels.
using TextureResolverFn = std::function<ResolvedTexture(const std::string&)>;

/// Aplica um layout carregado ao UIManager, criando botões e labels
/// @param layout  Resultado do carregamento do layout
/// @param ui      UIManager destino
/// @param screen_w  Largura da tela em pixels
/// @param screen_h  Altura da tela em pixels
/// @param action_callbacks  Mapa de nome da ação -> callback (ex: "start_game" -> lambda)
/// @param texture_resolver  Função opcional para resolver bg_texture → TextureHandle + dimensões
/// @return Número de elementos criados
size_t apply_layout_to_ui(const UILayoutResult& layout, UIManager* ui,
                           int screen_w, int screen_h,
                           const std::unordered_map<std::string, std::function<void()>>& action_callbacks,
                           TextureResolverFn texture_resolver = nullptr,
                           const ButtonSetManager* button_sets = nullptr);

} // namespace aom
