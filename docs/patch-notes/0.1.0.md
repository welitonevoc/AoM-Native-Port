# Patch 0.1.0 — MVP Completo / Vertical Slice
*11 de novembro de 2026*

## Destaques
Esta atualização completa o MVP (Produto Mínimo Viável) do Age of Mythology Native Port, entregando uma partida jogável do início ao fim: economia, construção, combate e condição de vitória.

## Novo Conteúdo
### Condição de Vitória (Conquest e Sudden Death)
Partidas agora terminam corretamente. Vence por Conquest quando não há mais Town Centers inimigos, ou por Sudden Death quando o Town Center do jogador é destruído. Uma tela de vitória/derrota é exibida ao final da partida.

### Town Center
O Town Center é agora construível pelo jogador via HUD (custo: 400 Madeira, 200 Ouro) e funciona como ponto de retorno de recursos e fonte de população (+15 de limite populacional cada).

### Seleção de Grupo (Box Select)
Selecione múltiplas unidades arrastando o dedo sobre a tela. Anéis de seleção aparecem nas unidades selecionadas, e um clique no vazio limpa a seleção.

### HUD Completo
O HUD em jogo agora exibe contador de população (atual/cap), minimapa com blips de recursos e unidades, painel de comandos contextual e barra visual de progresso de produção.

### IA Inimiga
Oponentes controlados por IA agora treinam unidades, coletam recursos, constroem edifícios e atacam o jogador de forma autônoma.

### Áudio Expandido
Suporte a sound banks consumidos dos arquivos do jogo, áudio posicional 3D, múltiplos efeitos sonoros simultâneos sem cortes, e controle de volume por categoria (música e SFX).

### UI Baseada em XMB e Localização
Menus e painéis de comando carregados a partir de arquivos XMB (`uimain.xmb`, `commandpanel.xmb`) com fallback procedural. Strings da UI extraídas para tabela de localização (base para futuros idiomas).

## Qualidade de Vida
- Barra de progresso de produção com tooltip dinâmico mostrando custo e descrição do comando
- Contagem de multi-seleção exibida no HUD ("N selecionadas")
- Minimapa radar no canto inferior esquerdo com cores por tipo de recurso

## Desempenho
- Validação de 60 FPS estáveis e sem vazamento de memória (OOM) em sessão prolongada
- Culling por frustum e distância na renderização
- Pool de partículas sem realocações durante o jogo

## Problemas Conhecidos
- O teste de save/load de entidades não pôde ser validado em ambiente de desenvolvimento (restrição de sistema de arquivos). Requer validação em dispositivo Android real.
- Teste de soak (4+ horas contínuas) não executado neste build.
- Métricas de tempo de carregamento não mensuradas nesta versão.

---
Obrigado por jogar! Seu feedback nos ajuda a tornar o jogo melhor.
Relate problemas em: https://github.com/welitonevoc/AoM-Native-Port/issues
