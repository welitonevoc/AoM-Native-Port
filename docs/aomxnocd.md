# aomxnocd.exe — Análise Completa de Funcionamento

**Arquivo:** `aomxnocd.exe`  
**Tamanho:** 6.8 MB (7.110.656 bytes)  
**MD5:** `09876f130d02ae760a6b06ce6a9c92db`  
**SHA256:** `08ec47726908fdda18af16bd476c345e6a384a5e46973b52d49ad3f8f763e1ca`  
**Tipo:** PE32 executable (GUI) Intel 80386, for MS Windows  
**Data de linkagem:** Mon Jan 5 20:32:55 2004  
**Linker:** Microsoft Visual C++ 6.0  
**ImageBase:** `0x00400000`  
**Entry Point:** `0x004ce392`  
**Crack:** `!CRACKED BY VENGEANCE` (NoCD — proteção SafeDisc removida)

---

## 1. Visão Geral

`aomxnocd.exe` é o executável principal **crackeado** do jogo **Age of Mythology: The Titans** (expansão de 2003 desenvolvida pela Ensemble Studios e publicada pela Microsoft Game Studios).

Não é um cracker isolado. É o próprio `aomx.exe` original com a verificação de CD (SafeDisc / secdrv.sys) removida. O executável contém praticamente todo o motor do jogo compilado, incluindo:

- Loop principal de simulação
- Sistema de renderização DirectX 8/9
- Pathfinding e física de unidades
- Sistema de IA completo
- Gerenciamento de recursos e economia
- Poderes dos deuses
- Campanhas, cenários e triggers
- UI completa (gadget system)
- Rede / multiplayer (Zone / DirectPlay)
- Áudio (Miles Sound System + DirectSound)
- Animações (Granny)

O executável depende fortemente de `aomac.dll` (biblioteca principal do jogo) e de várias DLLs de terceiros (`granny.dll`, `mss32.dll`, `RockallDLL.dll`).

---

## 2. Estrutura do Binário (PE)

| Seção     | Tamanho Virtual | Características                  |
|-----------|------------------|----------------------------------|
| `.text`   | ~5.9 MB         | Código executável (READ/EXECUTE) |
| `.rdata`  | ~327 KB         | Dados somente leitura (strings, tabelas de import, constantes) |
| `.data`   | ~737 KB         | Dados inicializados              |
| `.data1`  | ~2.3 KB         | Dados adicionais                 |
| `.rsrc`   | ~178 KB         | Recursos (ícones, diálogos, version info) |

- **Subsystem:** Windows GUI
- **Relocations:** Stripped
- **Symbols / Line numbers:** Stripped
- **Arquitetura:** x86 32-bit apenas

### DLLs Importadas

| DLL              | Função principal                          |
|------------------|-------------------------------------------|
| `aomac.dll`      | Lógica principal do jogo (core do motor)  |
| `KERNEL32.dll`   | Sistema de arquivos, memória, threads     |
| `USER32.dll`     | Janelas, input, mensagens                 |
| `GDI32.dll`      | Desenho 2D básico                         |
| `ADVAPI32.dll`   | Registro do Windows                       |
| `ole32.dll` / `OLEAUT32.dll` | COM / OLE                        |
| `WINMM.dll`      | Multimídia / timers                       |
| `WS2_32.dll`     | Sockets (rede)                            |
| `MSVCRT.dll` / `MSVCP60.dll` | Runtime C/C++ Visual Studio 6   |
| `IMM32.dll`      | Input Method Editor                       |
| `VERSION.dll`    | Informações de versão                     |
| `granny.dll`     | Sistema de animação 3D (RAD Game Tools)   |
| `mss32.dll`      | Miles Sound System (áudio)                |
| `RockallDLL.dll` | Alocador de memória customizado           |
| `RPCRT4.dll`     | RPC                                       |

---

## 3. Arquitetura Interna do Motor

O motor foi organizado em três grandes camadas de código-fonte (visíveis pelos caminhos de debug embutidos):

### 3.1 Camada Core (`D:\core\`)

Infraestrutura genérica reutilizada:

