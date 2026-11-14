# CURRENT_STATE — baseado na versão 1.0.11140131 (14/11/2026). Máx. ~60 linhas.
Legenda: ✅ confirmado no código/captura · ❓ hipótese sem teste · ⬜ ainda não feito

## Já aplicado no GitHub (verificado no código)
- ✅ Decoder DDT Formato 3 (PALETTE 4-bit Alpha DecodeBt8As4444): linha a linha, 4 pixels por grupo de 6 bytes com inversão vertical e paleta RGB444. Logo Titans, pórtico e botões renderizam 100% opacos e na orientação correta.
- ✅ Mapeamento UV de Botões (`ui_layout.cpp`): inversão de V nos botões e ButtonSets; jade e molduras renderizam perfeitamente.
- ✅ Localização PT-BR dos Menus (`engine_assets.cpp`): IDs oficiais (`19825`..`23603`) traduzidos para Português (Aprender a Jogar, Campanha, Jogador Único, Multijogador, Opções, Mais, Sair, etc.).

## Correções aplicadas nesta iteração (14/11/2026)
- ✅ **Centralização Exata de Textos de Botões (`ui.cpp` & `gles_renderer.cpp`)**: Implementado `measure_ui_text` em `IRenderer` e `GLESRenderer`. A largura dos botões agora usa métricas reais de glifos ao invés de estimativa por tamanho de bytes.
- ✅ **Suporte UTF-8 com Acentuação PT-BR (`gles_renderer.cpp`)**: Decodificador UTF-8 para Latin-1 supplement (ç, ã, õ, é, ú, etc.) mapeando caracteres acentuados para glifos ASCII sem descartar letras nem truncar palavras.
- ✅ **Orientação da Costa e Mar 3D (`engine_terrain.cpp` & `engine_states.cpp`)**: Revertida inversão lateral. A ilha com platô verde elevado fica no lado esquerdo da tela (`wx < shore_x`), e o mar azul profundo no lado direito e horizonte. Câmera isométrica frontal ajustada (`yaw=0.08`, `pitch=0.35`, `target=(-1.0, 0.5, 0.0)`).
- ✅ **Edifícios Completos e Visibilidade do Solar/Manor (`engine_gameplay.cpp`)**: `b.complete = true` e `b.progress = 1.0f` ativados em todos os edifícios do menu (remove andaimes amarelos). Solar (Manor) dimensionado para escala 1.35x e posicionado no platô esquerdo com palmeiras e tropas Atlantes em patrulha visível.

## Próximos passos (ordem)
1. ✅ APK v1.0.11140131 gerado e assinado em `/sdcard/aom-native-v1.0.11140131.apk`.
2. Validar no dispositivo o alinhamento central dos botões e a cena do Solar com a praia e o mar.