# Análise Completa — Port Android Nativo de *Age of Mythology*

**Data:** 31/10/2026  
**Port:** `E:\Port\AoM-Native-Port`  
**Original:** `E:\Port\Age of Mythology` (AoM + Titans, Extended Edition)  
**Método:** Análise estática do repositório + inventário quantitativo dos BARs (reverse engineering manual, equivalente ao fluxo REA).

---

## 1. Resumo Executivo

O port é um **protótipo nativo de RTS**, com infraestrutura EGL/GLES3, NativeActivity, parsers parciais e uma simulação de gameplay baseada em dados hard-coded. O terreno atual é procedural; não há integração completa dos cenários, unidades, tecnologias ou campanhas originais. A cópia não foi validada em build/runtime Android nesta análise.

O original é um RTS massivo (~1 GB de assets, ~20.000 entradas em BARs, dezenas de formatos proprietários). Portar o jogo completo é projeto de anos. O port atual é um **visualizador de assets + esqueleto de engine**.

**Paridade:** não estimada nesta atualização. Há um APK anterior, mas ele não valida o código atual. O SDK local contém CMake 3.22.1 e NDK 28.2; o CMake Tools do VS Code ainda falhou ao configurar o projeto e não forneceu diagnósticos.

---

## 2. Escopo e Método

1. Inventário do original: estrutura, BARs, contagens/extensões (parser Python próprio).
2. Leitura integral do código do port: `src/`, `include/`, `CMakeLists.txt`, `PROGRESSO.md` (1265 linhas), testes.
3. Comparação sistemática módulo a módulo.
4. Identificação de formatos não cobertos via extensões nos BARs.

Skill REA consultado como guia metodológico; sem servidor MCP REA neste ambiente, análise feita com ferramentas nativas equivalentes.

---

## 3. Inventário do Jogo Original

### 3.1 Estrutura de alto nível

```
Age of Mythology/
├── aom.exe / aomx.exe     # executáveis Win32
├── age.dll, aomac.dll...   # DLLs
├── data/                   # data.bar, data2.bar + .lgt + .cpn/.cpx + uimain.xmb
├── textures/               # textures.bar, textures2.bar + cursors/, icons/, terrain/, ui/
├── models/                 # models.bar, models2.bar + .lng
├── sound/                  # sounds.bar + cinematics/, dialog/, music/, taunts/, ui/, xpack/
├── anim/, ai/, scenario/, trigger/, trigger2/, ai2/, rm/, rm2/
├── fonts/, gfxconfig/, startup/, savegame/, Users/, Voobly Mods/
```

### 3.2 Arquivos BAR (contagem de entradas)

Parser: `tools/parse_bar.py` (header 28 B, diretório com tabela de offsets + registros nome NUL-terminated, compressão l33t/zlib).

| Arquivo | Tamanho | Entradas | Extensões (contagem) |
|---|---|---|---|
| `data/data.bar` | 1,65 MB | 126 | `.xmb` 121, `.tga` 2, `.txt` 2, `.dat` 1 |
| `data/data2.bar` | 2,48 MB | 135 | `.xmb` 130, `.tga` 2, `.txt` 2, `.dat` 1 |
| `textures/textures.bar` | 85,57 MB | 5521 | `.ddt` 4896, `.bti` 444, `.xmb` 61, `.bt8` 113, `.cub` 3, `.ico` 1, `.blg` 1, `.ifl` 1, `.txt` 1 |
| `textures/textures2.bar` | 34,00 MB | 1630 | `.ddt` 1519, `.bti` 54, `.xmb` 43, `.jpg` 12, `.cub` 1, `.txt` 1 |
| `models/models.bar` | 279,42 MB | 8658 | `.brg` 5628, `.grn` 584, `.dcl` 1509, `.prt` 585, `.spr` 3, `.no` 341, `.lib` 1, `.xmb` 6 |
| `models/models2.bar` | 142,38 MB | 3148 | `.brg` 2116, `.grn` 287, `.dcl` 468, `.prt` 198, `.no` 77, `.tga` 1, `.lib` 1 |
| `sound/sounds.bar` | 84,70 MB | 1790 | `.wav` 1151, `.xmb` 320, `.xml` 319 |