- `d3drenderer.cpp` / `d3dtexture.cpp` — Renderização Direct3D
- `camera.cpp` — Sistema de câmera
- `ParticleSystemManager.cpp` — Partículas
- `Lightning.cpp` — Efeitos de raio
- `BSoundSet.cpp` — Conjuntos de som
- `gad*.cpp` — Sistema de gadgets de UI (botões, listas, sliders, textboxes, menus, diálogos, palettes)
- `screen.cpp` / `uilayout.cpp` — Layout de telas
- `pointer.cpp` — Cursor / ponteiro
- `random.cpp` — Gerador de números aleatórios

### 3.2 Camada RTS Genérica (`D:\rts3\`)

Código compartilhado da família Age of Empires / Age of Mythology:

- `agame.cpp` / `agameUI.cpp` — Loop de jogo e UI principal
- `amain.cpp` — Entry point de aplicação
- `aplayer.cpp` — Jogador
- `amodepregame.cpp` / `amodesimulation.cpp` — Modos de tela (pré-jogo e simulação)
- `agadfavor.cpp` — Gadget de Favor
- `agadarmybanner.cpp` / `agadidlebanner.cpp` — Banners de exército / unidades ociosas
- `acampaignscreen.cpp` / `acampaignloadingscreen.cpp` — Telas de campanha
- `aeditorscreen.cpp` / `aeditorui.cpp` — Editor de cenários
- `AMP*.cpp` — Sistema multiplayer (Game Room, Setup, Communications)

### 3.3 Camada Específica de Titans (`D:\titan\`)

Lógica exclusiva de Age of Mythology / The Titans:

#### Núcleo do jogo
- `game.cpp` / `gameui.cpp` / `gameuistate.cpp`
- `world.cpp` — Mundo / mapa
- `player.cpp`
- `unit.cpp` + dezenas de ações de unidade
- `squad.cpp` / `squadai.cpp`

#### Unidades e ações
- `unitaction.cpp`
- `unitmoveaction.cpp` / `unitmovebygroupaction.cpp`
- `unitgatheraction.cpp` / `unitautogatheraction.cpp`
- `unitrangedattackaction.cpp` / `unitchargedrangedattackaction.cpp`
- `unitbuckattack.cpp` / `unitjumpattackaction.cpp` / `unitwhirlwindattackaction.cpp`
- `unitconvertaction.cpp` / `unitautoconvertaction.cpp`
- `unitexploreaction.cpp` / `unitwanderaction.cpp`
- `unitidleaction.cpp` / `unitdeathaction.cpp`
- `unitthrowaction.cpp` / `unitlightaction.cpp`
- `unitai.cpp` / `unitgoreaction.cpp`

#### Poderes dos deuses (lista quase completa)
- `power.cpp` (base)
- `powerbolt.cpp`, `powerchaos.cpp`, `powerclone.cpp`
- `powerdamageunit.cpp`, `powerearthquake.cpp`, `powerenrage.cpp`
- `powerforestfire.cpp`, `powerfrost.cpp`, `powergaiaforest.cpp`
- `powerhealing.cpp`, `powerhealingspring.cpp`, `powerheroize.cpp`
- `powerimplode.cpp`, `powerinferno.cpp`, `powerlightning.cpp`
- `powerlocust.cpp`, `powermeteor.cpp`, `powerpeace.cpp`
- `powerprosperity.cpp`, `powerreversetime.cpp`, `powersnowstorm.cpp`
- `powerswapunit.cpp`, `powertartariangate.cpp`, `powertornado.cpp`
- `powertremor.cpp`, `powerundermine.cpp`, `powervolcano.cpp`
- `powervortex.cpp`, `poweranimalmagnet.cpp`
- `protopowerspiders.cpp`, `protopowertempunit.cpp`
- `powerrain.cpp`, `powerspiders.cpp`

#### IA e Knowledge Base
- `ai.cpp` / `aifuncs.cpp`
- `aiplanattack.cpp` / `aiplanbuildwall.cpp` / `aiplanexplore.cpp` / `aiplantransport.cpp`
- `aiprompt.cpp` / `CPAIPersonalities.cpp`
- `kb.cpp` / `kbplayer.cpp` / `kbtechtree.cpp` / `kbprogression.cpp`
- `kbattackroutemanager.cpp` / `kbbuildingplacement.cpp`

