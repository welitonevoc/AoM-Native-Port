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

**Sobre a expansão *The Titans*:** lançada em setembro de 2003, *Age of Mythology: The Titans* adiciona a civilização Atlantean (deuses maiores Kronos, Oranos e Gaia), novas unidades míticas (Behemoth, Automaton, Argus, entre outras), o conceito de **Titan** invocável, auto-queue de produção, novos poderes divinos e a campanha *The New Atlantis* (`xpack campaign.cpx`). Nos archives, o conteúdo da expansão convive com o jogo base por meio dos arquivos `*2.bar` (`data2.bar`, `textures2.bar`, `models2.bar`, `anim2.bar`, `sounds2.bar`, `scenarios2.bar`, `godpowers2.bar`) e dos dados `protox.xmb` / `techtreex.xmb`, montados em **overlay** sobre o base: entradas homônimas da expansão substituem as do original; entradas exclusivas são adicionadas. O `aomxnocd.exe` é o binário da expansão (não o `aom.exe` do jogo base) e espera essa estrutura de dados.

**Sobre o crack NoCD:** a proteção original da Microsoft usava **SafeDisc** (driver `secdrv.sys` e checagens de mídia física). O executável marcado com `!CRACKED BY VENGEANCE` tem essa verificação removida ou contornada, permitindo iniciar o jogo **sem inserir o CD/DVD original**. Isso não elimina a necessidade das DLLs (`aomac.dll`, `granny.dll`, `mss32.dll`, `RockallDLL.dll`) nem dos arquivos de dados (`.bar`, `.xmb`, campanhas, etc.): apenas dispensa a presença da mídia física no drive. Em sistemas modernos o driver SafeDisc costuma ser bloqueado; por isso versões NoCD (ou o patch de compatibilidade da comunidade / Voobly) são amplamente usadas para rodar o jogo original.

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

---

## 11. Aprofundamento — Sistema de Poderes dos Deuses

### 11.1 Arquivos-fonte de poderes encontrados no binário

Cada poder possui implementação própria:

| Arquivo | Poder |
|---------|-------|
| `powerbolt.cpp` | Bolt |
| `powerchaos.cpp` | Chaos |
| `powerclone.cpp` | Clone |
| `powerdamageunit.cpp` | Damage Unit |
| `powerearthquake.cpp` | Earthquake |
| `powerenrage.cpp` | Enrage |
| `powerforestfire.cpp` | Forest Fire |
| `powerfrost.cpp` | Frost |
| `powergaiaforest.cpp` | Gaia Forest |
| `powerhealing.cpp` | Healing |
| `powerhealingspring.cpp` | Healing Spring |
| `powerheroize.cpp` | Heroize |
| `powerimplode.cpp` | Implode |
| `powerinferno.cpp` | Inferno |
| `powerlightning.cpp` | Lightning |
| `powerlocust.cpp` | Locust Swarm |
| `powermeteor.cpp` | Meteor |
| `powerpeace.cpp` | Peace |
| `powerprosperity.cpp` | Prosperity |
| `powerreversetime.cpp` | Reverse Time |
| `powersnowstorm.cpp` | Snow Storm |
| `powerswapunit.cpp` | Swap Unit |
| `powertartariangate.cpp` | Tartarian Gate |
| `powertornado.cpp` | Tornado |
| `powertremor.cpp` | Tremor |
| `powerundermine.cpp` | Undermine |
| `powervolcano.cpp` | Volcano |
| `powervortex.cpp` | Vortex |
| `poweranimalmagnet.cpp` | Animal Magnetism |
| `powerrain.cpp` | Rain |
| `powerspiders.cpp` / `protopowerspiders.cpp` | Spiders |
| `protopowertempunit.cpp` | Temporary Unit (genérico) |

### 11.2 Categorias de poderes

- **GenericGodPower**
- **EconomicGodPower**
- **OffensiveGodPower**
- **MythUnitGodPower**
- **SpecialPowers**

### 11.3 Slots por idade (UI / lógica)

```
FirstAge-GodPower
SecondAge-First-MinorGod-GodPower
SecondAge-Second-MinorGod-GodPower
ThirdAge-First-MinorGod-GodPower
ThirdAge-Second-MinorGod-GodPower
FourthAge-First-MinorGod-GodPower
FourthAge-Second-MinorGod-GodPower
```

### 11.4 API de IA para poderes (funções expostas aos scripts XS)

```cpp
bool aiCastGodPowerAtPosition(int godPowerTechID, vector pos);
bool aiCastGodPowerAtUnit(int godPowerTechID, int unitID);
bool aiSetGodPowerEventHandler(string handlerFunctionName);
int  aiFindBestAttackGodPowerPlan(void);
int  aiFindBestTownDefenseGodPowerPlan(void);
int  aiGetGodPowerProtoIDForTechID(...);
int  aiGetGodPowerTechIDForSlot(...);
int  aiGetGodPowerType(...);
bool aiTaskUnitSpecialPower(int unitID, ...);
```

### 11.5 Ciclo de vida de um plano de poder (BAIPlanGodPower)

1. Criação do plano
2. `setupBuildingPlacement` (quando o poder envolve posicionamento)
3. Validação de influências / distâncias
4. `update` → tentativa de cast (`cast god power`)
5. Possíveis falhas:
   - `BAIPlanGodPower::failed`
   - `failed to place god power`
   - `failed to start placement for god power`

### 11.6 Identificadores internos

- `PowerID`
- `PowerProtoID`
- `PowerTechID`
- `GodPowerExclusion` / `GodPowerExclusionHesperides`
- `GodPowersSync` (sincronização em multiplayer)

---

## 12. Aprofundamento — Sistema de IA (Plans + Knowledge Base)

### 12.1 Gerenciador de planos (AIPM)

Mensagens de erro revelam a estrutura interna:

```
AIPM::createPlan: %d is invalid type.
AIPM::createPlan: '%s' is already in use by planIndex=%d.
AIPM::createPlan: '%s' is too long for a plan name.
AIPM::createPlan: Addition to mMainPlans failed.
AIPM::createPlan: Addition to mPlans failed.
AIPM::createPlan: Addition to parent failed.
AIPM::createPlan: Invalid KB.
AIPM::createPlan: NULL planName.
AIPM::createPlan: No plan created.
```

Estruturas inferidas:
- `mPlans` — lista geral de planos
- `mMainPlans` — planos principais
- Relação pai/filho entre planos
- Dependência obrigatória de um Knowledge Base (KB) válido

### 12.2 Tipos de planos identificados

| Plano | Uso |
|-------|-----|
| `AttackPlan` | Ataques coordenados |
| `BuildPlan` | Construção de edifícios |
| `MovePlan` | Movimentação de grupos |
| `ExplorePlan` | Exploração do mapa |
| `TownDefensePlan` | Defesa da cidade |
| `PathPlan` | Pathfinding de alto nível |
| `BAIPlanGodPower` | Lançamento de poderes |
| `ProgressPlan` / `ChildTechProgression` / `ChildUnitProgression` | Progressão tecnológica e de unidades |
| `NumFoodPlan` / `NumWoodPlan` / `NumGoldPlan` / `NumFavorPlan` | Metas de recursos |

### 12.3 BuildPlan — estados e falhas comuns

O `BuildPlan` possui máquina de estados bem definida. Falhas registradas:

- Não consegue pagar o edifício
- Placement nulo ou inválido
- Tipo de unidade de construção inválido
- Sem unidades restantes para construir
- Não consegue pathfind até o ponto de construção
- Foundations restantes sem conclusão
- Reinício por placement ID inválido

### 12.4 Knowledge Base (KB)

Arquivos-fonte:
- `kb.cpp`
- `kbplayer.cpp`
- `kbtechtree.cpp`
- `kbprogression.cpp`
- `kbattackroutemanager.cpp`
- `kbbuildingplacement.cpp`

Responsabilidades:
- Representação do estado do mundo para a IA
- Árvore tecnológica do jogador
- Rotas de ataque
- Colocação inteligente de edifícios
- Progressão de unidades e tecnologias

### 12.5 Funções de script XS relacionadas a planos

Constantes geradas dinamicamente:
```cpp
const int cAttackPlan%s=%d;
const int cAttackPlanAttackRoutePattern%s=%d;
const int cAttackPlanBaseAttackMode%s=%d;
const int cAttackPlanRetreatMode%s=%d;
const int cBuildPlan%s=%d;
const int cExplorePlan%s=%d;
```

---

## 13. Aprofundamento — Carregamento de Dados (BAR / XMB)

### 13.1 Archives BAR conhecidos

**Base game:**
```
data.bar
textures.bar
models.bar
anim.bar
sounds.bar
scenarios.bar
godpowers.bar
ppdata.bar
gamesettings.bar   (quando presente)
```

**The Titans (overlay — montados por cima):**
```
data2.bar
textures2.bar
models2.bar
anim2.bar
sounds2.bar
scenarios2.bar
godpowers2.bar
gamesettings2.bar
```

### 13.2 Semântica de overlay (comportamento original)