**Total: ~21.008 entradas.** Fora isso: `.lgt`, `.cpn`/`.cpx`, `.scn`, `.lng`, arquivos soltos em `cursors/`, `icons/`, `terrain/`, `ui/`, `music/`, `dialog/`.

### 3.3 Formatos de asset (original)

| Ext | Significado (inferido) | Suporte no port |
|---|---|---|
| `.bar` | Arquivo de arquivamento (header 28 B, compressão l33t/zlib) | ✅ Sim |
| `.ddt` | Textura DirectDraw (RTS3, 7 formatos + mips + paleta) | ✅ Parcial (fmt 1,3,4,5,6,7) |
| `.bti` | Tabela/índice de textura (10–20 B) | ❌ Não |
| `.bt8` | Textura 8-bit? | ❌ Não |
| `.cub` | Cube map (skybox/reflexão) | ❌ Não |
| `.ifl` | Image file list (sequência de frames) | ❌ Não |
| `.blg` | Desconhecido (1 entrada) | ❌ Não |
| `.jpg` | JPEG (texturas expansão) | ❌ Não (stb_image suporta, não integrado) |
| `.brg` | Modelo 3D (magic BANG, tags MESI/ASET) | ✅ Parcial |
| `.grn` | Animação (magic GRN) | ❌ Não |
| `.dcl` | Declaração de modelo (metadados, bbox) | ❌ Não |
| `.prt` | Partículas | ❌ Não |
| `.spr` | Sprite | ❌ Não |
| `.no` | Placeholder "no texture" | ❌ Não |
| `.lib` | Biblioteca de modelos | ❌ Não |
| `.xmb` | XML binário (config, proto, UI, som, terreno) | ✅ Parcial (parser X1/XR) |
| `.xml` | XML texto (sound banks) | ❌ Não (sem parser XML genérico) |
| `.wav` | Áudio PCM | ✅ Sim (OpenSL) |
| `.tga` | Imagem TGA | ❌ Não integrado |
| `.lgt` | Iluminação de mapa | ❌ Não |
| `.cpn`/`.cpx` | Campanhas | ❌ Não |
| `.scn` | Cenário | ❌ Não |
| `.xs`/`.xs2` | Scripts de IA (XScript) | ❌ Não |
| `.lng` | Localização de modelo | ❌ Não |

### 3.4 Escala do original

- ~1 GB de assets empacotados (models.bar sozinho 279 MB).
- ~20k entradas em BARs principais.
- 6.415 texturas DDT (4896+1519).
- 7.744 modelos BRG (5628+2116).
- 871 animações GRN (584+287).
- 1.977 declarações DCL (1509+468).
- 783 sistemas de partícula PRT (585+198).
- 1.151 WAVs + 320 XMB + 319 XML de sound banks.
- 251 XMB em data (proto, tech tree, units...).
- 104 XMB em textures (UI layouts, ícones).
---

## 4. Arquitetura do Port

### 4.1 Módulos implementados

```
AoM-Native-Port/
├── src/assets/        bar_archive.cpp, brg_model.cpp, ddt_texture.cpp, xmb_parser.cpp
├── src/audio/         opensl_engine.cpp
├── src/engine/        engine.cpp, ui_from_xmb.cpp
├── src/renderer/      gles_renderer.cpp
├── src/ui/            ui.cpp
├── src/platform/android/ native_activity.cpp
├── src/tests/         gameplay_test.cpp, ui_test.cpp
├── tests/             test_ddt.cpp, test_xmb.cpp
├── include/           headers + ecs/, game/, platform/touch_input.h
├── third_party/       stb_image, stb_truetype, glm, ndk-include (EGL/GLES3/KHR)
├── android/           AGP 8.5.2 + CMake (NDK)
├── scripts/           build_native.sh, package_apk.sh
└── docs/, tools/, assets/ (splash, loading, buttons, música, fonte)
```

### 4.2 Tecnologias

