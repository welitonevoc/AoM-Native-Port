#include "ui/menu_parser.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>

#include "ui/ui.h" // para UIButton

namespace aom {

// Helper to trim whitespace
static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// Helper: busca o conteúdo entre tags <tag> e </tag>
static std::string extract_tag_content(const std::string& xml, const std::string& tag) {
    std::string open_tag = "<" + tag + ">";
    std::string close_tag = "</" + tag + ">";

    size_t open_pos = xml.find(open_tag);
    if (open_pos == std::string::npos) return "";

    size_t content_start = open_pos + open_tag.length();
    size_t close_pos = xml.find(close_tag, content_start);
    if (close_pos == std::string::npos) return "";

    return trim(xml.substr(content_start, close_pos - content_start));
}

bool MenuParser::parse(const std::string& xml_content, MenuParseResult& result) {
    result.success = false;
    result.buttons.clear();

    // Procurar pela tag <menu>
    std::string menu_tag = "<menu>";
    size_t menu_pos = xml_content.find(menu_tag);
    if (menu_pos == std::string::npos) {
        result.error_message = "XML não contém tag <menu> raiz";
        return false;
    }

    // Encontrar o fechamento </menu>
    std::string end_menu_tag = "</menu>";
    size_t end_menu_pos = xml_content.find(end_menu_tag, menu_pos);
    if (end_menu_pos == std::string::npos) {
        result.error_message = "XML tag <menu> fechada não encontrada";
        return false;
    }

    // Extrair o conteúdo entre <menu> e </menu>
    std::string menu_content = xml_content.substr(menu_pos + menu_tag.length(), end_menu_pos - (menu_pos + menu_tag.length()));

    // Parsear cada <button> dentro do <menu>
    size_t pos = 0;
    while ((pos = menu_content.find("<button>", pos)) != std::string::npos) {
        size_t button_end = menu_content.find("</button>", pos);
        if (button_end == std::string::npos) {
            result.error_message = "Botão <button> sem fechamento </button>";
            return false;
        }

        std::string button_content = menu_content.substr(pos, button_end - pos);

        MenuButton btn;
        btn.label = extract_tag_content(button_content, "label");
        btn.action = extract_tag_content(button_content, "action");
        btn.enabled = true;

        // Verificar se ambos foram encontrados
        if (btn.label.empty()) {
            result.error_message = "Botão missing <label> tag";
            return false;
        }
        if (btn.action.empty()) {
            result.error_message = "Botão missing <action> tag em: " + btn.label;
            return false;
        }

        result.buttons.push_back(btn);
        pos = button_end + 9; // pular </button>
    }

    // Se não encontrou botões, é erro
    if (result.buttons.empty()) {
        result.error_message = "Nenhum <button> encontrado no menu XML";
        return false;
    }

    result.success = true;
    return true;
}

std::vector<MenuButton> MenuParser::parse_buttons(const std::string& xml_content) {
    MenuParseResult result;
    if (parse(xml_content, result)) {
        return result.buttons;
    }
    return {};
}

bool MenuParser::load_from_file(const std::string& file_path, MenuParseResult& result) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        result.error_message = "Não pode abrir arquivo: " + file_path;
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return parse(buffer.str(), result);
}

UIButton* button_from_menu(const MenuButton& menu_btn, UIManager* ui, int screen_w, int screen_h) {
    if (!ui) return nullptr;

    // Converter coordenadas do menu (base 1024x768) para pixels da tela
    // O menu XML do AoM usa resolução virtual 1024x768
    float scale_x = static_cast<float>(screen_w) / 1024.0f;
    float scale_y = static_cast<float>(screen_h) / 768.0f;

    glm::vec2 pos(
        menu_btn.rect.x * scale_x,
        menu_btn.rect.y * scale_y
    );
    glm::vec2 size(
        menu_btn.rect.z * scale_x, // width (rect.z)
        menu_btn.rect.w * scale_y  // height (rect.w)
    );

    // Se o botão não tem rect definido ainda, usar posições padrão baseadas na label
    if (menu_btn.rect.w == 0.0f || menu_btn.rect.h == 0.0f) {
        // Posicionamento simples: centralizar e dar um tamanho padrão
        // Isso será sobrescrito pelo layout real depois
        pos = glm::vec2(200 * scale_x, 300 * scale_y);
        size = glm::vec2(200 * scale_x, 50 * scale_y);
    }

    // Criar o callback que dispara a ação
    auto it = std::find_if(
        std::begin(/* future: could search global action map */),
        std::end(/* future: could search global action map */),
        [&menu_btn](const auto&) { return false; } // placeholder
    );

    // Por enquanto, criar um callback que imprime a ação e fecha o menu
    auto callback = [action = menu_btn.action]() {
        // TODO: integrar com o ActionRegistry ou Engine
        // Por enquanto, apenas log
        // __android_log_print(ANDROID_LOG_INFO, "AoM-UI", "Ação disparada: %s", action.c_str());
        // Em um projeto real, isso dispararia o evento no Engine
    };

    // Criar botão no UIManager
    // Cores padrão do AoM: ouro #FFD000
    uint32_t normal_color = 0xFFFFD000;
    uint32_t pressed_color = 0xFFCCCCCC;

    UIButton* button = ui->add_button(
        pos,
        size,
        menu_btn.label,
        callback,
        normal_color,
        pressed_color
    );

    return button;
}

} // namespace aom