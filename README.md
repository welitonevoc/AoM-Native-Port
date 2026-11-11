# AoM Native Port (Port Nativo para Android)

Um protótipo independente de engine RTS nativa em **C++20** para **Android**, inspirado no clássico *Age of Mythology*. Este é um projeto não oficial, sem fins lucrativos, desenvolvido para fins educacionais e de preservação técnica, sem vínculo ou endosso da Microsoft ou World's Edge.

---

## 📋 Visão Geral

O objetivo deste projeto é construir uma engine de estratégia em tempo real (RTS) nativa, leve e de alto desempenho para arquiteturas ARM64 no Android, capaz de interpretar os formatos originais de dados e oferecer jogabilidade fluida a 60 FPS com renderização moderna em OpenGL ES 3.

### Principais Características
- **C++20 Puro no Core:** Arquitetura desacoplada, alta performance e zero sobrecarga de interpretadores.
- **Renderização Gráfica em GLES 3.x:** Suporte a shaders modernos, mapeamento de textura com mipmaps e gerenciamento eficiente de VBO/VAO.
- **Parsers de Formatos Originais:**
  - **BAR:** Leitor e extrator de arquivos compactados de recursos (`data.bar`, `textures.bar`).
  - **DDT:** Decodificador de texturas com suporte a compactação DXT/RGB/RGBA.
  - **BRG:** Leitor de geometria 3D e malhas de unidades/edifícios.
  - **XMB:** Parser de arquivos binários de dados e propriedades (`proto.xmb`).
- **Sistema de UI Adaptado para Touch:** Interface de usuário construída para telas sensíveis ao toque com detecção de toques simples, arrasto, pinça para zoom (pinch-to-zoom) e seleção de tropas.
- **Áudio Nativo:** Reprodução de efeitos sonoros (PCM WAV) e músicas de fundo.
- **Reprodução de Vídeo via NDK:** Decodificação acelerada por hardware de vídeos (H.264 / MP4) usando `AMediaExtractor` e `AMediaCodec`.

---

## 🎯 Escopo do MVP (Produto Mínimo Viável)

O projeto atual foca em um ciclo jogável rápido e estável com os seguintes elementos:

- **1 Facção Jogável:** Gregos (Hoplite, Toxotes, Villager).
- **1 Unidade Mítica:** Minotauro.
- **Economia Básica:** Coleta de Comida (Food), Madeira (Wood) e Ouro (Gold).
- **Construção:** Town Center, Casas (House), Quartel (Barracks) e Torres.
- **Combate e Pathfinding:** Algoritmo A* para navegação no terreno e sistema de combate corpo-a-corpo e à distância.
- **Mapa Procedural:** Terreno com elevação e texturização dinâmica.
- **Interface & HUD:** Painel de recursos, status de unidades e menu de ações.

Para mais detalhes sobre o planejamento, consulte [`docs/MVP.md`](docs/MVP.md) e [`docs/ANALISE_COMPLETA.md`](docs/ANALISE_COMPLETA.md).

---

## 📂 Estrutura do Repositório

```text
├── android/            # Projeto Android (Gradle, Manifest, NDK bridge)
├── assets/             # Assets essenciais de interface, fontes e testes
├── docs/               # Documentação técnica detalhada e especificações do MVP
├── include/            # Cabeçalhos C++ organizados por subsistemas
│   ├── campaign/       # Sistema de missões
│   ├── engine/         # Ciclo de vida, renderizador e loop principal
│   ├── network/        # Infraestrutura de rede (futuro)
│   └── video/          # Decodificador de mídia NDK
├── scripts/            # Scripts utilitários de build e extração
├── src/                # Implementação em C++ dos subsistemas
├── third_party/        # Dependências embutidas (GLM, stb_image, stb_truetype, NDK headers)
├── tools/              # Ferramentas Python para manipulação de BAR, DDT, BRG e XMB
├── CMakeLists.txt      # Configuração principal de compilação CMake
└── PROGRESSO.md        # Registro histórico e checklist de desenvolvimento
```

> [!NOTE]
> Este repositório contém o código-fonte da engine, ferramentas e scripts de compilação. Ele **não** redistribui os arquivos proprietários completos do jogo original.

---

## 🛠️ Como Compilar e Executar

### Pré-requisitos
- **Arquitetura Alvo:** Android `arm64-v8a` (Android 8.0+ / API 24+).
- **Ferramental:** Android NDK (r25 ou superior), CMake 3.22+, JDK 17+.

---

### Opção 1: Compilação via Android Studio / Linha de Comando (Gradle)

1. Clone o repositório:
   ```bash
   git clone https://github.com/welitonevoc/AoM-Native-Port.git
   cd AoM-Native-Port
   ```
2. Gere o APK de desenvolvimento:
   ```bash
   ./gradlew :app:assembleDebug
   ```
3. O APK gerado estará em:
   ```text
   android/app/build/outputs/apk/debug/app-debug.apk
   ```

---

### Opção 2: Compilação Nativa Direta no Dispositivo (Termux / Antigravity)

Caso esteja compilando diretamente em um ambiente Linux/Android local com ferramentas nativas instaladas:

```bash
# Compilar a biblioteca C++ compartilhada (.so)
bash scripts/build_native.sh

# Empacotar e assinar o APK
bash scripts/package_apk.sh
```

---

## 🎮 Importando Dados e Recursos do Jogo