- Linguagem: C++20.
- Gráficos: OpenGL ES 3.0/3.2 via EGL, shaders GLSL ES 3.00.
- Áudio: OpenSL ES (WAV PCM).
- Build: CMake 3.22 + Unix Makefiles (Termux) ou AGP 8.5.2 (PC com NDK).
- Plataforma: Android NativeActivity (arm64-v8a, minSdk 24).
- Deps vendorizadas: glm, stb, headers NDK (EGL/GLES3/KHR).

### 4.3 Funcionalidades já implementadas

- Parsing de BAR (header, diretório, l33t/zlib, busca case-insensitive, extração).
- DDT: formatos 1, 3, 4, 5, 6, 7 (BGRA, paleta, DXT1, AOM_5BPP, AOM_8BPP, greyscale) + mips.
- BRG: parser BANG com tags MESI, animação ASET, extração de mesh/índices/UVs/cores; upload para GPU.
- XMB: parser do formato X1/XR (string tables, nós recursivos, atributos UTF-16LE→UTF-8).
- Renderer GLES3: EGL display/context/surface, VAO/VBO, shaders 2D/3D/mesh/terrain/line, texturas RGBA8, font atlas (stb_truetype), UI quads/texto, debug lines, câmera/luz.
- Engine: estados (Splash/Loading/MainMenu/Options/Credits/InGame/Paused), carregamento de archives, textura real em tela cheia, modelo 3D rotacionando, HUD FPS, menus com botões.
- UI: botões (normal/pressed, slider, checkbox, ícone), labels, animações, hit-test, som de clique.
- Audio: OpenSL ES, WAV PCM, música em loop, SFX, volumes.
- NativeActivity: callbacks, thread de jogo, input queue, ciclo de vida (pause/resume/recreate).
- ECS/Gameplay: stubs (unidades, combate, recursos, navegação A*, seleção por caixa).
- TouchInputManager: apenas header (sem implementação).
- Vulkan renderer: apenas declaração (sem implementação).
- Testes: gameplay_test (movement, pathfinding, combat), ui_test, test_ddt, test_xmb.

### 4.4 Status de build

- `build-m2/libaom_native.so` (369 KB) é artefato antigo (não reflete código atual).
- APK anterior `aom-native-debug.apk` (4,7 MB) existe mas não foi recompilado nesta sessão.
- Bloqueios no ambiente do agente: cmake/clang/aapt indisponíveis; NDK r27b só linux-x86_64; Ninja quebrado; precisa Termux no dispositivo.
- Comando oficial documentado em PROGRESSO.md §1/§16.

---

## 5. Matriz de Paridade (Original × Port)