1. O archive base é aberto primeiro.
2. O archive da expansão é montado por cima.
3. Entradas com o mesmo nome na expansão **substituem** as do base.
4. Entradas exclusivas da expansão são **adicionadas**.
5. A busca é case-insensitive e normaliza `\` → `/`.

### 13.3 Arquivos XMB / XML críticos

| Arquivo | Conteúdo |
|---------|----------|
| `proto.xmb` | Unidades do jogo base |
| `protox.xmb` | Unidades da expansão (super-set + atualizações) |
| `techtree.xmb` | Tecnologias base |
| `techtreex.xmb` | Tecnologias da expansão |
| `majorgods.xmb` | Deuses maiores |
| `minorgods.xmb` | Deuses menores |
| `relics.xml` / `relics2.xml` | Relíquias |
| `terraintypes.xml` | Tipos de terreno |
| `terrain\blends.txt` | Regras de blend de texturas |

### 13.4 Outros arquivos de dados referenciados

```
%sproto.txt
%sproto101x.xml
%stempproto.xml
%stemptechtree.xml
%sfaketechtreecosts.xml
%smajorgods.xml
%sminorgods.xml
%sheroNames.xml
%sicons.xml
%splayercolors.xml
%splaylist.xml
%spredefinedchats.xml
%sunitaitypes.txt
%sunits\%s.txt
%stechs\%s.txt
%stable of contents.xml
```

### 13.5 Fluxo de carregamento inferido

1. Localizar pasta de dados do jogo.
2. Abrir `data.bar` + montar `data2.bar`.
3. Abrir `textures.bar` + montar `textures2.bar`.
4. Abrir `models.bar` + montar `models2.bar`.
5. Abrir demais archives (anim, sounds, scenarios, godpowers) com o mesmo padrão.
6. Extrair / mapear `proto.xmb` → depois `protox.xmb` (deduplicação por nome).
7. Extrair `techtree.xmb` → depois `techtreex.xmb`.
8. Carregar majorgods, minorgods, godpowers.
9. Carregar campanhas (`main campaign.cpn` + `xpack campaign.cpx`).

### 13.6 Implicações para o BarArchive do port nativo

O comportamento que o seu `BarArchive` com múltiplos mounts já implementa (Sprint 11) está **alinhado** com o original:

- `mount()` empilha archives
- Último mount tem precedência
- `open_pair(base, expansion)` é o padrão correto
- Normalização de nomes case-insensitive

---

## 14. Aprofundamento — Ações de Unidade

Lista completa de ações encontradas nos caminhos de debug:

| Arquivo | Ação |
|---------|------|
| `unitmoveaction.cpp` | Movimento simples |
| `unitmovebygroupaction.cpp` | Movimento em grupo / formação |
| `unitgatheraction.cpp` | Coleta manual |
| `unitautogatheraction.cpp` | Coleta automática |
| `unitrangedattackaction.cpp` | Ataque à distância |
| `unitchargedrangedattackaction.cpp` | Ataque à distância com carga |
| `unitbuckattack.cpp` | Ataque de coice |
| `unitjumpattackaction.cpp` | Ataque com salto |
| `unitwhirlwindattackaction.cpp` | Ataque em redemoinho |
| `unitconvertaction.cpp` | Conversão |
| `unitautoconvertaction.cpp` | Conversão automática |
| `unitexploreaction.cpp` | Exploração |
| `unitwanderaction.cpp` | Vaguear |
| `unitidleaction.cpp` | Idle |
| `unitdeathaction.cpp` | Morte |
| `unitthrowaction.cpp` | Arremesso |
| `unitlightaction.cpp` | Ação de iluminação / luz |
| `unitaction.cpp` | Classe base de ações |
| `unitai.cpp` | IA de unidade individual |
| `unitgoreaction.cpp` | Reação de gore / sangue |

Padrão de design: **Action Pattern**. Cada unidade possui uma ação ativa e pode enfileirar ações futuras.

---

## 15. Aprofundamento — Condições de Vitória e Relíquias

### 15.1 Modos de vitória

- **Conquest** — destruir o Town Center inimigo
- **Wonder** — construir e defender uma Maravilha
- **Relic** — controlar relíquias
- **Sudden Death**
- **Score**

Flags e tipos lógicos:
- `LogicalTypeNeededForVictory`
- `LogicalTypeValidReverseWonderTarget`
- `LogicalTypeValidSPCUnitsDeadCondition`
- `AIWonderDestroyed` / `PlayerWonderDestroyed`

### 15.2 Relíquias identificadas (nomes completos)

```
Relic Ankh of Ra
Relic Anvil of Hephaestus
Relic Armor of Achilles
Relic Arrows of Alfar
Relic Black Lotus
Relic Blanket of Empress Zoe
Relic Boots of Kick Everything
Relic Bow of Artemis
Relic Buhen Flagstone
Relic Canopic Jar of Imsety
Relic Catoblepas Scales
Relic Dwarfen Calipers
Relic Eye of Horus
Relic Eye of Ornlu
Relic Fetters of Fenrir
Relic Gaias Book of Knowledge
Relic Girdle of Hippolyta
Relic Golden Bridle of Pegasus
Relic Golden Lions
Relic Harmonias Necklace
Relic Harters Folly
Relic Head of Orpheus
Relic Heras Thundercloud Shawl
Relic Khopesh of Horus
Relic Kithara of Apollo
Relic Mithril Horseshoes
Relic Monkey Head
```

Há também respawns especiais (`Golden Lions Relic Respawn`, `Monkey Head Relic Respawn`, `Pegasus Relic Respawn`).

---

## 16. Mapa de Prioridades para o AoM-Native-Port

Com base em tudo extraído do `aomxnocd.exe`, a ordem recomendada de fidelidade de comportamento é:

| Prioridade | Sistema | Motivo |
|------------|---------|--------|
| P0 | Carregamento BAR + overlay | Já implementado e validado |
| P0 | Proto / Techtree (XMB) | Base de todas as unidades e custos |
| P0 | Correção do ANR de missão | Bloqueante para jogabilidade |
| P1 | Town Center + economia básica | Fecha o loop do MVP |
| P1 | Conquest victory (ordem correta) | Bug já identificado |
| P1 | Ações de unidade (move, gather, attack) | Core do gameplay |
| P2 | Pathfinding + formações | Qualidade de movimento |
| P2 | Animações Granny básicas (idle/walk/attack) | Feedback visual |
| P3 | Poderes dos deuses (casting) | Diferencial do AoM |
| P3 | IA com planos simples | Single-player viável |
| P4 | Relíquias / Wonder / Score | Modos extras |
| P5 | Multiplayer / Zone | Fora do escopo atual |

---

*Seções 11–16 adicionadas em 2026-11-05 como aprofundamento solicitado.*

---

## 17. Aprofundamento — Tipos Abstratos de Unidades e Edifícios

O motor classifica unidades e edifícios em tipos abstratos usados por IA, efeitos e regras:

### Unidades
```
AbstractVillager
AbstractInfantry
AbstractArcher
AbstractCavalry
AbstractSiegeWeapon
AbstractScout
AbstractPharaoh
AbstractTradeUnit
AbstractCart
AbstractTitan
```

### Edifícios
```
AbstractSettlement      (Town Center / Settlements)
AbstractBarracks
AbstractArcheryRange
AbstractStable
AbstractTemple
AbstractArmory
AbstractDock
AbstractFarm
AbstractFortress
AbstractMonument
AbstractWall
```

Esses tipos são referenciados em:
- Planos de IA (`BuilderUnitType`, `BuildingAbstractTypeID`)
- Efeitos de dados (`BDataEffect` com atributo `unittype` / `protounit`)
- Lógica de combate e placement

---

## 18. Aprofundamento — Civilizações, Deuses e Idades

### 18.1 Civilizações (Cultures)

```
Greek
Egyptian
Norse
Atlantean
```

Strings de suporte:
- `Civilization` / `CivilizationLogic`
- `Culture` / `CultureLogic` / `CultureIcon` / `AllowedCulture`
- Aceleradores específicos: `AtlanteanBarracksAccel`, `EgyptBarracksAccel`, `AtlanteanPalaceAccel`, `AtlanteanSiegeShopAccel`

### 18.2 Deuses Maiores por idade (Age 1)

| Civilização | Deuses Age 1 |
|-------------|--------------|
| Greek       | Zeus, Poseidon, Hades |
| Egyptian    | Ra, Isis, Set |
| Norse       | Odin, Thor, Loki |
| Atlantean   | Kronos, Oranos (Ouranos), Gaia |

### 18.3 Deuses Menores por idade

**Age 2 (Classical):**
- Greek: Athena, Hermes, Ares
- Egyptian: Bast, Ptah, Anubis
- Norse: Freyja, Heimdall, Forseti
- Atlantean: Prometheus, Leto, Oceanus (Okeanus)

**Age 3 (Heroic):**
- Greek: Apollo, Dionysos, Aphrodite
- Egyptian: Hathor, Sekhmet, Nephthys
- Norse: Skadi, Bragi, Njord
- Atlantean: Hyperion, Rheia, Theia

**Age 4 (Mythic):**
- Greek: Hera, Hephaestus, Artemis
- Egyptian: Osiris, Horus, Thoth
- Norse: Baldr, Tyr, Hel
- Atlantean: Hekate, Atlas, Helios

Também existem entradas `Age 1_5` (transição) e variantes `Fake` / `Atlantean Heroes`.

---

## 19. Aprofundamento — Sistema de Combate e Dano

### 19.1 Tipos de dano / armadura

```
AttackPierce
AttackCrush
Armor
ArmorNormal
ArmorPierce
ArmorVulnerability
Crush
FireDamage
DamageBonus
DamageMax
DamageToBuildPercent
DamageToUpgrade
```

### 19.2 Projéteis e balística

Arquivos-fonte: `ballistic.cpp`, `armor.cpp`

```
Ballistic
BallisticBounceProto
BallisticImpactProto
BallisticSplashProto
BallisticSync
DirectProjectile
DestroyProjectile
CollidesWithProjectiles
```

Flags de configuração:
- `Allows projectiles to damage units other than their intended target`
- `Allows projectiles to do friendly fire` (requer `ballisticCollateralDamage`)
- `Determines if we precalculate whether or not a projectile hits its target`

### 19.3 Lógica de dano

- `DamageLogic` / `DamageTypeLogic`
- `AreaDamageConstant`
- `BuildDamageFactor`
- Efeitos XML: `BDamageEffect` com atributo `protounit`

---

## 20. Aprofundamento — UI Completa (Gadget System + XML)

### 20.1 Arquivos XML de interface

**Menus e telas principais:**
```
uimain.xml
uipregame.xml
uipregamemsg1.xml / uipregamemsg2.xml
%suicampaign.xml / %suicampaignloading.xml
%suipostgamescreen.xml
%suigodpicker.xml
%suimappicker.xml
%suisprmsetup.xml
%suistrategicmap.xml
%suigamelist.xml / %suigamepalettes.xml
```

**Opções e diálogos:**
```
%suioptions.xml / %suioptionsdialog.xml
%suiothersettings.xml
%suihotkeys.xml
%suiobjectives.xml / uiobjectivesdialog.xml
%suichatdialog.xml
%suimpfriendslist.xml / %suimpvotedialog.xml
%suiplayerstatusdialog.xml
uihelpdialog.xml
uieula.xml
uitributedlg.xml
```

**Editor e debug:**
```
%suieditormenu.xml / %suieditorpalettes.xml
uitriggereditor.xml / uitriggroupeditor.xml
uixsdebugger.xml
uicameraeditor.xml
uianimationdlg.xml
uiarmyedit.xml
uifilemanager.xml
uiscensetdlg.xml
uitechtree.xml / data\uifaketechtree.xml
```

**Multiplayer / Zone:**
```
uimplogin.xml
uimpsetupfinal.xml
uimplanip.xml
uimpeso.xml
```

**Outros:**
```
%suidefaultloading.xml
%suarenasetup.xml
%suiingamemsg1.xml / %suiingamemsg3.xml
data\uiautopatcherdialog.xml
data\uirmgeneration.xml
uiaiprompt.xml
uiprofileselect.xml
uireceive.xml
uilowdetailbackground.xml
```

### 20.2 Gadgets e componentes de UI

```
-backgroundgadget
-containergadget
-button / -button1 / -button2 / -button-%d
-slider-%d
-scrollhoriz / -scrollvert / -vertscrollbar / -horizscrollbar
-textbox / -textBox / -textscroll / -textArea
-dropList / -dropButton
-tab-%d / -tabPosition
-titlebar
-close
-upButton / -downButton
```

Banners e painéis:
```
-bannerFurledButton-%d
-bannerUnfurledButton%d
-DisplayPanelInventory
-DisplayPanelMulti
-DisplayPanelPercentageText
-DisplayPanelQueue
-DisplayPanelQueuePortrait
BottomPanel
AGameMinimap
```

Diálogos especiais:
```
AgeUpgradeDialog (+ subcomponentes GodName, GodText, MythTech, MythUnit, Power, Portrait...)
AdvancedGraphicsOptionsDialog
BrushMaskDialog / BrushSettingsDialog
CampaignLoadingScreen / CampaignMenu
ArenaScreen
```

### 20.3 Cursores

```
%scursors\%s.btx
%scursors\%s.tga
AlwaysFullColorAsCursor
AlwaysShowCursor
```

---

## 21. Aprofundamento — Terreno e Random Map

### 21.1 Arquivos-fonte

```
terrain.cpp
terraintypemanager.cpp
randommap.cpp
randommapfuncs.cpp
rmarea.cpp
rmobjectdef.cpp
rmConnection.cpp
rmshuffler.cpp
mix.cpp
lush.cpp
forestinfo.cpp
waterinfo.cpp
TownLandscaper.cpp
```

### 21.2 Conceitos

- **Terrain types** definidos em `terraintypes.xml`
- **Blends** em `terrain\blends.txt`
- **Áreas** (`rmarea`) com min/max blob distance
- **Conexões** entre áreas (`rmConnection`)
- **Objetos** de mapa (`rmobjectdef`)
- **Mix** de terrenos
- **Lush** (vegetação dinâmica / Gaia)
- **Water** e elevação
- **2PassWaterFog**

### 21.3 Mensagens de erro de terreno

```
BRandomMapGenerator::initializeTerrain -- failed: could not allocate terrain
BRandomMapGenerator::initializeTerrain -- failed: could not find mix specified
BRandomMapGenerator::initializeTerrain -- failed: could not get gameuistate
BRandomMapGenerator::initializeTerrain -- failed: could not get terrain type manager
BRandomMapGenerator::initializeTerrain -- failed: terrain did not initialize properly
BRandomMapGenerator::initializeTerrain -- failed: you must set a valid sea type to create a water based map
'%s' is not a valid terrain type
```

---

## 22. Aprofundamento — Áudio e Música

### 22.1 Tecnologias

- **Miles Sound System** (`mss32.dll`)
- **DirectSound** (`DirectSoundCreate8`, `DirectSoundEnumerateW`)
- Ambient Sound Manager (`ambientsoundmanager.cpp`)
- Sound Sets (`BSoundSet.cpp`)

### 22.2 Tipos de áudio

```
AmbientSounds.xml
BattleMusicTrigger
ForceBattleMusic
CoreSound
DefaultButtonSound
```

Flags de configuração:
- Disables the in game music
- Disables the in game music in multiplayer / single player
- Disables the music on the intro screen
- Disables the victory sounds
- Disables unit exists sounds
- Enables dynamic music
- AllowAudioTauntsCheckBox

### 22.3 Playlist e taunts

```
%splaylist.xml
%spredefinedchats.xml
%s\xpack\xtaunts\%s
```

---

## 23. Aprofundamento — Rede, Zone e Multiplayer

### 23.1 Componentes

- Sockets (`WS2_32.dll`)
- AMP (Age of Mythology multiplayer):
  - `AMPCommunications.cpp`
  - `AMPGameRoom.cpp`
  - `AMPGameSetupMode.cpp`
  - `AMPCustomScreen.cpp`
  - `AMPGameSetupQuickTeamScreen`
  - `AMPSetupScreen`
- Zone / Ensemble Studios Online:
  - `ZoneStats.DLL`
  - ZoneFriends server
  - ZoneStats reporting
- Sincronização de estado (`BGameStream`):
  - Câmera, comandos, unidades selecionadas, sync updates
  - `inletBeginUpdate` / `outletBeginUpdate`

### 23.2 Portas históricas (referência)

- UDP 2300 (tráfego in-game)
- UDP 64520
- TCP 28800–28805 (ESO)
- UDP 6073 (DirectPlay initial)

### 23.3 Chat e amigos

```
AIChat / AIStatementsAsChat
Connected to ZoneFriends server
Disconnect message from ZoneFriends server
(%s) - User has been added to your friends list
(%s) - User has been removed from your friends list
(%s) - User is now %s
```

---

## 24. Aprofundamento — Triggers, Scripts XS e Efeitos

### 24.1 Sistema de triggers

Arquivos:
- `triggerfuncs.cpp`
- `gadtrigger.cpp` / `gadtrigparam.cpp`
- `gadxsdebugger.cpp`

Editores de UI:
- `uitriggereditor.xml`
- `uitriggroupeditor.xml`
- `uixsdebugger.xml`
- `ATrigGroupEditor` / `ATriggerGadget` / `ATriggerArmyGadget`

### 24.2 Efeitos de dados (XML)

Classes de efeito encontradas:
```
BDataEffect
BDamageEffect
BCreateUnitEffect
BCreatePowerEffect
BSetAgeEffect
BSoundEffect
BTechStatusEffect
BTextOutputEffect
BConsoleCommandEffect
BSetNameEffect
```

Atributos comuns de efeitos:
```
action, amount, civ, component, damagetype, relativity,
resource, slot, stat, tech, type, protounit, unittype,
protoPower, cultureName, x/y/z (offset)
```

### 24.3 Scripts

```
%s%s.xs
%s%s.lxs
%s%s.trg
// TEST TRIGGER SCRIPT
// CXSDump
BXSRuntime::interpretBreakpoint
BXSRuntime::interpretCode
BXSRuntime::interpretEvents
```

---

## 25. Aprofundamento — Formações, Squads e Grupos

### 25.1 Conceitos

```
Formation          (formation.cpp)
Squad / SquadAI    (squad.cpp, squadai.cpp)
AreaGroup
Army
LogicalGroup
Stance             (%s-stancetype)
```

### 25.2 Operações

```
AddUnitsToLogicalGroup
AdjustArmy
AE-CreateArmy / AE-DeleteArmy / AE-ArmyList / AE-ArmyName
AE-GhostArmy / AE-GhostArmyPlayer
```

### 25.3 Banners de UI

```
%s-bannerFurledButton-%d
%s-bannerUnfurledButton%d
agadarmybanner.cpp
agadidlebanner.cpp
```

---

## 26. Aprofundamento — Gráficos e Câmera

### 26.1 Renderização

- Direct3D (`d3drenderer.cpp`, `d3dtexture.cpp`)
- Texturas DDT com mipmaps e downsample de terreno
- `2PassWaterFog`
- Screen capture (`BD3DRenderer::screenCapture`)

### 26.2 Câmera

```
camera.cpp
ACameraTrackGadget
CameraMinimapRadius
CameraShake
CameraTracks
%s-cameratrack
%ssupascreen.cam / %ssupatemp.cam
AllowCameraRotationCheckBox
uicameraeditor.xml
```

### 26.3 Configuração gráfica

```
AdvancedGraphicsOptionsDialog (+ OK / Cancel / Reset)
Error - Initializing graphics config system failed
Error - Opening graphics config file failed
gfxconfig / gfxconfig2
```

---

## 27. Aprofundamento — Save, Load e Record Game

```
BRecordGame::startUpdatePlayback
BGameStream (serialização de updates)
%s00000000.tmp
Could not save tag data. Check banglog.txt
```

O sistema de record/playback grava:
- Tempo de update
- Comandos
- Unidades selecionadas
- Câmera
- Sync state

---

## 28. Aprofundamento — Configuração e Startup

### 28.1 Variáveis de configuração

```
// *** BEGIN PERSISTENT CONFIG VARS ***
// *** END PERSISTENT CONFIG VARS ***
CONFIGS
Load configuration data
Load user configuration data
```

### 28.2 Startup

```
ASI_startup
BCommManager::startup: failed to initialize protocol
Initializing animations
Initializing powers
GetStartupInfoA
GetVersionExA / GetVersionExW
```

### 28.3 Version info

- `GetFileVersionInfoA/W`
- `GetFileVersionInfoSizeA/W`
- Product name embutido: Microsoft Ensemble Studios Age of Mythology - The Titans Expansion

---

## 29. Lista Completa de Arquivos-Fonte Identificados (Debug Paths)

### Core (`D:\core\`)
```
BSoundSet.cpp, Lightning.cpp, ParticleSystemManager.cpp,
camera.cpp, d3drenderer.cpp, d3dtexture.cpp,
gadconsole.cpp, gadcore.cpp, gaddialog.cpp, gadfield.cpp,
gadfieldset.cpp, gadformattextbox.cpp, gadlist.cpp,
gadlistSimple.cpp, gadmenu.cpp, gadpalette.cpp, gadradio.cpp,
gadslider.cpp, gadtext.cpp, gadtextsimple.cpp,
pointer.cpp, screen.cpp, uilayout.cpp, random.cpp
```

### RTS3 (`D:\rts3\`)
```
AMPCommunications.cpp, AMPCustomScreen.cpp, AMPGameRoom.cpp,
AMPGameSetupMode.cpp, acampaignloadingscreen.cpp, acampaignscreen.cpp,
aeditorscreen.cpp, aeditorui.cpp, agadarmybanner.cpp, agadfavor.cpp,
agadgettimeline.cpp, agadidlebanner.cpp, agadscenariosummary.cpp,
agame.cpp, agameUI.cpp, agamedisplaypanel.cpp, alayout.cpp,
amain.cpp, amaingamescreen.cpp, amodepregame.cpp,
amodesimulation.cpp, aplayer.cpp, asprmscreen.cpp, randomteams.cpp
```

### Titan (`D:\titan\`) — principais grupos
```
ai.cpp, aifuncs.cpp, aiplanattack.cpp, aiplanbuildwall.cpp,
aiplanexplore.cpp, aiplantransport.cpp, aiprompt.cpp,
CPAIPersonalities.cpp,

