# Arquitetura Técnica, Engenharia Reversa e Subsistemas do AoM Native Port

Este documento consolida a documentação técnica, as especificações de formatos de arquivos originais de *Age of Mythology* e *Age of Mythology: The Titans*, e os subsistemas de renderização, geometria e navegação implementados no port nativo em **C++20** para **Android (GLES3)**.

---

## 1. Pipeline de Texturas DDT (DirectDraw Texture)

O formato proprietário `.ddt` (DirectDraw Texture) é o padrão de armazenamento de imagens nos arquivos `.bar` do jogo. Cada textura possui um cabeçalho de 32 bytes (`RTS3` magic) identificando dimensões, formato de pixel, quantidade de canais e bits de transparência (alpha).

### 1.1 Formato 3: Paletizado com Alpha de 4 bits (`PALETTE 4-bit Alpha`)

Utilizado em artes de interface com transparência gradativa rica (como o logotipo de *The Titans*, pórtico de mármore e ornamentos de botões):
- **Problema anterior:** Era decodificado como blocos 4×4 em faixas de 64 pixels, gerando artefurada com 95%+ de transparência incorreta (`opaque=2707, trans=53146`).
- **Especificação Real (`DecodeBt8As4444`):**
  - Armazenado linha a linha contígua, processado em grupos de 4 pixels (6 bytes por grupo).
  - Cada grupo de 6 bytes contém:
    1. `uint16_t alpha_mask`: 4 nibbles de 4 bits cada (1 nibble por pixel).
    2. 4 bytes de índices para a tabela de paleta de 256 cores.
  - A paleta armazena cores em formato `B4G4R4` nos 12 bits inferiores; o nibble de alpha é mesclado nos bits 12–15 (`color = pal_color | (alpha_nibble << 12)`).
  - Aplicação de inversão vertical $V' = (H - 1 - y)$ para converter do espaço Direct3D (origem no topo) para OpenGL ES (origem na base).

### 1.2 Formato 5: RGB / Cutout com Alpha de 1 bit

Utilizado em texturas de terreno, edifícios e folhagem:
- **Problema anterior:** A inversão de máscara de 1 bit causava recorte inverso (árvores e telhados ficavam invisíveis ou com buracos transparentes).
- **Especificação Real:**
  - O bit 1 na máscara `am` indica transparência/cutout para folhas e bordas recortadas: `p.a = ((am >> bit) & 1) ? 0 : 255`.
  - Para texturas difusas sólidas padrão (`usage == 1` ou sem alpha declarada), o canal alfa é travado em `255` (opaco), garantindo a solidez dos telhados de edifícios (como o Solar Atlante) e peles de tropas.
  - No fragment shader de malhas (`kMeshFragmentSrc`), pixels com `baseColor.a < 0.25` são descartados (`discard`), permitindo recortes nítidos de folhagem sem artefatos de blending.

---

## 2. Pipeline de Geometria 3D BRG (`BANG` Format)

O formato `.brg` armazena geometria de malhas 3D para edifícios, unidades, árvores e rochas.

### 2.1 Estrutura de Chunks
- **Header Global:** Magic `BANG` (28 bytes).
- **Chunk `MESI` (Mesh Information):**
  - Offset 0 (2 bytes): Versão (típico `22` / `0x0016`).
  - Offset 2 (2 bytes): Atributos de formato / flags (valor `4` em edifícios e árvores, `12` em unidades com morph).
  - Offset 4 (2 bytes): `num_verts` (contagem de vértices).
  - Offset 6 (2 bytes): `num_faces` (contagem de faces/triângulos).
- **Dados dos Vértices:**
  - **Posições:** Sempre 12 bytes `float32` ($X, Y, Z$) por vértice quando `num_verts * 12 <= size`.
  - **Coordenadas de Textura (UV):** 4 bytes por vértice (2 `half_float` $U, V$, convertidos via `half_to_float`).
  - **Materiais por Face:** `num_faces * 2` bytes (`uint16_t` por face).
  - **Índices de Triângulos:** `num_faces * 6` bytes (3 `uint16_t` por triângulo).
- **Chunk `MTRL` (Material):** Contém strings ASCII com os nomes de textura (ex.: `Building X Roof Age1`, `World E Palm mapB`, `Infantry X 2 Sword Fighter Iron`).
- **Resolução de Texturas:** A engine normaliza os nomes e pesquisa nos archives `textures.bar` e `textures2.bar` tolerando variações de sufixo e extensão (`.ddt`).

---

## 3. Cena 3D do Menu Principal e Projeção de Câmera

A cena de fundo de *The Titans* utiliza renderização 3D em tempo real com ilha, mar e unidades móveis.