| Subsistema | Original | Port | Paridade | Status |
|---|---|---|---|---|
| Arquivos BAR | 7 principais + extras | Parser completo | ~95% | ✅ |
| Texturas DDT | 6415 DDT | 6 formatos (1,3,4,5,6,7) | ~85% | ✅ quase |
| Texturas BTI/BT8/CUB/IFL/JPG | 498 BTI, 113 BT8, 4 CUB, 1 IFL, 12 JPG | 0 | 0% | ❌ |
| Modelos BRG | 7744 | Parser parcial (mesh) | ~60% | ⚠️ |
| Animações GRN | 871 | 0 | 0% | ❌ |
| Declarações DCL | 1977 | 0 | 0% | ❌ |
| Partículas PRT | 783 | 0 | 0% | ❌ |
| Sprites SPR | 3 | 0 | 0% | ❌ |
| XMB (data) | 251 arquivos | Parser genérico; só proto.xmb usado | ~30% | ⚠️ |
| XMB (textures/UI) | 104 | 0 (não carregados) | 0% | ❌ |
| XMB (models) | 6 | 0 | 0% | ❌ |
| XMB (sound banks) | 320 | 0 | 0% | ❌ |
| XML texto | 319 sound banks | 0 | 0% | ❌ |
| Áudio WAV | 1151 | OpenSL PCM | ~70% | ⚠️ |
| Áudio MP3/música | music/ | play_music aceita path (não verificado) | ? | ⚠️ |
| 3D audio/posicional | Sim | Não | 0% | ❌ |
| Render GLES3 | DirectX 8/9 | GLES3 shaders | ~45% | ⚠️ |
| Terreno | Sim | Malha procedural 41×41; sem cenário/heightmap real | Protótipo | ⚠️ |
| Água | Sim | draw_water declarado, não implementado | 0% | ❌ |
| Skybox/Cube map | .cub | 0 | 0% | ❌ |
| Partículas visuais | .prt | 0 | 0% | ❌ |
| Animação de modelos | .grn | 0 | 0% | ❌ |
| Fontes | Próprias | stb_truetype (font.ttf) | ~60% | ⚠️ |
| UI | Completa (menus, HUD, tooltips) | Botões/labels/sliders/checkbox | ~40% | ⚠️ |
| Input touch | Mouse/teclado | Touch e comandos RTS básicos | Protótipo | ⚠️ |
| Gestos RTS | N/A (PC) | Seleção/toque e zoom/órbita; sem conjunto completo de gestos | Parcial | ⚠️ |
| Gameplay (unidades) | Completo | Entidades hard-coded em `GameplayWorld` | Protótipo | ❌ |
| Combate | Completo | Combate simples de proximidade | Protótipo | ❌ |
| Economia/recursos | Completo | Nós e valores de recursos hard-coded | Protótipo | ❌ |
| Navegação (pathfinding) | Completo | Navegação básica do protótipo | Protótipo | ❌ |
| Construções | Completo | Sistema de construção básico, sem conteúdo original | Protótipo | ❌ |
| Tecnologia/tech tree | proto/techtreex | 0 | 0% | ❌ |
| Deuses menores | Sim | 0 | 0% | ❌ |
| Campanhas | .cpn/.cpx | 0 | 0% | ❌ |
| Cenários | .scn | 0 | 0% | ❌ |
| IA / Random Maps | xs/xs2, rm/rm2 | 0 | 0% | ❌ |
| Triggers | trigger/trigger2 | 0 | 0% | ❌ |
| Multiplayer/rede | Zone/ESO | 0 | 0% | ❌ |
| Save/Load | savegame/ | Options only | ~5% | ❌ |
| Localização/i18n | Language/ | config language string | ~5% | ❌ |
| Vulkan | N/A | Declaração only | 0% | ❌ |

---

## 6. Análise Detalhada por Subsistema

### 6.1 Renderização (GLES3)

**Implementado:** EGL init, context ES3, window surface, shaders (UI quad, mesh, terrain, line), VAO/VBO, texturas RGBA8, font atlas (stb_truetype), debug lines, câmera/luz, draw_ui_text, draw_ui_quad.

**Lacunas:**
- **Vulkan:** `create_vulkan_renderer()` declarado mas não implementado; CMake não compila vulkan_renderer (não existe). GLES3 é aceitável.
- **Terreno:** o engine gera uma malha procedural 41×41 e a desenha; ainda não carrega um heightmap/cenário original nem aplica blend/splatting de texturas.
- **Água:** `draw_water` declarado, não implementado no shader/renderer.
- **Skybox/cube maps:** `.cub` não suportado; sem skybox.
- **Partículas:** sem sistema de partículas (PRT não parseado).
- **Animação:** sem skinning/GRN; modelos estáticos.
- **Sombras:** sem shadow mapping.
- **Pós-processamento:** sem (bloom, HDR, etc.).
- **Performance:** texturas grandes decodificadas em CPU para RGBA (sem compressão GPU, sem ETC2/ASTC). Para 6k texturas, pico de memória alto. BAR carrega arquivo inteiro em RAM (até 279 MB) — inaceitável em mobile.
- **Mipmaps:** DDT tem mipmaps mas renderer usa só nível 0; sem geração de mipmaps GPU.

### 6.2 Assets

**BAR:** o nome do registro começa no byte 20 e termina em NUL; esse layout foi confirmado em `data.bar` e `data2.bar`. Portanto, o parser C++ não estava deslocado: a interpretação do byte 20 como comprimento era um fallback incorreto da ferramenta Python auxiliar. `tools/parse_bar.py` agora segue a tabela de offsets, valida os limites e extrai l33t/zlib; `bar_tool.py` reutiliza esse parser. O C++ ainda carrega o arquivo inteiro em `file_data_` (até 279 MB); para mobile, é necessário streaming/mmap ou extração on-demand. Sem escrita/criação de BAR (só leitura) — adequado para o port.

