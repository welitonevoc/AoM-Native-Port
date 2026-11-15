# AoM Native Port (Port Nativo para Android)

Um porte e engine RTS nativa em **C++20** para **Android**, inspirado no clássico *Age of Mythology* e em sua expansão *The Titans*. Este é um projeto independente, sem fins lucrativos, desenvolvido exclusivamente para fins de pesquisa, estudo e preservação técnica de formatos legados.

---

## 📋 Visão Geral do Projeto

O objetivo do projeto é construir uma engine de estratégia em tempo real (RTS) nativa, de alto desempenho e baixo consumo de recursos para dispositivos Android (ARM64 e x86_64), capaz de interpretar diretamente os formatos de dados originais do jogo de PC e rodar com taxa estável de **60 FPS** usando **OpenGL ES 3.0**.

### ⚡ Principais Destaques Técnicos

- **C++20 Nativo de Alta Performance:** Sem interpretadores ou wrappers pesados. Ciclo principal gerenciado via `android_native_app_glue` / `NativeActivity`.
- **Renderizador Gráfico GLES 3.0 Moderno:**
  - Terreno de alta precisão com elevação e splatting de texturas de grama, terra e penhasco.
  - Água dinâmica com reflexos de Fresnel analíticos, mapa normal procedural/DDT, caustics e espuma de praia autêntica.
  - Cúpula celeste (Skydome) e suporte a Cube Maps originais (`.cub`).
  - Shaders otimizados para GPUs mobile (Adreno, Mali, PowerVR).
- **Parsers dos Formatos Originais da Ensemble Studios:**
  - **BAR / BAR2 (Multi-mount Overlay):** Leitor de arquivos de arquivo compactados via `mmap` com suporte a sobreposição transparente da expansão *The Titans* (`*2.bar`).
  - **DDT:** Decodificador de texturas suportando formatos 1, 2, 3 (paleta 256 cores com canal alfa 4-bit), 5 (RGB5551/alfa 1-bit) e 6 (DXT/ARGB).
  - **BRG:** Leitor de modelos 3D com geometria multiparte, malhas unificadas e atribuição de texturas através de chunks MTRL.
  - **GRN:** Suporte a animações e malhas Granny 2.
  - **XMB:** Parser de arquivos binários compactados XML (`proto.xmb`, `protox.xmb`, `techtree.xmb`, etc.).
  - **SCN / SCX:** Carregador de cenários binários comprimidos com descompressão l33t/zlib e leitura de heightmaps.
  - **CPN / CPX:** Parser de campanhas originais (*Fall of the Trident* e *The New Atlantis*).
  - **LGT:** 11 presets de iluminação solar e ambiente.
- **Interface e Menus Autênticos (XMB + UI Touch):**
  - Carregamento de interfaces autênticas como `uipregamenew.xml`, `uicampaign.xmb` e `uicampaignsetup.xmb`.
  - Mapeamento de botões autênticos (`buttonsets.xmb`) com texturas PNG/DDT e estados (normal, rollover, pressionado, desabilitado).
  - Controles por toque com gestos de pan, pinch-to-zoom isométrico e seleção por anel/área.
- **Multiplayer LAN Nativo:**
  - Descoberta automática de salas na rede local via UDP broadcast (porta 2300).
  - Protocolo completo de conexão, chat e gerenciamento de slots com suporte a teclado virtual do Android.
- **Áudio Posicional 3D em Baixa Latência:**
  - Motor OpenSL ES nativo com atenuação espacial, rolloff quadrático e panning estéreo de energia constante.
  - Reprodução de vozes gregas originais (*"Prostagma!"*, *"Etimos!"*, *"Vulome!"*) e efeitos de combate/colheita.
- **Cinemáticas e Vídeo:**
  - Suporte a cutscenes autênticas via `AMediaCodec` / `AMediaExtractor` do NDK para reprodução eficiente sem travamentos.

---

## 🎯 Estado Atual e Recursos Funcionais

- **Modo Campanha:**
  - Telas de seleção de campanha com visualização de mapas regionais, descrição de objetivos e seletor de dificuldades (*Fácil, Moderado, Difícil, Titã*).
  - Missão 1 (*Omens*): Arkantos, desembarque pirata e confronto com o Kraken.
