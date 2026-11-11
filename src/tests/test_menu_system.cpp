// Test menu system functionality
#include "ui/ui.h"
#include <cassert>
#include <iostream>

void test_menu_system_basic() {
    aom::ui::MenuSystem menu_system;
    menu_system.initialize(nullptr); // Pass nullptr for renderer in test
    
    // Test 1: Estado inicial
    assert(menu_system.get_state() == aom::ui::MenuState::MAIN_MENU);
    
    // Test 2: Adicionar botão do menu
    menu_system.add_button("start", "Start Game", []() {
        std::cout << "Game started!" << std::endl;
    });
    
    menu_system.add_button("quit", "Quit Game", []() {
        std::cout << "Game quit!" << std::endl;
    });
    
    assert(menu_system.get_button_count() == 2);
    
    // Test 3: Navegação do menu
    menu_system.handle_input(aom::ui::MenuAction::DOWN);
    assert(menu_system.get_selected_button() == 1);
    
    menu_system.handle_input(aom::ui::MenuAction::UP);
    assert(menu_system.get_selected_button() == 0);
    
    // Test 4: Seleção do menu
    int click_count = 0;
    menu_system.add_button("test", "Test Click", [&click_count]() {
        click_count++;
    });
    
    menu_system.handle_input(aom::ui::MenuAction::SELECT);
    assert(click_count == 1);
    
    std::cout << "Todos os testes do menu_system passaram!" << std::endl;
}

void test_menu_state_changes() {
    aom::ui::MenuSystem menu_system;
    menu_system.initialize(nullptr);
    
    // Test state change
    menu_system.set_state(aom::ui::MenuState::SETTINGS_MENU);
    assert(menu_system.get_state() == aom::ui::MenuState::SETTINGS_MENU);
    
    // Test state back to MAIN_MENU
    menu_system.set_state(aom::ui::MenuState::MAIN_MENU);
    assert(menu_system.get_state() == aom::ui::MenuState::MAIN_MENU);
    
    std::cout << "Testes de mudança de estado do menu passaram!" << std::endl;
}

void test_menu_action_processing() {
    aom::ui::MenuSystem menu_system;
    menu_system.initialize(nullptr);
    
    int action_called = 0;
    auto action_func = [&action_called]() { action_called++; };
    
    menu_system.add_button("test", "Test", action_func);
    
    // Test action processing
    menu_system.handle_input(aom::ui::MenuAction::SELECT);
    assert(action_called == 1);
    
    std::cout << "Testes de processamento de ações do menu passaram!" << std::endl;
}

int main() {
    test_menu_system_basic();
    test_menu_state_changes();
    test_menu_action_processing();
    
    std::cout << "\n=== Todos os testes do menu system passaram! ===" << std::endl;
    return 0;
}