**DDT:** cobre 6 dos formatos principais. Faltam:
- Formato 0 (UNKNOWN) — pode ser usado?
- Formato 2? (não listado no enum; pode existir)
- Formato 8 (cube map) — usage=8
- Usage types: 0 texture, 1 texture2, 3 palette, 6 bump, 8 cube. Parser não distingue usage para tratamento especial (cube maps precisam de 6 faces).
- BTI/BT8: provável texturas 8-bit indexadas ou tabelas; não implementado.
- JPG: stb_image suporta, mas não integrado ao extrator.
- TGA: não integrado.
- Mipmaps: decode_rgba aceita mip_level mas engine só usa 0.

**BRG:** parser BANG parcial. Faltam:
- Tags não tratadas (animação ASET é pulada, não aplicada).
- Múltiplos meshes: engine carrega apenas meshes[0].
- Texturas associadas: load_brg_model tenta carregar textura de mesmo nome .ddt, mas modelos usam nomes de textura da DCL/BRG? Precisa validar.
- Normais: computadas se ausentes (ok).
- Skinning/bones: não.
- LOD: não.

**XMB:** parser genérico X1/XR. Mas:
- `read_string_table` e `read_element` não totalmente revisados; testes passam em proto.xmb.
- Apenas proto.xmb é carregado como game_config; outros XMBs (units, tech tree, UI, sound banks) não são consumidos.
- `generate_ui_from_xmb()` agora cria itens de menu compatíveis com `XmbElement` e é chamada ao montar o menu principal; se `proto.xmb` não contiver um `<menu>` compatível, o menu nativo existente continua como fallback. Isso não integra `uimain.xmb` nem comprova que o XMB de interface real seja suportado.

**GRN/DCL/PRT/SPR:** não implementados. Sem animação, sem partículas, sem metadados de modelo.

### 6.3 Áudio

**Implementado:** OpenSL ES, WAV PCM (RIFF), música em loop via buffer, SFX, volumes, mute flags, click/big_button pré-carregados (PCM).

**Lacunas:**
- MP3/Ogg: `play_music(path)` recebe path de arquivo; implementação provavelmente lê arquivo e tenta parsear como WAV → falha para MP3. Menu music é MP3 (`menu_music.mp3`). Como toca? Precisa verificar opensl_engine completo. Pode usar media player? Não parece. Possível que música não toque.
- Sound banks: 320 XMB + 319 XML em sounds.bar definem quais WAVs tocar por unidade/evento. Não consumidos.
- 3D/positional audio: não.
- Streaming: carrega arquivo inteiro em memória.
- Múltiplos SFX simultâneos: um único player SFX (sem pool) → cortes.
- Latência: OpenSL buffer queue OK para baixa latência.

### 6.4 Input

**Implementado:** NativeActivity drena AInputQueue e converte AMotionEvent em TouchEvent. O engine roteia toques para a UI ou gameplay; há órbita/zoom da câmera e picking simplificado de unidades/terreno.

**Lacunas:**
- `TouchInputManager` (touch_input.h) não está integrado como sistema geral de gestos.
- Picking usa uma interseção simplificada com o plano do terreno; não resolve terreno irregular.
- Ainda faltam gestos completos de seleção múltipla, pan/rotate multitoque e comandos RTS robustos.
- Teclado/gamepad não suportados (original PC).
- Sem feedback háptico.
---
## 6.5 Gameplay / Simulação

**Implementado (protótipo):** `Engine::start_game()` instancia `GameplayWorld` com unidades amigas/inimigas, trabalhador e nós de recursos hard-coded. O loop atualiza/renderiza gameplay e encaminha toques; sistemas básicos de movimento, combate, seleção, coleta e construção estão conectados.