O APK não inclui os arquivos proprietários de *Age of Mythology*. Para usar
texturas, modelos, sons, campanhas e dados de balanceamento originais, importe
a pasta do jogo da sua cópia legal.

### Opção recomendada: importar pelo próprio app

1. Instale o APK.
2. Abra o app e toque em **OPÇÕES**.
3. Toque em **IMPORTAR DADOS DO JOGO**.
4. Selecione a pasta que contém as subpastas `data` e `textures`
   (normalmente a pasta `Age of Mythology`).
5. Aguarde a cópia e o reinício automático.

O importador usa o seletor de pastas do Android (SAF), copia os arquivos para
`Android/data/com.aom.nativeport/files/AoM/` em segundo plano e **não exige**
permissão de acesso a todos os arquivos do dispositivo.

**Como funciona internamente:**

1. A engine nativa chama `requestGameDataImport()` no Java via JNI.
2. O Java abre o seletor SAF e recebe permissão persistente de leitura.
3. A pasta é copiada recursivamente na thread `aom-data-import`, sem travar a UI.
4. Ao terminar, a Activity é recriada e a engine localiza os `.bar` na pasta
   privada do app.

### Opção manual

Copie a estrutura de pastas do jogo (não apenas alguns `.bar` isolados) para
uma das pastas abaixo:

- `/storage/emulated/0/Download/AoM/`
- `/storage/emulated/0/Documents/AoM/`
- `Android/data/com.aom.nativeport/files/AoM/` (via USB/ADB)

Estrutura esperada:

O **mínimo obrigatório** para a engine iniciar é:

```text
AoM/
├── data/data.bar
└── textures/textures.bar
```

O conjunto **recomendado** (e já carregado pelo runtime atual) é:

```text
AoM/
├── data/
│   ├── data.bar
│   └── main campaign.cpn        # campanha base
├── textures/textures.bar
├── models/models.bar            # malhas 3D
├── scenario/scenarios.bar       # cenários
└── sound/sounds.bar             # áudio
```

Se você tem **The Titans**, copie também os arquivos `*2.bar`:
`data2.bar`, `textures2.bar`, `models2.bar`, `scenarios2.bar`,
`sounds2.bar`, `anim2.bar` e `godpowers2.bar`. A engine **monta a expansão
por cima do jogo base** (overlay): os arquivos com o mesmo nome são
sobrescritos pela versão da expansão e os arquivos exclusivos dela
(`protox.xmb`, `techtreex.xmb`, `xpack campaign.cpx`, …) são adicionados.

A instalação original também contém pastas de apoio que o app **ainda não
consome**: `fonts/`, `gfxconfig/`, `gfxconfig2/`, `startup/`, `ai/`,
`ai2/`, `rm/`, `rm2/`, `avi/` e os arquivos de idioma
(`language.dll`, `xpacklanguage.dll`, pastas LCID). Copiá-las não quebra nada;
elas serão usadas nas próximas etapas. **Não copie** `savegame/`,
`screenshots/`, `Voobly Mods/` nem `_CommonRedist/`.

> Nesta instalação do AoM Extended Edition não existem as pastas `campaign/`,
> `gfx/` nem `screenshots/`. As campanhas ficam em `data/main campaign.cpn` e
> `data/xpack campaign.cpx`; os perfis gráficos ficam em `gfxconfig/` e
> `gfxconfig2/`; `screenshots/` é criada pelo jogo original ao tirar uma
> captura.

A engine procura, nesta ordem: pasta privada do app, `Documents/AoM`,
`Download/AoM`, `AoM` na raiz do armazenamento e pastas legadas. A primeira
pasta que contiver `data/data.bar`, `textures/textures.bar`,
`models/models.bar`, `data.bar` ou `textures.bar` é usada. Também é aceita uma
subpasta de primeiro nível, como `Download/AoM/Age of Mythology/...`.

Não é necessário extrair os `.bar`: eles são lidos diretamente. No Android 11+,
se o acesso ao armazenamento compartilhado for negado, use o importador interno
ou `adb push` para a pasta privada do app.

**Problemas comuns:**

- **“Pasta inválida”** — selecione a pasta que contém `data/data.bar` e
  `textures/textures.bar`, não uma pasta acima nem um arquivo isolado.
- **Texturas não aparecem** — confirme que `textures/textures.bar` foi copiado
  e não está corrompido.
- **`Download/AoM` não é detectado no Android 11+** — use a importação interna
  ou `adb push`; o acesso direto ao armazenamento compartilhado pode estar
  bloqueado.

📖 **Instruções completas, comandos ADB, árvore de arquivos, verificação por
logcat e solução de problemas:** [`docs/IMPORTACAO_DADOS.md`](docs/IMPORTACAO_DADOS.md).

---

## 📊 Status do Projeto

Consulte o arquivo [`PROGRESSO.md`](PROGRESSO.md) para ver o histórico detalhado de commits, tarefas concluídas e os próximos passos da implementação.

---

## ⚖️ Licença e Direitos Autorais

- *Age of Mythology* e seus recursos visuais/sonoros são marcas registradas e propriedades intelectuais da Microsoft Corporation / World's Edge.
- O código-fonte deste projeto é disponibilizado exclusivamente para fins de estudo, engenharia reversa para interoperabilidade e preservação histórica de software.
- Bibliotecas de terceiros localizadas em `third_party/` (como GLM e STB) mantêm suas respectivas licenças originais.