### 3.1 Geometria da Câmera e Oclusão Central
- **Parâmetros da Câmera:**
  - Posição: $Eye \approx (0.13, 7.64, 14.05)$
  - Ponto de Olhar: $Target = (-1.0, 0.5, 0.0)$
  - Campo de Visão: $FOV = 45^\circ$, Aspect Ratio $16:9$.
- **Área Oclusa pela Interface:**
  - O pórtico de mármore e os botões ocupam a faixa central: $\text{Screen } X \in [32\%, 68\%]$.
  - Entidades posicionadas entre $x = -3.8f$ e $-2.2f$ projetavam em $\text{Screen } X \in [38\%, 44\%]$, ficando totalmente ocultas atrás das colunas e placas de jade.

### 3.2 Posicionamento nos Flancos Abertos
- **Flanco Esquerdo (Platô Verde Elevado, $\text{Screen } X \in [22\%, 28\%]$):**
  - Solar Atlante 1 (`Manor Atlante`) em $x = -6.5f, y = 1.35f, z = 0.0f$.
  - Quartel Atlante em $x = -7.2f, y = 1.45f, z = 1.5f$.
  - Palmeiras e Carvalhos em $x = -5.2f$ a $-7.8f$.
  - Patrulhas de Murmillo 2 e Arcus 2.
- **Flanco Direito (Praia e Costa do Mar, $\text{Screen } X \in [68\%, 74\%]$):**
  - Solar Atlante 2 (`Manor Atlante`) em $x = 3.6f, y = 1.10f, z = -0.5f$.
  - Farol costeiro em $x = 4.2f, y = 0.40f, z = -3.5f$.
  - Bosque de Palmeiras na areia em $x = 2.5f$ a $4.5f$.
  - Patrulhas de Murmillo 1 e Arcus 1 patrulhando a orla da praia.

---

## 4. Sistema de Navegação e Correção de Elevação

- **Isolamento de Altura em Navegação:**
  - O algoritmo A* de navegação em grade gera pontos de rota no plano horizontal.
  - Para evitar que unidades se enterrem sob o terreno (pois a encosta costeira varia de $y = 0.85f$ a $1.50f$, enquanto a grade possuía $y = 0.0f$), a atualização de ordens (`update_orders` em `include/ecs/gameplay.h`) isola o movimento no plano XZ:
    $$\Delta_{xz} = \begin{pmatrix} wp_x - tr_x \\ wp_z - tr_z \end{pmatrix}, \quad v_x = \frac{\Delta_x}{\|\Delta\|} \cdot s, \quad v_z = \frac{\Delta_z}{\|\Delta\|} \cdot s, \quad v_y = 0.0f$$
  - As tropas mantêm sua elevação no relevo da encosta durante toda a patrulha.

---

## 5. Renderização de Água Estilizada do AoM e Espuma de Rebentação

O shader de água (`kWaterFragmentSrc` em `src/renderer/gles_renderer.cpp`) foi reescrito para respeitar a estética de *Age of Mythology*:
- **Eliminação de Especular PBR Ruidoso:** Substituído o brilho concentrado de potência 32 por iluminação difusa suave do sol de estilo RTS clássico.
- **Caustics de Textura:** Amostragem contínua e cruzada de `water0000.ddt` (`uWaterTex`) com coordenadas UV duplas em movimento secular.
- **Espuma de Rebentação das Ondas (Surf):**
  - Mapeamento analítico da costa idêntico ao relevo do terreno:
    $$\text{shoreOffset} = -0.4 + z \cdot 0.12 + \sin(z \cdot 0.22) \cdot 0.9$$
    $$\text{distToCoast} = x - \text{shoreOffset}$$
  - Propagação ondulatória em direção à praia gerando cristas espumosas espessas na zona rasa (`surfZone` / `crestFoam`).
  - Marulhar da maré com oscilação periódica na faixa de areia (`washFoam`), resultando em ondas quebrando de forma realista e estilizada.

---

## 6. Interface de Usuário e Localização PT-BR

- **Localização de Strings:** Mapeamento integral dos IDs originais de string do jogo (`19825` a `23603`) para Português do Brasil em `src/engine/engine_assets.cpp`.
- **Centralização Precisa:** `measure_ui_text` calcula a largura real considerando caracteres multibyte UTF-8 acentuados (`ã`, `é`, `ç`, etc.), garantindo alinhamento central milimétrico nos botões de jade.
- **Correção de UV nos Botões:** Inversão no eixo vertical $V$ no parser de layout XMB para corrigir a orientação de cabeçalhos e bordas ornamentadas.