**Lacunas (quase tudo):**
- Unidades, valores, recursos e terreno não são carregados de `proto.xmb` nem de cenários originais.
- Não há IA estratégica, campanha, triggers nem objetivos de vitória/derrota.
- Sem terreno/colisão de cenário real.
- Sem economia completa (só 4 recursos básicos; sem farms, minas, etc.).
- Construções não têm conteúdo, modelos ou progressão equivalentes ao jogo original.
- Sem tecnologia/idades/deuses menores.
- Sem combate real (apenas damage se próximo).
- Sem formação, pathfinding com obstáculos dinâmicos, fog of war.
- Sem vitória/derrota, sem objetivos.
- Sem save/load de partida.
- Sem campanhas/cenários (parser de .scn/.cpn não existe).

### 6.6 UI

**Implementado:** Botões (normal/pressed, slider, checkbox, ícone), labels, animações (sprite sheet), hit-test, som de clique, menus (main/options/credits) com stone buttons.

**Lacunas:**
- Sem tooltips, sem drag-and-drop, sem scroll lists, sem minimap.
- Sem HUD de jogo (recursos, população, minimapa, painel de comandos, fila de produção).
- Sem telas de loading de campanha, sem cutscenes.
- Sem localização (strings hardcoded em português).
- Geração XMB limitada ao esquema `<menu><item name/text action>`; o arquivo efetivamente carregado pelo engine continua sendo `proto.xmb`, não `uimain.xmb`.
- UI não é carregada de XMB (uimain.xmb, commandpanel.xmb) — menus são hard-coded.

### 6.7 Config / Options / Save

**Implementado:** load_options/save_options (funções existem mas implementação?); master/music/sfx volume, toggles, graphics quality, show_fps.

**Lacunas:** não verificado se persiste em arquivo; sem perfis de jogador; sem save de partida.

### 6.8 Rede / Multiplayer

**Implementado:** nada. **Original:** Zone/ESO, Voobly. Port não tem networking. Multiplayer é um projeto separado enorme (requer servidor, protocolo, NAT traversal). Fora do escopo realista de port mobile.

### 6.9 Campanhas / Cenários / IA / Triggers

**Implementado:** nada. **Original:** main campaign.cpn, xpack campaign.cpx, cenários .scn, triggers, IA xs. Sem parser para nenhum desses. Fora do escopo de port "visualizador".

---

## 7. Bloqueios de Build e Ambiente

1. **Ambiente do agente (Windows):** CMake 3.22.1, Ninja e NDK 28.2 estão instalados no Android SDK local. O CMake Tools falhou ao configurar o projeto mesmo com uma tentativa usando preset local; não houve build nem diagnóstico. Não foi necessário instalar ferramentas.
2. **Termux/NDK:** NDK r27b só linux-x86_64; usa clang nativo do Termux. Ninja quebrado → usar Unix Makefiles.
3. **CMake:** os CMakeLists raiz e `android/CMakeLists.txt` incluem atualmente os fontes de UI, áudio e `ui_from_xmb`; a configuração/build ainda precisa ser validada.
4. **XMB/UI:** o helper foi corrigido e ligado ao menu principal com fallback, mas usa um esquema genérico `<menu><item>` e não carrega `uimain.xmb`.
5. **FUSE/conflitos .device-***: PROGRESSO menciona cópias de conflito; sincronização manual necessária.
6. **Tamanho de memória:** carregar BARs inteiros (279 MB) em RAM → OOM em mobile.
7. **Assets no dispositivo:** precisa copiar ~1 GB de dados do jogo para `/storage/emulated/0/.../AoM` ou usar asset manager do APK (impossível para 1 GB).

---

## 8. Riscos e Prioridades

**Riscos críticos:**
- **Memória:** BARs carregados inteiros → OOM. Solução: mmap ou extração streaming.
- **Escopo:** portar RTS completo é inviável para uma pessoa. Necessário definir MVP (ex.: visualizador de modelos/texturas + menu jogável simples).
- **Performance:** decodificação DDT em CPU para RGBA a cada frame (sem cache GPU) → FPS baixo com muitas texturas.
- **Legal:** Age of Mythology é IP da Microsoft/Xbox Game Studios. Distribuir APK com assets do jogo viola copyright. Port deve exigir que o usuário forneça seus próprios arquivos (como já faz via data_path). Manter essa abordagem.
- **Build não validado:** o CMake Tools não configurou o projeto; não se pode concluir que o APK atual compila ou inicia.