kb.cpp, kbplayer.cpp, kbtechtree.cpp, kbprogression.cpp,
kbattackroutemanager.cpp, kbbuildingplacement.cpp,

unit.cpp, unitaction.cpp, unitai.cpp, unitmoveaction.cpp,
unitmovebygroupaction.cpp, unitgatheraction.cpp,
unitautogatheraction.cpp, unitrangedattackaction.cpp,
unitchargedrangedattackaction.cpp, unitbuckattack.cpp,
unitjumpattackaction.cpp, unitwhirlwindattackaction.cpp,
unitconvertaction.cpp, unitautoconvertaction.cpp,
unitexploreaction.cpp, unitwanderaction.cpp, unitidleaction.cpp,
unitdeathaction.cpp, unitthrowaction.cpp, unitlightaction.cpp,
unitgoreaction.cpp,

power.cpp + todos os power*.cpp listados na seção 11,

terrain.cpp, terraintypemanager.cpp, randommap.cpp,
randommapfuncs.cpp, rmarea.cpp, rmobjectdef.cpp,
rmConnection.cpp, rmshuffler.cpp, mix.cpp, lush.cpp,
forestinfo.cpp, waterinfo.cpp, TownLandscaper.cpp,

game.cpp, gameui.cpp, gameuistate.cpp, world.cpp, player.cpp,
squad.cpp, squadai.cpp, formation.cpp, armor.cpp, ballistic.cpp,
animset.cpp, effects.cpp, brush.cpp, director.cpp,
triggerfuncs.cpp, relicmanager.cpp, WeatherManager.cpp,
ambientsoundmanager.cpp,

