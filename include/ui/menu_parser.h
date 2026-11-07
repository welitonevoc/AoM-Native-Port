#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace aom {

/// Dados de um botão extraídos de um menu XML do AoM
struct MenuButton {
    std::string label;      ///< Texto exibido no botão (ex: "Learn To Play")
    std::string action;     ///< Nome da ação a disparar (ex: "start_game")
    glm::vec4 rect;         ///< Retângulo x, y, width, height (será preenchido pelo layout)
    bool enabled = true;    ///< Se o botão está habilitado
};

/// Resultado do parser de menu XML
struct MenuParseResult {
    bool success = false;
    std::string error_message;
    std::vector<MenuButton> buttons;
};

/// Parser para menus XML do Age of Mythology (formato uimainmenu.xml, ui_editor_menu.xml, etc.)
/// Suporta o formato:
/// <menu>
///   <button>
///     <label>Texto</label>
///     <action>nome_acao</action>
///   </button>
/// </menu>
class MenuParser {
public:
    MenuParser() = default;
    ~MenuParser() = default;

    MenuParser(const MenuParser&) = delete;
    MenuParser& operator=(const MenuParser&) = delete;

    /// Parse um arquivo XML de menu e extrai os botões
    /// @param xml_content Conteúdo do arquivo XML como string
    /// @param result Output com os botões parseados e status
    /// @return true se parseou com sucesso
    bool parse(const std::string& xml_content, MenuParseResult& result);

    /// Convenience function: parse e retorna só os botões
    std::vector<MenuButton> parse_buttons(const std::string& xml_content);

    /// Carrega e parseia um arquivo XML direto do disco
    /// @param file_path Caminho para o arquivo .xml
    /// @param result Output com os resultados
    /// @return true se carregou e parseou com sucesso
    bool load_from_file(const std::string& file_path, MenuParseResult& result);
};

/// Converte um MenuButton em UIButton do UIManager
/// O botão será criado na posição relativa; o caller deve posicionar conforme layout
UIButton* button_from_menu(const MenuButton& menu_btn, UIManager* ui, int screen_w, int screen_h);

} // namespace aom