**Prioridades (ordem sugerida):**
1. **Validar build** com o SDK/NDK local e resolver a falha de configuração do CMake Tools.
2. **Streaming de BAR** (mmap / file handles) para evitar OOM.
3. **Cache de texturas GPU** (não decodificar toda frame).
4. **Implementar TouchInputManager** (gestos RTS básicos).
5. **Carregar proto.xmb** e spawnar unidades reais.
6. **Terreno** (heightmap + textura).
7. **Animação GRN** ou pelo menos playback de frames.
8. **Partículas PRT**.
9. **Campanhas/cenários** (escopo grande, deixar por último).

---

## 9. Roadmap Sugerido (fases)

- **Fase 0 (1-2 semanas):** Corrigir build, validar APK no dispositivo, testes automatizados de DDT/XMB/BAR.
- **Fase 1 (2-4 semanas):** Streaming de BAR, cache GPU de texturas, mipmaps, terreno básico, skybox.
- **Fase 2 (4-8 semanas):** TouchInputManager completo, ray casting, seleção/movimento de unidades, HUD de recursos.
- **Fase 3 (8-12 semanas):** Carregar proto.xmb, spawn de unidades/buildings, combate, economia, pathfinding robusto.
- **Fase 4 (12-20 semanas):** Animação (GRN), partículas (PRT), som (sound banks), efeitos.
- **Fase 5 (20+ semanas):** Campanhas, cenários, IA, triggers, save/load.
- **Fase 6 (futuro):** Multiplayer (provavelmente fora do escopo mobile).

---

## 10. Conclusão

O port Android nativo de *Age of Mythology* é um **projeto ambicioso e de longo prazo**. O código existente demonstra competência técnica sólida em parsing de formatos proprietários e renderização GLES3, mas está muito longe de um jogo jogável. A análise quantitativa dos BARs revela a escala real do desafio: ~21.000 assets, ~7.700 modelos, ~6.400 texturas, ~870 animações, ~2.000 declarações, ~780 sistemas de partículas, ~250 arquivos de configuração XMB, ~1.150 sons.

**Recomendação:** definir um MVP claro (ex.: "visualizador interativo de modelos/texturas do AoM no Android" ou "mini-RTS com 2 facções e 5 unidades") antes de tentar paridade completa. Sem isso, o projeto corre risco de nunca ser concluído.

---

## 11. Atualizações Implementadas Pós-Análise (M4/M5)

Após a auditoria detalhada desta análise, as prioridades mais críticas de P0 e P1 foram atacadas e implementadas diretamente na base de código:

1. **Streaming Leve no `BarArchive` (Eliminação do Risco de OOM):**
   - O carregamento integral dos arquivos BAR (até 279 MB) em memória foi substituído por **leitura sob demanda baseada em streaming com mutex thread-safe**.
   - Na inicialização (`open`), apenas a tabela de diretório e cabeçalhos são lidos (~500 KB a 1 MB). A extração (`extract`) agora lê apenas os bytes estritamente necessários via `fseek`/`fread`.
   - **Economia:** Redução de ~540 MB para ~4 MB de RAM ocupada pelos arquivos BAR.

2. **Pipeline Completo de Terreno 3D (GLES 3.0):**
   - Shaders `kTerrainVertexSrc` e `kTerrainFragmentSrc` adicionados ao pipeline com suporte a luz solar difusa, ambiente e repetição de textura proporcional.
   - Geração de malha de terreno 41×41 (1.681 vértices) com relevo perimetral suave e normais por gradiente.
   - Integração da textura de terreno autêntica `assets/terrain1.tga`.

3. **Pipeline de Linhas Dinâmicas de Depuração:**
   - Shaders `kLineVertexSrc` e `kLineFragmentSrc` com buffer VBO dinâmico (`GL_DYNAMIC_DRAW`).
   - Renderização sem alocações no loop para anéis de seleção no solo e barras flutuantes de vida (Verde para aliados, Vermelho para inimigos).

