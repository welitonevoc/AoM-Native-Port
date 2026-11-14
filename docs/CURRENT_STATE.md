# CURRENT_STATE — baseado na versão 1.0.11140229 (14/11/2026). Máx. ~60 linhas.
Legenda: ✅ confirmado no código/captura · ❓ hipótese sem teste · ⬜ ainda não feito

## Já aplicado no GitHub (verificado no código)
- ✅ Decoder DDT Formato 3 (PALETTE 4-bit Alpha DecodeBt8As4444): Logo Titans, pórtico e botões renderizam 100% opacos e na orientação correta.
- ✅ Mapeamento UV de Botões (`ui_layout.cpp`): inversão de V nos botões e ButtonSets; jade e molduras renderizam perfeitamente.
- ✅ Localização PT-BR dos Menus (`engine_assets.cpp`): IDs oficiais (`19825`..`23603`) traduzidos para Português.
- ✅ Centralização Exata de Textos de Botões (`ui.cpp` & `gles_renderer.cpp`): `measure_ui_text` com suporte a caracteres acentuados PT-BR.
- ✅ Decodificação de Alpha DDT Formato 5 (`ddt5_auto.h` & `ddt_texture.cpp`): Máscara 1-bit alpha corrigida e diffuse sólido.

## Correções aplicadas nesta iteração (14/11/2026)
- ✅ **Parser de Geometria BRG (`brg_model.cpp`)**: Corrigida a leitura de posições de vértices de 12 bytes (`float32` x,y,z) para modelos com `vert_stride == 4` (edifícios como Manor/Solar, palmeiras e rochas). Resolvido o desalinhamento que corrompia malhas, UVs e materiais.
- ✅ **Posicionamento e Elevação de Entidades do Menu (`engine_gameplay.cpp` & `gameplay.h`)**:
  - Solar Atlante (Manor 1 & 2), Quartel, Farol, palmeiras e patrulhas de Murmillo e Arcus reposicionados para os flancos visíveis da tela (platô esquerdo Screen X ~ 22% a 28% e praia direita Screen X ~ 68% a 74%), sem oclusão pelo pórtico central (Screen X = 32% a 68%).
  - `update_orders` em `gameplay.h` ajustado para navegar no plano horizontal XZ com `tr.velocity.y = 0.0f`, eliminando o afundamento das tropas abaixo do terreno.
- ✅ **Shader de Água Estilizado e Espuma de Rebentação (`gles_renderer.cpp`)**:
  - Removido reflexo especular PBR excessivo em favor da iluminação clássica de AoM.
  - Integrada textura de caustics `water0000.ddt` (`uWaterTex`) com coordenadas UV duplas em movimento.
  - Adicionadas ondas com cristas de espuma volumosa quebrando na praia (`surfZone` / `crestFoam`) e marulhar dinâmico da maré na orla (`washFoam`).

## Próximos passos (ordem)
1. ✅ APK v1.0.11140229 gerado e assinado em `/sdcard/aom-native-v1.0.11140229.apk`.
2. Validar no dispositivo as espumas quebrando na praia, o Solar Atlante e as patrulhas nos flancos abertos.