gadtechtree.cpp, gadtrigger.cpp, gadtrigparam.cpp,
gadxsdebugger.cpp, gadcolordropdown.cpp, gadedgedensity.cpp,
titanuilayout.cpp, editorui.cpp
```

---

## 30. Checklist Final — O que o aomxnocd.exe Contém vs. o Port Nativo

| Subsistema no original | Presente no binário | Status no AoM-Native-Port |
|------------------------|---------------------|---------------------------|
| BAR loading + overlay | Sim | Implementado (Sprint 11) |
| XMB proto/techtree | Sim | Implementado |
| God powers data | Sim | Carregado; casting parcial |
| GLES / DirectX renderer | DirectX | GLES3 feito |
| Touch / Mouse input | Mouse+KB | Touch parcial |
| Unit actions (move/gather/attack) | Completo | Protótipo |
| Pathfinding A* | Sim | Protótipo |
| Formations / Squads | Sim | Pendente |
| AI Plans + KB | Completo | Pendente |
| Town Center + economia full | Sim | Parcial (falta TC) |
| Victory conditions | Completo | Bug CONQUEST conhecido |
| Granny animations | Sim | Não interpretado |
| Terrain / Random Map | Completo | Mapa procedural básico |
| Triggers / XS scripts | Completo | Fora do MVP |
| Campaign loading | Sim | Detectado; ANR no start |
| Audio (Miles/DS) | Sim | WAV PCM básico |
| Multiplayer / Zone | Sim | Fora do escopo |
| UI gadget system | Completo | UI touch custom |
| Save / Record game | Sim | Experimental |
| Relics / Wonder | Sim | Pendente |

---

*Documento expandido com seções 17–30 em 2026-11-05. Cobre a totalidade das strings, caminhos de debug, sistemas e estruturas identificáveis por análise estática do binário aomxnocd.exe.*

---

## 31. Aprofundamento — Logical Types (Sistema de Classificação de Unidades)

O motor usa **Logical Types** para consultas rápidas de IA, targeting de poderes, filtros de minimapa e regras de combate. Lista completa extraída do binário (84 tipos):

### Afetados por poderes
```
LogicalTypeAffectedByChaos
LogicalTypeAffectedByFrostGiant
LogicalTypeAffectedByHealingSpring
LogicalTypeAffectedByHeroize
LogicalTypeAffectedByRestoration
LogicalTypeAffectedByTownBell
LogicalTypeAffectedByVortex
LogicalTypeEscapesFrostOnTransform
LogicalTypeCanBeHealed
LogicalTypeCanBeStoned
LogicalTypeCanBeThrown
LogicalTypePerseusCanStone
```

### Alvos válidos de poderes específicos
```
LogicalTypeValidBoltTarget
LogicalTypeValidDeconstructionTarget
LogicalTypeValidFlamingWeaponsTarget
LogicalTypeValidForestFireTarget
LogicalTypeValidFrostTarget
LogicalTypeValidLocustSwarmTarget
LogicalTypeValidMeteorTarget
LogicalTypeValidShiftingSandsTarget
LogicalTypeValidSpiderTarget
LogicalTypeValidSpyTarget
LogicalTypeValidTraitorsTarget
LogicalTypeValidTremorAttack
LogicalTypeValidReverseWonderTarget
LogicalTypeEarthquakeAttack
LogicalTypeImplodeAttack
LogicalTypeTornadoAttack
LogicalTypeVolcanoAttack
LogicalTypeRamAttack
LogicalTypeSeaSerpentAttack
LogicalTypeTartarianGateValidOverlapPlacement
```

### Combate e auto-attack
```
LogicalTypeHandUnitsAttack
LogicalTypeHandUnitsAutoAttack
LogicalTypeRangedUnitsAttack
LogicalTypeRangedUnitsAutoAttack
LogicalTypeSiegeAutoAttack
LogicalTypeAutoattackTargetsBuildings
LogicalTypePredatorsAttack
LogicalTypePredatorsAutoAttack
LogicalTypeMeanPredatorsAutoAttack
LogicalTypeSuperPredatorsAttack
LogicalTypeSuperPredatorsAutoAttack
LogicalTypeVillagersAttack
LogicalTypeVillagersRespondToAttack
LogicalTypeScoutsRespondToAttack
```

### Categorias de unidades / edifícios
```
LogicalTypeAbstractVillNotHero
LogicalTypeIdleCivilian
LogicalTypeIdleHero
LogicalTypeIdleMilitary
LogicalTypeLandMilitary
LogicalTypeNavalMilitary
LogicalTypeMilitaryUnitsAndBuildings
LogicalTypeUnitsNotBuildings
LogicalTypeMythUnitNotTitan
LogicalTypeGreekHeroes
LogicalTypeNonGreekUnit
LogicalTypeCavalryAndScout
LogicalTypeFavoriteUnit
LogicalTypeFavoriteMythUnit
LogicalTypeDangerousAnimals
LogicalTypeConvertsHerds
LogicalTypeOdinRegenerates
LogicalTypeParticipatesInBattlecries
LogicalTypeGarrisonOnBoats
LogicalTypeShipNotFishinghip
LogicalTypeMummyMinionTarget
LogicalTypeTimeshift
LogicalTypeNoFlailOnThrow
LogicalTypeScreamsOnSpiderPull
LogicalTypeInvulnerableToGaia
LogicalTypeDoesNotCreateLush
LogicalTypeEasySelectAvoid
```

### Edifícios
```
LogicalTypeBuildingNotTitanGate
LogicalTypeBuildingsNotHouses
LogicalTypeBuildingsNotWalls
LogicalTypeBuildingsThatTrainMilitary
LogicalTypeHouses
LogicalTypeGates
LogicalTypeNeededForVictory
LogicalTypeValidSPCUnitsDeadCondition
LogicalTypeFimbulWinterTCEvalType
```

### Minimap filters
```
LogicalTypeMinimapFilterEconomic
LogicalTypeMinimapFilterMilitary
```

Esses tipos são a base das queries da IA (`kbUnitQuery*`) e do targeting de poderes.

---

## 32. Aprofundamento — API de Scripts de IA (XS)

O binário expõe **centenas de funções** para scripts XS da IA (prefixos `ai`, `kb`, `tr`, `xs`, `ui`). Total aproximado de assinaturas documentadas nas strings: **~530 funções ai/kb**.

### 32.1 Gerenciamento de Planos (`aiPlan*`)

```cpp
bool aiPlanAddUnit(int planID, int unitID);
bool aiPlanAddUnitType(int planID, int unitTypeID, int numberNeed, int numberWant, int numberMax);
bool aiPlanAddWaypoint(int planID, vector waypoint);
bool aiPlanDestroy(int planID);
bool aiPlanDestroyByName(string name);
bool aiPlanGetActive(int planID);
bool aiPlanGetAllowUnderAttackResponse(int planID);
bool aiPlanGetAttack(int planID);
// + variáveis de usuário (Bool/Float/Int/String/Vector)
bool aiPlanAddUserVariableBool/Float/Int/String/Vector(...);
```

### 32.2 Tasking de unidades (`aiTask*`)

```cpp
bool aiTaskUnitWork(int unitID, int targetUnitID);
bool aiTaskUnitSpecialPower(int unitID, ...);
// transform, move, attack, etc.
```

### 32.3 Poderes e tributo

```cpp
bool aiCastGodPowerAtPosition(int godPowerTechID, vector pos);
bool aiCastGodPowerAtUnit(int godPowerTechID, int unitID);
bool aiBuyResourceOnMarket(int resourceID);
bool aiTribute(int playerID, int resourceID, float amount);
bool aiSetGodPowerEventHandler(string handlerFunctionName);
```

### 32.4 Knowledge Base — Units / Armies / Bases

```cpp
// Armies
bool kbArmyAddUnit(integer armyID, integer unitID);
bool kbArmyRemoveUnit(...);
bool kbArmyDestroy(integer id);
float kbArmyGetCurrentHitpoints(integer armyID);
float kbArmyGetMaximumHitpoints(integer armyID);