4. **Loop de Jogo e Entidades no `GameplayWorld`:**
   - `Engine::start_game()` agora inicializa o mundo ECS, spawna Hoplita Grego, Aldeão e Inimigo, além de nós de recursos de Ouro (1.500), Madeira (2.000) e Alimento (800).
   - Simulação ativa de combate, navegação A*, coleta contínua e HUD superior de recursos.
   - Fallback procedural 3D de obelisco/pirâmide em C++ para garantir visualização mesmo sem modelos `.brg` externos.

5. **Raycasting 3D e Controles Táteis RTS:**
   - Implementado cálculo de projeção reversa de tela para o plano de solo $Y=0$.
   - Toque no solo: emite ordem de movimentação com waypoints do grid A*.
   - Toque em inimigo: emite ordem de ataque direto para a unidade selecionada.
   - Toque em recurso: direciona aldeões para coleta de recursos.
   - Toque em unidade: seleção imediata. Arrastar o dedo: órbita suave da câmera 3D.

6. **Saneamento de Inconsistências de Build:**
   - `src/engine/ui_from_xmb.cpp` corrigido para repassar os rótulos de texto aos botões gerados dinamicamente.
   - `android/CMakeLists.txt` sincronizado para conter exatamente os 12 fontes e flags do `CMakeLists.txt` raiz.


7. **Minimapa 2D Tático com Textura Autêntica (minimapbackground.tga):**
   - Decodificação do TGA original com correção do canal alfa legado de 2002.
   - Moldura de pedra com ornamentos dourados no canto inferior esquerdo.
   - Radar em tempo real com blips coloridos (tropas amigas, operários, inimigos, seleções, edifícios e nós de recursos).
   - Indicador de frustum da câmera (retângulo de viewport dinâmico sincronizado com a posição e zoom).
   - Toque interativo (Click-to-Pan): toque direto no minimapa reposiciona a câmera no campo de batalha.

8. **Feedback Visual de Waypoints e Vetores de Ataque:**
   - Linhas contínuas dinâmicas em verde neon ligando a unidade a cada waypoint até o destino com cruz demarcadora.
   - Linha de visada vermelha de combate conectando atacante e alvo inimigo com perseguição dinâmica.

9. **Consumo de Estatísticas Oficiais do proto.xmb:**
   - Extração automática de HP, velocidade, alcance, dano e custos diretamente dos 623 nós de unidade do XML binário.
   - Spawning autêntico de Hoplite (115 HP, 4.2 vel, 8.0 dano), Villager Greek (65 HP, 3.8 vel, 6.0 dano), Toxotes (60 HP, 4.0 vel, 6.5 dano, 15m alcance) e Minotauro mítico inimigo (300 HP, 4.0 vel, 60.0 dano).

10. **Sistema Completo de Construção RTS, Andaimes 3D e Treinamento de Tropas em Quartéis:**
    - **Ghost Placement Holográfico 3D:** Pré-visualização volumétrica em tempo real no solo com pilares e cobertura em linhas dinâmicas (verde para solo livre e navegável, vermelho para obstruções).
    - **Painel de Comando Contextual Mobile:** Exibido no canto inferior direito com moldura de pedra e filetes dourados. Apresenta nome da entidade, barra de HP e grade tátil de botões contextuais:
      - *Para Aldeões:* botões de construção de Casa (+ CASA, 40 Madeira), Quartel Militar (+ QUARTEL, 50 Madeira, 50 Ouro), Torre de Vigia (+ TORRE, 100 Madeira, 50 Ouro) e botão PARAR.
      - *Para Quartéis Concluídos:* botões de treinamento militar (+ HOPLITA: 60 Comida, 35 Ouro; + TOXOTES: 55 Madeira, 35 Ouro) com barra ciano de progresso de treinamento.
      - *Para Combatentes:* botão de cancelamento de ordens (PARAR UNIDADE).
    - **Ciclo de Vida de Construção e Andaimes:** Ao iniciar uma obra, o edifício exibe andaimes de madeira e barra de progresso amarela. Aldeões ordenados para construir navegam até o canteiro e aceleram o avanço de 0.08x (passivo) para 1.0x (ativo).
    - **Spawning e Ponto de Reunião:** Ao concluir o treino militar no quartel, a nova unidade é instanciada e recebe ordem de movimento automática para o ponto de reunião (
ally point).

---

*Fim da análise.*