#### Terreno e Random Map
- `terrain.cpp` / `terraintypemanager.cpp`
- `randommap.cpp` / `randommapfuncs.cpp`
- `rmarea.cpp` / `rmobjectdef.cpp` / `rmConnection.cpp` / `rmshuffler.cpp`
- `mix.cpp` / `lush.cpp` / `forestinfo.cpp` / `waterinfo.cpp` / `TownLandscaper.cpp`

#### Outros
- `relicmanager.cpp`
- `formation.cpp` / `armor.cpp` / `ballistic.cpp`
- `animset.cpp` / `effects.cpp` / `brush.cpp`
- `WeatherManager.cpp` / `ambientsoundmanager.cpp`
- `director.cpp` / `triggerfuncs.cpp`
- `gadtechtree.cpp` / `gadtrigger.cpp` / `gadxsdebugger.cpp`

---

## 4. Formatos de Arquivo Suportados

O motor lê diretamente os seguintes formatos (sem necessidade de extração prévia na maioria dos casos):

| Extensão | Uso |
|----------|-----|
| `.bar`   | Archives principais de recursos (data, textures, models, anim, sounds, scenarios, godpowers) |
| `.xmb`   | Dados binários de jogo (proto, techtree, majorgods, etc.) |
| `.ddt`   | Texturas comprimidas (DXT1/DXT3/RGB/RGBA) |
| `.brg` / `.grn` | Malhas 3D + animações (formato Granny da RAD Game Tools) |
| `.cpn` / `.cpx` | Campanhas (base e Titans) |
| `.scn`   | Cenários |
| `.btx` / `.tga` | Cursores e elementos de UI |
| `.cam`   | Câmeras pré-definidas |
| `.lgt`   | Iluminação |
| `.dcl`   | Decals (ex: `corpse.dcl`) |
| `.prt`   | Partículas |
| `.cub`   | Cubemaps / skybox |
| `.xs` / `.lxs` | Scripts de trigger e AI |
| `.xml`   | UI, configurações, protos e techs em formato texto |
| `.txt`   | Animações (`*_anim.txt`), techs, units, terrain blends |
| `.bik`   | Vídeos originais (Bink Video) |

### Archives BAR conhecidos

**Base:**
- `data.bar`
- `textures.bar`
- `models.bar`
- `anim.bar`
- `sounds.bar`
- `scenarios.bar`
- `godpowers.bar`
- `ppdata.bar`
- `gamesettings.bar` (quando presente)

**Expansão The Titans (overlay):**
- `data2.bar`
- `textures2.bar`
- `models2.bar`
- `anim2.bar`
- `sounds2.bar`
- `scenarios2.bar`
- `godpowers2.bar`
- `gamesettings2.bar`

O motor monta a expansão **por cima** do base (últimas entradas sobrescrevem as anteriores).

### Arquivos XMB / XML importantes

- `proto.xmb` / `protox.xmb` — Definições de unidades
- `techtree.xmb` / `techtreex.xmb` — Árvore tecnológica
- `majorgods.xmb` / `minorgods.xmb`
- `relics.xml` / `relics2.xml`
- `terraintypes.xml` / `terrain\blends.txt`
- Diversos `ui*.xml` (options, campaign, godpicker, minimap, postgame, etc.)

---

## 5. Sistemas de Jogo Detalhados

### 5.1 Economia e Recursos

Recursos rastreados:
- **Food**
- **Wood**
- **Gold**
- **Favor**

Strings de debug mostram acompanhamento de renda em intervalos de 1, 2, 5 e 10 minutos:

```
1  Minute Income: Gold=%.2f, Wood=%.2f, Food=%.2f, Favor=%.2f.
```

Existem planos de IA específicos por recurso (`NumFoodPlan`, `NumWoodPlan`, `NumGoldPlan`, `NumFavorPlan`) e breakdown de coletores.

### 5.2 Unidades e Ações

O sistema de unidades é orientado a **ações** (Action Pattern):

- Movimento (simples e por grupo)
- Coleta (manual e automática)
- Ataque corpo-a-corpo e à distância
- Conversão
- Exploração / Wander
- Morte / Idle
- Ações especiais (Jump Attack, Whirlwind, Buck, Throw, Light, Charge Ranged)