// Bases
bool kbBaseAddUnit(int playerID, int baseID, int unitID);
bool kbBaseDestroy(int playerID, int baseID);
bool kbBaseGetActive/Economy/Forward/Main(...);

// Unit queries
bool kbUnitQueryResetData(long queryID);
bool kbUnitQueryResetResults(long queryID);
bool kbTargetSelectorAddQueryResults(int queryID);
```

### 32.5 Tech Tree / Resources / Areas

```cpp
int  kbTechTreeGetDropsiteUnitIDByResource(int resourceID, int index);
int  kbTechTreeGetNumberDropsiteUnitIDsByResource(int resourceID);
bool kbTechTreeAddDropsiteUnitIDByResource(...);
bool kbTechTreeClearDropsiteUnitIDsByResource(...);

int  kbAreaGetVisibilityChangeTime(int areaID);
bool kbAddAttackRouteIgnoreID/Type(...);
bool kbAddAttackRouteSector(int sector);
bool kbAttackRouteAddPath(...);
bool kbBuildingPlacementSetCenterPosition(vector position, float distance, float obstructionRadius);
```

### 32.6 Flags de comportamento da IA

```cpp
bool aiGetAllowAutoDropsites(void);
bool aiGetAllowBuildings(void);
bool aiGetAutoFavorGather(void);
bool aiGetAutosaveOn(void);
bool aiGetPauseAllAgeUpgrades(void);
bool aiIsMultiplayer(void);
bool aiCommsSetEventHandler(string handlerFunctionName);
```

### 32.7 Tempo e utilitários XS

```cpp
int xsGetTime(void);  // gametime em milissegundos
```

---

## 33. Aprofundamento — Constantes de Jogo (c*)

Constantes embutidas usadas pelos scripts e pelo motor:

### Modos de jogo
```
cGameModeConquest
cGameModeDeathmatch
cGameModeLightning
cGameModeSupremacy
```

### Dificuldade
```
cDifficultyEasy
cDifficultyModerate
cDifficultyHard
cDifficultyNightmare
```

### Tipos de área
```
cAreaTypeForest
cAreaTypeGold
cAreaTypeWater
cAreaTypeSettlement
cAreaTypeImpassableLand
cAreaGroupTypeLand
cAreaGroupTypeWater
cAreaGroupTypeImpassableLand
```

### Planos (templates)
```
cAttackPlan*
cBuildPlan*
cBuildWallPlan*
cDefendPlan*
cExplorePlan*
cFarmPlan*
cFishPlan*
cGatherPlan*
cGatherGoalPlan*
cGatherRelicPlan*
cGoalPlan*
cDropoffResourcePlan*
cEmpowerPlan*
cAttackStrategyPlan*
```

### Rotas de ataque
```
cAttackRouteFrontLeft / FrontRight
cAttackRouteBackLeft / BackRight
cAttackRouteLeftFront / LeftBack
cAttackRouteRightFront / RightBack
```

### Escrow / Recursos
```
cEconomyEscrowID = 1
cAllResources
cResource* (Food, Wood, Gold, Favor — gerados dinamicamente)
cAIResourceSubType*
```

### Upgrades
```
cUpgradeTypeAny
cUpgradeTypeArmor
cUpgradeTypeAttack
cUpgradeTypeHitpoints
cUpgradeTypeSpeed
cUpgradeTypeTrainPoints
cUpgradeTypeWorkRate
```

### Outros
```
cAge*          (idades)
cCiv*          (civilizações)
cCulture*
cAction* / cActionAny
cUnitType*     (gerados a partir do proto)
cBuildingPlacementEventDone / Failed
cBuildingPlacementPreference*
cConnectAllies / Enemies / Players / Areas
cAICommPrompt* / Record* / Response*
```

---

## 34. Aprofundamento — Atributos de Proto / Unidade

Atributos de unidades referenciados no binário (lidos de `proto.xmb` / `protox.xmb`):

### Vida e destruição
```
Hitpoints
MaxHitpoints
InitialHitpoints
DieAtZeroHitpoints
DoNotDieAtZeroHitpoints
ConvertToGaiaAtZeroHitpoints
MakeUnbuiltAtZeroHitpoints
RevertToSocketAtZeroHitpoints
HideHitpointsIfGaia
```

### Movimento e visão
```
MaxVelocity
LineOfSight / LOS
LOSModifier
GivesLOSToAll
CanBuildLOSProto
```

### Produção e custo
```
TrainPoints
BuildPoints
PopulationCount
CostFood / CostWood / CostGold / CostFavor
WorkRate
```

### Combate
```
Armor / ArmorNormal / ArmorPierce / ArmorVulnerability
DamageBonus / RandomizedDamageBonus
AttackPierce / AttackCrush
```

### Flags de comportamento
```
ObstructionRadius / ObstructionRadiusX / ObstructionRadiusZ
DontRotateObstruction
PlayerOwnsObstruction
```

Variáveis de runtime para UI/debug:
```
currentUnitArmor / currentUnitArmorFloat / currentUnitArmorList
currentUnitBuildPoints / currentUnitTrainPoints
currentUnitMaxVelocity
experienceMaxDamageBonus
```

---

## 35. Aprofundamento — Pathfinding e Obstruction

### Conceitos
```
ObstructionManager
ObstructionRadius / ObstructionRadiusX / ObstructionRadiusZ
DontRotateObstruction
PlayerOwnsObstruction
SelectWithObstruction
UseObstructionOnMinimap
UseAlignedObstructionOnMinimap
RETIREDObstructionSize
```

### Pathing
```
Pathing
Pathing Long
Pathing Short
pathingSync
renderPathingQuad
renderObstructionManager
Built pathing quads in %d ms.
Build connection failed: could not init the pather
AdjustPositionOnTerrainCollision
AlwaysCheckCollisions
```

### Building Placement
```
bool kbBuildingPlacementSetCenterPosition(vector position, float distance, float obstructionRadius);
cBuildingPlacementEventDone
cBuildingPlacementEventFailed
BPInfluenceDistance
BuildingPlacementDistance
```

---

## 36. Aprofundamento — Simulação e Tempo

```
Creating simulation mode
D:\rts3\amodesimulation.cpp
%s: gametime=%d.
gametime
int xsGetTime(void);           // milissegundos
int kbAreaGetVisibilityChangeTime(int areaID);
GetTickCount
gridMajorTick / gridMinorTick
```

O loop de simulação roda em modo próprio (`amodesimulation`), separado do modo pré-jogo (`amodepregame`).

---

## 37. Resumo Quantitativo da Análise Estática

| Categoria | Quantidade aproximada |
|-----------|----------------------|
| Strings totais extraídas | ~25.800 |
| Arquivos-fonte identificados (debug paths) | ~150+ |
| Logical Types | 84 |
| Funções de API ai/kb documentadas nas strings | ~530 |
| Poderes com implementação própria | ~30 |
| Ações de unidade | ~18 |
| Tipos abstratos | 21 |
| Arquivos XML de UI | ~50+ |
| Archives BAR (base + expansão) | 16 |
| Relíquias nomeadas | 27+ |
| Constantes c* de jogo | dezenas (modos, planos, áreas, upgrades...) |
| Deuses (maiores + menores) | 12 maiores + 36 menores (4 civs × 3 idades) |

---

## 38. Como Usar Este Documento no Port Nativo

1. **Parsers** — Seções 4, 13 e 34 validam o que `BarArchive` e `GameDataManager` devem carregar e quais atributos de proto importam.
2. **Gameplay MVP** — Seções 5, 14, 17, 19 e 35 guiam move/gather/attack, tipos abstratos, dano e pathfinding.
3. **Vitória e economia** — Seções 15 e 5.1 + constantes `cGameMode*` e recursos.
4. **IA futura** — Seções 12, 31, 32 e 33 são o mapa da API XS e dos Logical Types.
5. **Poderes** — Seção 11 + Logical Types de targeting (31).
6. **UI touch** — Seção 20 lista todos os painéis/diálogos que o original tem (referência de feature parity).
7. **Priorização** — Seção 16 e 30.

---

*Seções 31–38 adicionadas em 2026-11-05. Documento considerado completo para análise estática do binário aomxnocd.exe.*

---

## 39. Aprofundamento Técnico — Imports por DLL

### 39.1 aomac.dll
```
ShellExecuteA
```
Única exportação importada explicitamente pelo EXE; a maior parte da lógica do jogo está em `aomac.dll` (carregada e usada via GetProcAddress / link implícito adicional).

### 39.2 KERNEL32.dll (amostra das principais)
```
CreateFileA, ReadFile, WriteFile, DeleteFileA, MoveFileA
FindFirstFileA, FindNextFileA, CreateDirectoryA
CreateEventA, WaitForSingleObject, WaitForMultipleObjects, Sleep, SleepEx
CreateThread-related, TerminateThread, GetCurrentThread, GetCurrentProcess
HeapAlloc, LocalAlloc, LocalFree, GlobalMemoryStatus, GlobalReAlloc
LoadLibraryA, FreeLibrary, GetModuleHandleA, GetModuleFileNameA
InterlockedIncrement/Decrement/Exchange
GetSystemTime, SystemTimeToTzSpecificLocalTime
IsBadReadPtr, IsBadWritePtr, DebugBreak
GetSystemDirectoryA, GetWindowsDirectoryA
```

### 39.3 USER32.dll
```
GetAsyncKeyState, GetKeyState, MapVirtualKeyA, GetKeyboardLayout
CreateWindow/ShowWindow/DestroyWindow/SetWindowPos/MoveWindow
SendMessageA, TranslateMessage, PostQuitMessage
SetCapture, ReleaseCapture, SetCursor, DestroyCursor
BeginPaint, EndPaint, RedrawWindow, UpdateWindow
ClientToScreen, ScreenToClient, GetWindowRect, GetSystemMetrics
GetFocus, SetFocus, IsWindow, IsIconic, IsWindowVisible
```

### 39.4 GDI32.dll
```
BitBlt, StretchBlt, PatBlt
CreateCompatibleDC, CreateDIBitmap, CreateDIBSection, CreateBitmap
SelectObject, DeleteObject, DeleteDC
CreateSolidBrush, CreatePalette, SelectPalette, RealizePalette
SetBkColor, SetBkMode, SetTextColor, SetTextAlign
GetDeviceCaps, GetGlyphOutlineA, GetStockObject
```
Uso típico: UI 2D, texto, minimapa auxiliar, screen capture — não o render 3D principal (Direct3D).

### 39.5 granny.dll — API completa importada
```
_GrannyOpenVersion@20 / _GrannyClose@4
_GrannySetFilesytem@20
_GrannyAllowTextureFormat@8 / _GrannyDisallowTextureFormat@8