- **Modo Partida Rápida / Skirmish:**
  - Economia RTS completa: aldeões realizam colheita autônoma (comida, madeira, ouro) e depositam no Centro da Cidade.
  - Construção de edificações com custos deduzidos, progresso de obra e efeitos de faíscas.
  - Fila de treinamento com limite populacional ($Pop/Cap$) dinâmico.
  - Combate corpo a corpo e à distância com IA tática de oponentes.
  - Condições de vitória ativas: **Conquest** e **Sudden Death**.
  - Sistema de **Save/Load Persistente** (formato binário `AOMS` v1) com roundtrip completo de recursos, tempo de jogo, unidades e edifícios.
- **Cenário Dinâmico:**
  - Vegetação nativa (carvalhos, palmeiras, pinheiros, rochas) e fauna decorativa espalhada pelo mapa (gazelas, aurochs, leões, javalis).

---

## 📂 Estrutura do Projeto

```text
├── android/            # Projeto Android nativo (AndroidManifest, Java bridge, ProGuard)
├── assets/             # Texturas, fontes e arquivos de apoio empacotados no APK
├── docs/               # Documentação técnica de arquitetura, símbolos e guias
│   ├── CURRENT_STATE.md # Estado consolidado e tarefas abertas (máx. 60 linhas)
│   ├── SYMBOLS.md       # Mapa índice de funções de todo o código-fonte
│   └── IMPORTACAO_DADOS.md # Manual detalhado de importação e solução de problemas
├── include/            # Cabeçalhos públicos em C++20 organizados por subsistemas
│   ├── assets/         # BAR, DDT, BRG, XMB, CubeMap, ButtonSets
│   ├── audio/          # Interfaces de áudio posicional 3D
│   ├── campaign/       # Gerenciamento de campanhas e missões
│   ├── ecs/            # Componentes e sistemas de jogabilidade RTS
│   ├── engine/         # Núcleo da engine, ciclo de vida e estados
│   ├── network/        # Protocolo e gerenciamento de rede multiplayer LAN
│   ├── renderer/       # Abstração de renderização GLES 3.0 e câmera
│   └── ui/             # Sistema de widgets, layouts XMB e toque
├── src/                # Implementação modular dos subsistemas em C++
│   ├── assets/         # Decodificadores binários de formatos originais
│   ├── audio/          # Backend OpenSL ES multi-voz
│   ├── campaign/       # Triggers, cinemáticas e objetivos
│   ├── engine/         # Engine core e módulos divididos por responsabilidade
│   ├── game/           # Save/load, regras de vitória, god powers e triggers
│   ├── network/        # Sockets UDP, serialização e lobby
│   ├── platform/       # Entrada touchscreen e NativeActivity bridge
│   ├── renderer/       # Shaders GLSL, mesh builders e render loop
│   └── ui/             # Widgets nativos (ScrollList, RadioSet, Button, Label)
├── tests/              # Referências de assets extraídos e testes de validação
├── tools/              # Utilitários de indexação, extração e testes de conformidade
├── CMakeLists.txt      # Pipeline de compilação modular CMake
├── PROGRESSO.md        # Registro cronológico detalhado de todos os sprints
└── AGENTS.md           # Regras e convenções estritas de desenvolvimento
```

---

## 🛠️ Como Compilar e Empacotar o APK

### Pré-requisitos
- **Sistema Operacional:** Windows, Linux ou macOS.
- **Android SDK:** API 34 ou 36.1 instalada com `build-tools` (34.0.0 ou 37.0.0).
- **Android NDK:** r25c até r28 (suporte ao compilador Clang C++20).
- **Java:** JDK 17+ (ou JBR do Android Studio).
- **CMake:** 3.22.1 ou superior + **Ninja**.

---

### Opção 1: Empacotamento Automatizado via PowerShell (Windows)

O repositório inclui um pipeline completo em [`scripts/package_apk.ps1`](scripts/package_apk.ps1), que automatiza a compilação Java, minificação com **R8/ProGuard**, stripping de bibliotecas nativas via `llvm-strip`, alinhamento `zipalign` e assinatura criptográfica v2 e v3:

```powershell
# Executar a partir da raiz do repositório
powershell -ExecutionPolicy Bypass -File scripts\package_apk.ps1
```

O instalador final otimizado será gerado na raiz:
```text
aom-native-debug.apk (~17.5 MB)
```

Para compilar, instalar diretamente no celular conectado via ADB e abrir o app:
```powershell
powershell -ExecutionPolicy Bypass -File scripts\package_apk.ps1 -Install -Launch
```

