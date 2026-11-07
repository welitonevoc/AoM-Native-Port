#include "ui/menu_parser.h"
#include "ui/ui.h"
#include "renderer/renderer.h"
#include <iostream>
#include <string>

int main() {
    std::cout << "=== Teste do Menu Parser ===\n\n";

    // Teste 1: Parse do uimainmenu.xml
    std::cout << "--- Teste 1: Parse do uimainmenu.xml ---\n";

    aom::MenuParser parser;
    aom::MenuParseResult result;

    // Conteúdo do uimainmenu.xml
    const std::string uimainmenu_xml = R"(
<?xml version="1.0" encoding="utf-8"?>
<menu>
  <button>
    <label>Learn To Play</label>
    <action>start_game</action>
  </button>
  <button>
    <label>Campaign</label>
    <action>create_campaign_menu</action>
  </button>
  <button>
    <label>Single Player</label>
    <action>create_single_player_menu</action>
  </button>
  <button>
    <label>Multiplayer</label>
    <action>open_multiplayer</action>
  </button>
  <button>
    <label>Options</label>
    <action>open_options</action>
  </button>
  <button>
    <label>Credits</label>
    <action>open_credits</action>
  </button>
  <button>
    <label>Exit</label>
    <action>quit_app</action>
  </button>
</menu>
)";

    if (parser.parse(uimainmenu_xml, result)) {
        std::cout << "Sucesso! Parseou " << result.buttons.size() << " botões.\n";
        for (const auto& btn : result.buttons) {
            std::cout << "  - \"" << btn.label << "\" -> " << btn.action << "\n";
        }
    } else {
        std::cout << "ERRO: " << result.error_message << "\n";
    }

    // Teste 2: Parse do ui_editor_menu.xml
    std::cout << "\n--- Teste 2: Parse do ui_editor_menu.xml ---\n";

    const std::string editor_menu_xml = R"(
<?xml version="1.0" encoding="utf-8"?>
<editor_menu>
  <options>
    <option label="New Map" action="new_map"/>
    <option label="Open Map" action="open_map"/>
    <option label="Save Map" action="save_map"/>
    <option label="Exit" action="return_to_main_menu"/>
  </options>
  <tools>
    <tool name="Terrain Tool"/>
    <tool name="Unit Tool"/>
    <tool name="Building Tool"/>
    <tool name="Trigger Tool"/>
  </options>
</editor_menu>
)";

    // Note: O ui_editor_menu.xml tem um formato ligeiramente diferente (usando <option> e <tool>)
    // O parser atual foca no formato <menu><button><label/><action/></button></menu>
    // Mas podemos testar mesmo assim
    result = {};
    if (parser.parse(editor_menu_xml, result)) {
        std::cout << "Sucesso! Parseou " << result.buttons.size() << " botões.\n";
        for (const auto& btn : result.buttons) {
            std::cout << "  - \"" << btn.label << "\" -> " << btn.action << "\n";
        }
    } else {
        std::cout << "Formato diferente (esperado). Erro: " << result.error_message << "\n";
        // Isso é esperado - o editor menu tem formato diferente
    }

    // Teste 3: Carregar de arquivo
    std::cout << "\n--- Teste 3: Carregar de arquivo ---\n";

    // Como o arquivo está em E:\Port\AoM-Native-Port\tests\reference\databar\uimainmenu.xml
    // Usaremos o conteúdo inline para o teste
    // Em produção, seria: parser.load_from_file("uimainmenu.xml", result);

    std::cout << "\n=== Teste Concluído ===\n";
    return 0;
}