_GrannyOpenModel@12 / _GrannyCloseModel@8 / _GrannyTransformModel@16
_GrannyOpenAnimation@12 / _GrannyCloseAnimation@8
_GrannyOpenSequence@12 / _GrannyCloseSequence@8
_GrannyAddControlledAnimation@20 / _GrannyRemoveControlledAnimation@12
_GrannyTransformAnimation@16
_GrannyGetAnimationParameter@20 / _GrannySetAnimationParameter@24
_GrannyAdvanceTime@12 / _GrannyGetCurrentTime@8

_GrannyGetBoneCount@12 / _GrannyGetBoneState@20
_GrannyLockSequenceForRendering@16
_GrannyLockNextRenderingState@12 / _GrannyUnlockRenderingState@4
_GrannyGetRenderingStatesLeft@12 / _GrannyUnlockRendering@8
_GrannyLockNextNewTexture@8 / _GrannyUnlockNewTexture@8
_GrannyGetNewTexturesLeft@8
```
Conclusão: o motor usa Granny para **modelos, animações controladas, bones, sequences e texturas de animação**. O port nativo precisa reimplementar ou substituir esse pipeline (ex.: carregar BRG/GRN → meshes GLES + clips de animação).

### 39.6 mss32.dll (Miles Sound System)
```
_AIL_shutdown@0
RIB_load_provider_library
RIB_request_interface / RIB_request_interface_entry
_RIB_find_files_provider@20
```
Áudio via Miles + RIB (provider plugins). O EXE também usa WINMM (`mmioRead`, etc.).

### 39.7 RockallDLL.dll
```
??0FAST_HEAP@@QAE@H_N00@Z   // FAST_HEAP constructor
??1FAST_HEAP@@UAE@XZ        // destructor
```
Alocador de memória customizado (MicroQuill Rockall) — comum em jogos Ensemble da época.

### 39.8 Outros
- **WS2_32**: sockets (WSASocketA e família)
- **ADVAPI32**: registro + security descriptors (SafeDisc residual / Zone)
- **ole32 / OLEAUT32 / RPCRT4**: COM, GUID, UUID
- **IMM32**: IME (internacionalização de input)
- **VERSION**: GetFileVersionInfo*

---

## 40. Aprofundamento — Modos de Aplicação (Game Modes)

O EXE cria explicitamente os seguintes modos/submodos:

```
Creating pregame mode
Creating simulation mode
Creating campaign mode
Creating single player setup mode
Creating single player submode
Creating editor submode
Creating scenario test submode
Creating Arena mode
Creating RM setup mode
Creating mp session mode
Creating mp session menu submode
Creating mp session quick team submode
Creating postgame submode
Creating record game submode
Creating cinematic submode
Creating cinematic build submode
Creating broadcast submode
Creating receive submode
Creating profileselect mode
Creating game menu submode
Creating options sub mode for pre game ui
Creating options sub mode for simulation
Creating HotKey Setup sub mode for pre game ui
Creating HotKey Setup sub mode for simulation
```

Arquitetura típica Ensemble:
- **Pregame** — menus, setup, seleção de mapa/deus
- **Simulation** — partida em andamento (`amodesimulation.cpp`)
- **Campaign / Editor / Arena / MP Session** — modos especializados
- **Record Game** — playback de partidas gravadas
- **Cinematic** — cutscenes

---

## 41. Aprofundamento — Stack de Rede

### Classes
```
BCommManager          — gerenciador central de comunicações
BConnection           — conexão individual (send/sendTo)
BSocksProtocol        — resolução DNS / protocolo socks-like
BSocksReliableListener — accept de clientes (FD_ACCEPT)
BWinsockPeerConnection — host/connect UDP peer-to-peer
BGameStream           — stream de updates sincronizados
```

### Fluxo de conexão (inferido)
1. `BCommManager::startup` — inicializa protocolo
2. Host: `BWinsockPeerConnection::host` cria socket UDP
3. Client: `connect` + listen no comm manager
4. `BSocksReliableListener` recebe `FD_ACCEPT` e notifica observer
5. Durante o jogo: `BGameStream` serializa updates (tempo, comandos, seleção, câmera, sync)

### Conteúdo de um update (`BGameStream`)
```
addUpdateTime / getUpdateTime
addCommand / getCommand (+ amount)
addSelectedUnit / getSelectedUnit (+ amount)
addCamera / getCamera
addSync / getSync
inletBeginUpdate / inletEndUpdate
outletBeginUpdate / outletEndUpdate
```

Arquivo de log de sync: `RTS3synclog.txt`  
Flags: `BallisticSync`, `GodPowersSync`, `ObMgrSync`, `pathingSync`, `FinalReleaseSync`

---

## 42. Aprofundamento — Record Game (Replay)

### UI
```
RecordGameCheckBox / RecordGameCheckBox-Disabled
RecordGameCtrl
  - Play / Pause / PlayFast
  - ProgressBar
  - Bookmarks (Add / Next / Prev)
  - FogCheck / LoopCheck / ViewLockCheck
  - Player selector