---

### Opção 2: Compilação via Gradle / Android Studio

1. Abra a pasta `android/` no **Android Studio**.
2. Aguarde a sincronização do Gradle e clique em **Run** ou execute:
   ```bash
   ./gradlew :app:assembleRelease
   ```

---

### Opção 3: Compilação dos Testes Automatizados no Host

Para validar a integridade dos subsistemas matemáticos, de áudio, parsing e gameplay:

```bash
# Criar pasta de build e compilar suítes
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --target gameplay_test test_audio_3d test_bar_overlay test_network

# Executar testes
ctest --test-dir build --output-on-failure
```

---

## 🎮 Como Importar os Dados Originais do Jogo

O aplicativo contém o motor e a lógica completa de execução, mas **não inclui os arquivos de áudio, texturas e modelos comerciais** de *Age of Mythology*. Você deve importá-los da sua cópia original de PC.

### Método 1: Importação Direta pelo App (Recomendado)

1. Instale e abra o aplicativo no Android.
2. Toque em **OPÇÕES** no menu principal.
3. Toque no botão **IMPORTAR DADOS DO JOGO**.
4. O seletor de pastas nativo do Android (**Storage Access Framework - SAF**) será exibido.
5. Navegue até a pasta do seu jogo (por exemplo, `Age of Mythology` ou `Age of Mythology Gold Edition`) e toque em **Usar esta pasta**.
6. O aplicativo copiará os arquivos essenciais em segundo plano para a pasta interna segura (`Android/data/com.aom.nativeport/files/AoM/`) e reiniciará a engine com tudo carregado.

### Método 2: Cópia Manual via USB / ADB

Você pode copiar a pasta do jogo diretamente para o seu aparelho via cabo USB ou ADB para um dos seguintes caminhos:

- `/storage/emulated/0/Download/AoM/`
- `/storage/emulated/0/Android/data/com.aom.nativeport/files/AoM/`

#### Estrutura de Arquivos Recomendada:

```text
AoM/
├── data/
│   ├── data.bar
│   ├── data2.bar               # The Titans (opcional)
│   ├── main campaign.cpn       # Campanha original
│   └── xpack campaign.cpx      # Campanha The Titans (opcional)
├── textures/
│   ├── textures.bar
│   └── textures2.bar           # The Titans (opcional)
├── models/
│   ├── models.bar
│   └── models2.bar             # The Titans (opcional)
├── scenario/
│   ├── scenarios.bar
│   └── scenarios2.bar          # The Titans (opcional)
├── sound/
│   ├── sounds.bar
│   └── sounds2.bar             # The Titans (opcional)
├── anim/
│   ├── anim.bar
│   └── anim2.bar               # The Titans (opcional)
└── god powers/
    ├── godpowers.bar
    └── godpowers2.bar          # The Titans (opcional)
```

> [!TIP]
> **Suporte a The Titans:** Se você possui a expansão *The Titans*, basta incluir os arquivos `*2.bar`. A engine nativa aplica o sistema de **overlay** idêntico ao jogo de PC, montando os arquivos da expansão sobre o jogo base e substituindo automaticamente dados atualizados de unidades, tecnologias e deuses.

Para mais informações sobre solução de problemas, permissões e análise de logs, veja o guia completo em [`docs/IMPORTACAO_DADOS.md`](docs/IMPORTACAO_DADOS.md).

---

## 🔍 Depuração e Logs

Para acompanhar o diagnóstico da engine em tempo real durante a execução no Android:

```bash
adb logcat -d | grep -E "AoM|\[TEX\]|\[GADGET\]|\[DIAG\]|\[FONT\]|\[DDT\]|\[AUDIO\]"
```

---

## ⚖️ Aviso Legal e Licenças

- *Age of Mythology*, *The Titans* e suas marcas, artes, modelos, músicas e dados são propriedades intelectuais registradas da **Microsoft Corporation** e **World's Edge**.
- Este repositório é um projeto não-oficial desenvolvido por entusiastas da comunidade para interoperabilidade, aprendizado de computação gráfica e engenharia reversa de formatos antigos.
- Nenhuma propriedade intelectual comercial ou arquivo `.bar` original é redistribuído neste repositório.
- Bibliotecas embutidas na pasta `third_party/` mantêm suas licenças autorais individuais (MIT/Public Domain).
