# Documentação — Menu UI (Pregame)

## Visão geral

O menu pré-jogo do **AoM Native Port** é baseado no layout autêntico de *Age of Mythology: The Titans* (`uipregamenew.xmb`). Todos os menus são implementados como métodos da classe `Engine` em `src/engine/engine.cpp`, usando o `UIManager` (`src/ui/ui.cpp`) para renderizar botões, labels e animações.

## Estados do menu (`EngineState`)

Definidos em `include/engine/engine.h`:

| Estado | Valor | Menu | Descrição |
|--------|-------|------|-----------|
| `MAIN_MENU` | `uipregamenew` | Menu principal The Titans | 7 botões de jade originais + 2 botões extras (uigamelist, uigodpicker) |
| `GAME_LIST` | `uigamelist` | Lista de jogos salvos, campanhas, skirmish | Jogos salvos, campanhas disponíveis, mapa aleatório |
| `GOD_PICKER` | `uigodpicker` | Escolha de civilização / deus menor | 4 civilizações, deus maior, deuses menores |
| `OPTIONS` | `uioptions` | Configurações | Áudio, gráficos, importação de dados do jogo |
| `CREDITS` | — | Créditos | Equipe de desenvolvimento |
| `MULTIPLAYER_*` | — | Multijogador | LAN, lobby, etc. |

## Fluxo de navegação

```
SPLASH → LOADING → MAIN_MENU (uipregamenew)
                              │
         ┌────────┬──────────┼──────────┬──────────┐
         ▼        ▼          ▼          ▼          ▼
     start_game() create_campaign_menu() open_multiplayer() open_options() open_credits()
         │                                              │
         ▼                                              ▼
   IN_GAME                                       GAME_LIST (uigamelist)
                                              │
                                              ├─ jogos salvos
                                              ├─ campanhas
                                              └─ skirmish
                                              │
                                              ▼
                                         GOD_PICKER (uigodpicker)
                                              │
                                              ├─ escolher civilização
                                              ├─ escolher deus menor
                                              └─ INICIAR JOGO → IN_GAME
```

## Como adicionar um novo menu

1. Adicione um novo valor ao `enum class EngineState` em `include/engine/engine.h`.
2. Declare o método `void create_nome_menu();` na classe `Engine`.
3. Implemente o método em `src/engine/engine.cpp`, seguindo o padrão:
   ```cpp
   void Engine::create_nome_menu() {
       clear_ui();
       const float vw = config_.window_width;
       const float vh = config_.window_height;
       const float sx = vw / 1024.0f;
       const float sy = vh / 768.0f;
       // ... add_button / add_label ...
   }
   ```
4. Adicione o `case` correspondente em `Engine::set_state()`.
5. (Opcional) Adicione botão de acesso no `create_main_menu()`.

## Padrão de coordenadas

Todas as posições usam a resolução de referência **1024×768** (tamanho do layout XMB original), escaladas para a resolução atual:

```cpp
const float sx = vw / 1024.0f;
const float sy = vh / 768.0f;
ui_->add_button({x * sx, y * sy}, {w * sx, h * sy}, "Label", callback);
```

## Cores usadas

| Uso | Cor ARGB | Exemplo |
|-----|----------|---------|
| Título / texto principal | `0xFFFFD000` | Dourado AoM |
| Texto secundário | `0xFFCCCCCC` | Cinza claro |
| Texto desativado / aviso | `0xFF888888` | Cinza médio |
| Botão normal (jade) | `0x00000000` | Transparente (exibe textura original) |
| Botão pressionado | `0x55FFD700` | Dourado com 33% opacidade |
| Botão extra (uigamelist/godpicker) | `0xFF8B4513` | Marrom / bronze |

## Arquivos relacionados

| Arquivo | Função |
|---------|--------|
| `include/engine/engine.h` | Declaração de estados e métodos de menu |
| `src/engine/engine.cpp` | Implementação de todos os menus (`create_*_menu`) |
| `include/ui/ui.h` | `UIManager`, `UIButton`, `UILabel`, `add_button`, `add_label` |
| `src/ui/ui.cpp` | Renderização e processamento de toque da UI |
| `include/engine/ui_layout.h` | `UILayoutManager` (layouts XMB do data.bar) — ainda não usado no menu pregame |
| `assets/uipregamenew.xmb` | Layout XMB de referência (arte original The Titans) |
| `assets/titans_menu_parts/` | Partes gráficas do menu (botões jade, portal, logos) |
| `docs/MENU_UI.md` | Este documento |