SinglePlayerSetup-RecordGame
uiOpenRecordGameBrowser
enterRecordGameMode(<name>)
```

### Runtime
```
BRecordGame::startUpdatePlayback
  — depende de mGameStream file markers
  — suporte a buffering
```

O replay é **deterministic lockstep** baseado no mesmo `BGameStream` do multiplayer (comandos + sync checksums), não gravação de vídeo.

---

## 43. Aprofundamento — Pipeline Direct3D / Texturas

### Classes
```
BD3DRenderer   — renderer principal + screenCapture
BD3DTexture    — carga e update de texturas
```

### Formatos de textura
- **DDT** — formato proprietário (DXT / RGB / RGBA)
- **BDT** — variante/nível mip (mensagens `load bdt`)
- Downsample de terreno (`updateTerrain`, `mspTerrainMipMap`)
- Exigência de 16-bit em alguns caminhos de copy/update

### Erros típicos
```
BD3DTexture::load ddt: failed to read level size info
BD3DTexture::loadddt: failed to decomrpess to surface
BD3DTexture::updateTerrain: failed to downsample surface
BD3DTexture::updateTerrain: mspTerrainMipMap missing
Buffer format not supported by BD3DRenderer::screenCapture
```

### Blend ops referenciados
```
D3DBLENDOP_ADD / SUBTRACT / REVSUBTRACT / MIN / MAX
D3DBLEND_ONE, SRCALPHA, INVSRCALPHA, DESTCOLOR, etc.
```

No port: mapear DDT → texturas GLES comprimidas (ETC2/ASTC) ou RGBA, e reimplementar o mipmap de terreno.

---

## 44. Aprofundamento — Granny Animation Pipeline (para o port)

Fluxo típico inferido das imports:

1. `GrannyOpenVersion` / `GrannySetFilesytem`
2. `GrannyOpenModel` → malha + skeleton
3. `GrannyOpenAnimation` / `GrannyOpenSequence`
4. `GrannyAddControlledAnimation` em um modelo
5. Loop:
   - `GrannyAdvanceTime`
   - `GrannyTransformAnimation` / `GrannyTransformModel`
   - `GrannyGetBoneState` / `GrannyGetBoneCount`
   - `GrannyLockSequenceForRendering` → `LockNextRenderingState` → draw → unlock
6. Texturas de animação: `LockNextNewTexture` / `UnlockNewTexture`
7. `GrannyClose*` ao descarregar

Arquivos de dados associados:
- `.grn` / `.brg` — modelos e clips
- `*_anim.txt` — scripts de animação (gerados pelo “bang animation tool”)
- Low-poly: `%slow\LP %s.grn`

**Implicação para AoM-Native-Port:** sem reimplementar um subset do Granny (ou converter assets offline para um formato simples de skeleton+clips), as unidades continuarão estáticas.

---

## 45. Mapa de Dependências Externas do EXE

```
aomxnocd.exe
 ├── aomac.dll          (core do jogo — obrigatória)
 ├── granny.dll         (animações 3D — obrigatória)
 ├── mss32.dll          (áudio Miles — obrigatória)
 ├── RockallDLL.dll     (heap custom — obrigatória)
 ├── d3d8/d3d9 (sys)    (via dynamic / DirectX runtime)
 ├── WS2_32, WINMM, USER32, GDI32, KERNEL32, ...
 └── dados em disco
      ├── *.bar (data, textures, models, anim, sounds, scenarios, godpowers + *2)
      ├── proto.xmb / protox.xmb / techtree*.xmb
      ├── campanhas .cpn / .cpx
      └── UI XML, language DLLs, gfxconfig