Classes abstratas importantes:
- `AbstractVillager`
- `AbstractBarracks`
- `AbstractTradeUnit`
- `AbstractTitan`

### 5.3 Pathfinding e Formações

- `ObstructionManager`
- `PathPlan`
- `AttackRoute` / `AttackRoutePattern`
- `AreaGroup`
- `Formation`
- `AutoMover`
- `unitmovebygroupaction.cpp`

### 5.4 Sistema de IA

A IA é baseada em **planos** (Plans):

| Plano | Função |
|-------|--------|
| `AttackPlan` | Ataques |
| `BuildPlan` / `Build Settlement Plan` | Construção |
| `MovePlan` | Movimentação |
| `ExplorePlan` | Exploração |
| `TownDefensePlan` | Defesa da cidade |
| `PathPlan` | Pathfinding de alto nível |
| `BAIPlanGodPower` | Uso de poderes divinos |
| `ProgressPlan` / `ChildTechProgression` / `ChildUnitProgression` | Progressão tecnológica e de unidades |

Knowledge Base (KB):
- `kb.cpp`, `kbplayer.cpp`, `kbtechtree.cpp`, `kbprogression.cpp`
- Gerenciamento de rotas de ataque e colocação de edifícios

### 5.5 Condições de Vitória

Modos detectados:
- **Conquest** (destruir Town Center inimigo)
- **Wonder**
- **Relic**
- **Sudden Death**
- **Score**

Há lógica específica para:
- `LogicalTypeNeededForVictory`
- `LogicalTypeValidReverseWonderTarget`
- `AIWonderDestroyed` / `PlayerWonderDestroyed`
- Relíquias com nomes completos (Ankh of Ra, Armor of Achilles, Eye of Horus, Fetters of Fenrir, etc.)

### 5.6 Idades e Deuses

Progressão de idades:
1. Archaic Age
2. Classical Age
3. Heroic Age
4. Mythic Age
5. Titan (via poder / construção)

Sistema de diálogo de upgrade de idade (`AgeUpgradeDialog`) com slots para:
- Nome e texto do deus
- Tecnologias míticas
- Unidades míticas
- Poderes

### 5.7 Poderes dos Deuses

Cada poder tem implementação própria (lista em 3.3). O sistema de IA possui planos específicos para escolher e lançar poderes (`FindBestAttackGodPowerPlan`, `FindBestTownDefenseGodPowerPlan`, `BAIPlanGodPower`).

### 5.8 Terreno e Random Map

- Geração procedural completa (`randommap.cpp`, `randommapfuncs.cpp`)
- Tipos de terreno (`terraintypemanager.cpp`)
- Áreas, conexões, objetos e misturas (`rmarea`, `rmConnection`, `rmobjectdef`, `mix`)
- Lush (vegetação dinâmica) e florestas
- Água (`waterinfo.cpp`)

### 5.9 Animações (Granny)

- Formato `.grn` / `.brg`
- Scripts de animação `*_anim.txt`
- Diretório de animações configurável (`AnimDir`)
- Suporte a low-poly (`%slow\LP %s.grn`)
- Controle de loop, rate, timeshift e death animations

### 5.10 Áudio

- Miles Sound System (`mss32.dll`)
- DirectSound
- Música de batalha dinâmica (`BattleMusicTrigger`, `ForceBattleMusic`)
- Ambient sounds
- Sons de unidades, conversão, vitória, etc.

### 5.11 Rede / Multiplayer

- Sockets (`WS2_32.dll`)
- Sistema Zone (Ensemble Studios Online)
- `ZoneStats.DLL` e reporting de estatísticas
- AMP (Age of Mythology multiplayer) — Game Room, Setup Mode, Communications
- Sincronização de estado (`BGameStream` com câmera, comandos, unidades selecionadas, sync updates)

### 5.12 UI (Gadget System)

Sistema próprio de gadgets (`gad*.cpp` no core + extensões no titan):
- Botões, listas, sliders, textboxes, menus, diálogos, radio buttons, palettes
- Layouts (`uilayout.cpp`, `titanuilayout.cpp`)
- Telas de campanha, editor, opções, pós-jogo, god picker, minimap, etc.
- Cursores customizados (`.btx` / `.tga`)