```

O crack NoCD remove apenas a checagem de mídia; todas as DLLs e dados acima continuam necessários para o binário original rodar.

---

*Seções 39–45 adicionadas em 2026-11-05 — análise técnica de imports, modos, rede, replay, D3D e Granny.*

---

## 46. Ferramentas e Utilitários para Análise e Modding

Esta seção reúne ferramentas da comunidade úteis para inspecionar, converter e editar os mesmos formatos que o `aomxnocd.exe` consome. São especialmente relevantes para o projeto **AoM-Native-Port**, que precisa ler `.bar`, `.xmb`, `.ddt`, `.brg` e `.grn` sem depender do binário original.

### 46.1 Repositório principal — ptasev/Age-of-Mythology

- **URL:** https://github.com/ptasev/Age-of-Mythology  
- **Descrição:** Libraries and apps for interacting with files for the Age of Mythology game.  
- **Solução:** `AoMEngineLibrary.sln` (C# / .NET)  
- **Releases / builds:** https://petar.page/l/modding  

Projetos incluídos na solution (entre outros):

| Projeto | Função |
|---------|--------|
| **AoMEngineLibrary** | Biblioteca C# central para ler/escrever formatos binários do motor |
| **AoMModelEditor** | Editor visual de modelos (BRG/GRN) — ver §46.2 |
| **AoMBrgEditor** | Utilitário focado em BRG |
| **AoMDdtConverter** | Conversão de texturas DDT |
| **AoMProtoEditor** | Editor de definições proto/protox — ver §46.5 |
| **AoMTextEditor** | Edição de arquivos de texto/dados do jogo |
| **AoMEngineLibrary.Graphics.Converters** | Conversores gráficos (ex.: para glTF) |
| **AoMEngineLibrary.Tests** | Testes da biblioteca |

A biblioteca expõe leitores/escritores para estruturas internas (ex.: `GrnBinaryReader`, nós de mesh/vértices GRN, materiais BRG, etc.), o que a torna a referência de implementação mais próxima de “código aberto” para os formatos que o EXE carrega via Granny e archives.

### 46.2 AoM Model Editor (AME)

- **Autor:** Petar (ptasev / p2t5r)  
- **Disponível em:** https://p2t5r.itch.io/aom-model-editor e builds ligados ao repositório acima  
- **Função:** Abrir, visualizar, converter e editar modelos 3D originais do Age of Mythology e da Extended Edition.

**Formatos:**

| Formato | Tipo de animação | Uso típico |
|--------|------------------|------------|
| **`.brg`** | Vertex morph (keyframes de vértices) | Muitas unidades/efeitos do jogo base |
| **`.grn`** | Skeleton / bones (Granny) | Modelos com esqueleto; animações em arquivos separados (ex.: `Base.grn` + `Base_attacka.grn`) |

**Recursos principais:**

- Exportar BRG/GRN → **glTF** (interoperável com Blender, 3ds Max, etc.)
- Importar glTF → BRG ou GRN (com diálogos de settings)
- Edição de materiais, attachpoints (`Dummy_*`), frames e flags de mesh
- Estatísticas do modelo (verts, faces, dummies, frames, materiais, duração da animação)
- Aplicar animação GRN sobre um modelo base

**Detalhes BRG:**

- Nós glTF com nome começando em `Dummy_` viram attachpoints
- Nomes de textura vêm do filename da textura, do nome da textura ou do material
- Sample rate no import define quantos frames de morph são amostrados por segundo de animação

**Detalhes GRN:**

- Um arquivo pode guardar só mesh+skeleton ou skeleton+animation
- Todo mesh exportável precisa de skeleton com pelo menos um bone
- No pipeline do jogo, animações costumam ser clips GRN separados referenciados pelos scripts `*_anim.txt`

Para o **AoM-Native-Port:** o AME (ou a AoMEngineLibrary por baixo) é o caminho prático para **inspecionar e converter** assets oficiais para um formato que o renderer GLES consiga animar (glTF → meshes + skeleton + clips), em vez de reimplementar as 29 funções Granny do EXE de imediato.

### 46.3 Integração com modeladores 3D (Blender / 3ds Max)

#### Plugin / scripts para 3ds Max (AoM Model Plugin)

- **URL:** https://p2t5r.itch.io/aom-model-plugin  
- **Instalação típica:** extrair no diretório do 3ds Max e executar `RunAMP.ms` via MaxScript → Run Script  

**Recursos:**

- Attachpoints: adicionar, remover e editar direto na viewport
- Frames customizados e duração livre
- Materiais com flags especiais do motor AoM
- Import/export BRG e GRN a partir de malhas e bones do Max  
- BRG: Editable_Meshes visíveis = meshes; Dummies = attachpoints; texturas pelo filename  
- GRN: meshes com Skin modifier e pelo menos um bone; bones podem ser Editable_Mesh, Dummy, Box, BoneGeometry, Biped, Editable_Poly; objetos hidden não exportam  

#### Blender e glTF

O fluxo recomendado pela ferramenta oficial do Petar é:

1. Exportar o modelo do jogo (BRG/GRN) para **glTF** no AME  
2. Editar no **Blender** (ou outro editor glTF)  
3. Reimportar glTF → BRG ou GRN no AME com os settings adequados  

Assim não é obrigatório um exportador nativo Blender→BRG; o glTF funciona como formato ponte. Para o port nativo, o mesmo glTF pode alimentar diretamente o pipeline GLES (ou ser convertido offline para um formato binário simples de runtime).

#### ABE — AoM Brg Editor (legado / 3ds Max)

Citado na comunidade HeavenGames como “AoM Brg Editor” (Tasev): edição de BRG a partir da UI do 3ds Max. Complementar ao plugin mais novo; ainda útil em workflows antigos de modding.

### 46.4 AoM Engine Library (C#)

Núcleo do repositório **ptasev/Age-of-Mythology**:

- Leitura/escrita de **texturas** (incluindo caminhos relacionados a DDT conforme os conversores do solution)
- Leitura/escrita de **modelos** BRG e GRN (estrutura de nós, vértices, weights, materiais, animações)
- Suporte a dados de **proto** e demais arquivos binários/texto do motor
- API orientada a objetos (ex.: `GrnBinaryReader`, nós `GrnMeshVerticesNode`, etc.)
- Testes automatizados em `AoMEngineLibrary.Tests`

**Uso sugerido no contexto do port:**

1. Como **especificação viva** dos formatos (melhor que engenharia reversa só a partir do EXE)  
2. Para escrever **ferramentas offline** em C# ou para comparar o output do parser C++ do AoM-Native-Port  
3. Para pipelines de conversão em batch (BAR extract → BRG/GRN → glTF → assets do APK)

Não substitui o runtime C++20 do port; acelera o entendimento e a validação dos assets.

### 46.5 Proto Editor (AoMProtoEditor e afins)

- **AoMProtoEditor** — projeto dentro do solution `ptasev/Age-of-Mythology` (`src/AoMProtoEditor`)  
- Foco: edição de arquivos de **definição de unidades/facções** (`proto` / `protox`, em geral após conversão XMB↔XML)

**O que o proto controla (resumo):**

- Lista de todas as unidades do jogo (base + expansão em `protox`)
- Atributos: hitpoints, velocidades, LOS, custos (Food/Wood/Gold/Favor), train/build points, population, armadura, bônus de dano, flags de comportamento
- Ligações com animações, sons, ações e tipos abstratos / logical types

**Fluxo clássico da comunidade (HeavenGames / AoMed):**

1. Extrair `proto.xmb` / `protox.xmb` do `data.bar` / `data2.bar`  
2. Converter XMB → XML (AoMed, conversores da Engine Library, etc.)  
3. Editar XML (ou via Proto Editor com UI)  
4. Reconverter XML → XMB e reempacotar no BAR se necessário  

Outras ferramentas históricas:

- **AoMed / AOM Edit (Ykkrosh)** — conversor clássico XMB↔XML, BRG, decompressão de `.scn`, explorador de BAR  
- **Bar Explorer (Vachu)** — navegação/extração rápida de `.bar`  
- **Pavinati Protox.XML Editor** — UI antiga focada em protox.xml (flags, unit types, adicionar unidade)

Para o **AoM-Native-Port:** o `GameDataManager` que já lê `proto.xmb` + `protox.xmb` com deduplicação deve permanecer alinhado aos mesmos campos que essas ferramentas expõem (custos, HP, population, etc.), para que dados editados pela comunidade continuem carregáveis.

### 46.6 Outras ferramentas de comunidade (referência)

| Ferramenta | Uso |
|------------|-----|
| **AoMed / AOM Data File Converter** | XMB↔XML, BRG, SCN, exploração de BAR |
| **Bar Explorer** | Extrair/listar conteúdo de `.bar` |
| **AOM Lists (Reyk)** | Listas organizadas de animações, sons, objetos, ícones |
| **Voobly + compatibility patch** | Execução online do EXE original em PCs modernos |
| **Documentação HeavenGames** | Guias de proto, techtree, scenario design, mod resources |

### 46.7 Como encaixar no AoM-Native-Port

| Necessidade do port | Ferramenta recomendada |
|---------------------|------------------------|
| Entender layout BRG/GRN | AoMEngineLibrary + AME |
| Converter modelos para pipeline GLES | AME → glTF → import no asset pipeline do port |
| Validar/editar stats de unidades | Proto Editor / XML proto + comparação com `GameDataManager` |
| Extrair assets de `.bar` | Bar Explorer / AoMed / tools Python do próprio port (`tools/`) |
| Texturas DDT | AoMDdtConverter + decoder DDT do port |
| Referência de formatos “oficiosa” | Código-fonte e testes do ptasev/Age-of-Mythology |

Nenhuma dessas ferramentas redistribui assets comerciais do jogo: todas assumem que o usuário possui uma cópia legítima e trabalha sobre os arquivos da instalação local — o mesmo princípio do importador SAF do AoM-Native-Port.

---

## 47. Destaques Úteis para o Port (versão expandida)

1. **Granny / animações** — O EXE importa 29 funções Granny. Sem um subset runtime ou conversão offline `.brg`/`.grn` → glTF (AME + AoMEngineLibrary), as unidades permanecem estáticas. Priorizar idle/walk/attack no MVP.

2. **BGameStream** — Formato de sync/replay (tempo, comandos, unidades selecionadas, câmera, checksums). Útil se no futuro houver gravação de partidas ou netcode lockstep; não é bloqueante para o MVP single-player.

3. **Modos pregame vs simulation** — O binário separa explicitamente `Creating pregame mode` e `Creating simulation mode`. O engine nativo já espelha essa divisão (menu vs partida); manter a carga pesada de missão **fora** da thread de UI evita o ANR documentado no PROGRESSO.md.

4. **DDT / BDT e terreno** — Mensagens de falha do `BD3DTexture` e o mipmap de terreno (`updateTerrain`, `mspTerrainMipMap`) servem de checklist para o decoder de texturas e o terreno procedural do port.

5. **Overlay Titans** — Comportamento oficial confirmado: `*2.bar` e `protox`/`techtreex` por cima do base. O `BarArchive` multi-mount do port está alinhado; validar sempre com instalação Extended Edition real.

6. **Ferramentas externas** — Usar **ptasev/Age-of-Mythology**, **AME** e **Proto Editor** como fonte de verdade dos formatos e para pipelines de conversão, em vez de depender só de strings do `aomxnocd.exe`.

---

*Seções 46–47 adicionadas em 2026-11-05: Visão Geral ampliada (Titans + NoCD), ferramentas de modding/análise e destaques expandidos para o AoM-Native-Port.*