---

## 6. Fluxo de Inicialização (inferido)

1. Entry point (`0x004ce392`)
2. Inicialização do runtime C++ e Rockall allocator
3. Carregamento de `aomac.dll`
4. Inicialização de gráficos (Direct3D) e configuração (`gfxconfig`)
5. Abertura dos archives BAR (base + expansão como overlay)
6. Carregamento de protos, techtree, majorgods, godpowers
7. Inicialização de áudio (Miles + DirectSound)
8. Sistema de gadgets / UI
9. Entrada no menu principal (`MAIN_MENU`)
10. Ao iniciar partida/missão: carga de cenário/campanha, terreno, unidades, pathfinding e início do loop de simulação

---

## 7. Strings de Debug e Logging Relevantes

Exemplos úteis para entender o comportamento em runtime:

```
%s: gametime=%d.
UnitID: %s %d
%d ally units. / %d enemy units. / %d self units.
Food=%6.2f.  Wood=%6.2f.  Gold=%6.2f.  Favor=%6.2f.
Cost: Food(%d), Wood(%d), Gold(%d), Favor(%d)
%d. Train Unit: %s (%d)
%d. Research Tech: %s (%d)
BAIPlanGodPower::update : %s - cast god power
BAIPlanGodPower::failed : %s
BRandomMapGenerator::initializeTerrain -- failed: ...
BD3DTexture::load ddt: failed to read level size info
Could not create build plan.
Loaded campaign from disk ... : %d scenarios
```

---

## 8. Proteção Original e Crack

- Proteção original: **SafeDisc** (secdrv.sys)
- O executável contém a marca `!CRACKED BY VENGEANCE`
- Relocations e símbolos foram removidos (típico de release + crack)
- Checksum zerado

---

## 9. Implicações para o Port Nativo (AoM-Native-Port)

Este binário **não deve ser recompilado ou emulado** como estratégia principal. Ele serve como:

1. **Referência de comportamento** — nomes de sistemas, ordem de carregamento, regras de vitória, economia e IA.
2. **Fonte de nomes de arquivos e estruturas** — confirma quais `.bar`, `.xmb` e arquivos de UI existem e como são referenciados.
3. **Validação de parsers** — o que o seu `BarArchive` + `GameDataManager` deve ser capaz de carregar.
4. **Lista de poderes e ações de unidade** — útil para priorizar o que implementar no MVP e nas fases seguintes.
5. **Referência de overlay da expansão** — confirma que `*2.bar` sobrescreve o base.

### Pontos de atenção para o projeto nativo

| Área no original | Status recomendado no port |
|------------------|----------------------------|
| Renderização DirectX | Já substituída por GLES 3 |
| Input mouse/teclado | Mapear para touch (tap/drag/pinch/pan) |
| Granny animations | Ainda não interpretadas (pendência importante) |
| Pathfinding A* | Já existe protótipo |
| Conquest victory | Existe bug de ordem de checagem (já documentado) |
| ANR no início de missão | Carga síncrona na thread de UI (corrigir com loading assíncrono) |
| Poderes dos deuses | Carregados via overlay; lógica de casting ainda parcial |
| Multiplayer / Zone | Fora do escopo do MVP |

---

## 10. Resumo Executivo

`aomxnocd.exe` é um motor RTS completo de 2003–2004, escrito em C++ com Visual Studio 6, usando DirectX, Granny, Miles Sound System e um sistema de gadgets próprio. Ele carrega dados através de archives `.bar` com suporte a overlay da expansão The Titans, define unidades e tecnologias via `.xmb`, controla o jogo através de um sistema de ações de unidade e planos de IA, e possui condições de vitória, poderes divinos, terreno procedural e multiplayer.

Para o projeto **AoM-Native-Port**, este executável é a melhor referência de comportamento disponível. O caminho correto continua sendo a engine nativa em C++20 + GLES3 que já está sendo construída, usando os formatos de dados originais e este documento como guia de fidelidade de comportamento.

---

*Documento gerado a partir de análise estática do binário (strings, imports, seções PE, caminhos de debug embutidos e classificação de sistemas). Última atualização: 2026-11-05.*
