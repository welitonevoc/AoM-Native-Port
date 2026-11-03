# MVP — Age of Mythology Native (Android)

## Visão
Um **mini-RTS jogável** em Android com 1 facção (Gregos), 1 mapa procedural, 3 tipos de unidade e economia básica, rodando a 60 FPS em GLES3. Não tenta paridade completa com o original.

## Escopo (IN)
- 1 facção: Gregos (Hoplite, Toxotes, Villager).
- 1 mapa procedural (terreno atual 41×41).
- 3 tipos de unidade + 1 myth unit (Minotaur) como inimigo.
- Economia: Food, Wood, Gold (Favor desativado).
- Construção: Town Center, House, Barracks (via proto.xmb stats).
- Combate corpo-a-corpo e à distância (já em CombatSystem).
- Pathfinding A* (já implementado).
- Touch input: tap selecionar, drag mover, pinch zoom, pan (TouchInputManager).
- HUD: recursos, população, seleção, botão de construção.
- Áudio: WAV PCM (já), música de menu MP3 (verificar).
- Vídeos de abertura (logo e introdução): transcodificados localmente de BIK para MP4/H.264; sem campanhas.
- Save/Load de partida (serialização básica).

## Fora do escopo (OUT)
- Campanhas/cenários (.cpn/.scn).
- Multiplayer/rede.
- IA avançada (random maps, triggers).
- Animações em runtime (scripts `.txt`, frames `.brg` e rigs/clipes `.grn`), partículas (.prt), skybox (.cub).
- Outras facções (Egípcios, Nórdicos, Atlantes).
- Deuses menores / tecnologias avançadas.
- Localização (strings fixas em português).

## Critérios de sucesso
1. APK instala e roda em Android 8+ (API 26+).
2. 60 FPS estáveis em mid-range (Snapdragon 6xx).
3. Partida completa (spawn → economia → combate → vitória) em < 10 min.
4. Sem OOM em 30 min de jogo (streaming de BAR).
5. Touch input responsivo (< 100 ms de latência).

## Milestones
- M1 (implementado; runtime pendente): infraestrutura nativa, parsers e render.
- M2 (protótipo implementado): UI, áudio, terreno e input touch; validar integração e comportamento em aparelho.
- M3 (protótipo implementado): unidades, combate, economia e pathfinding; completar regras do MVP e validar uma partida.
- M4 (próximo): HUD completo (incluindo população), Town Center, seleção de grupo, save/load e condição de vitória.
- M5: estabilidade, desempenho, consumo de memória e testes de aceitação em aparelhos-alvo.
- M6: release beta após cumprir os critérios de sucesso.

## Estado atual
- Hoplite, Villager Greek, Toxotes e Minotaur são instanciados usando atributos extraídos de `proto.xmb`; somente o Hoplite tem malha e textura reais integradas ao gameplay.
- O inventário e o mapa de dependências dos scripts de animação e recursos BRG/Granny estão em `docs/ANIMATION_MAP.md`; o port ainda não interpreta nem reproduz esses clips.
- O mapa 41×41, movimento, combate, coleta e construção funcionam como protótipos. As regras e a partida completa ainda não foram validadas.
- O HUD atual mostra recursos, seleção/vida e ações, mas não população. Favor ainda aparece no HUD, apesar de estar fora da economia do MVP.
- Há House, Barracks e Tower; Town Center ainda não está implementado.
- O `TouchInputManager` existe, mas sua integração como entrada geral do engine e a seleção de grupo precisam ser concluídas.
- Save/load é experimental: persiste unidades e posições em um slot simples, mas ainda não restaura todos os recursos e estados; condição de vitória continua pendente.
- Build e assinatura do APK foram verificados; instalação, runtime, 60 FPS, duração da partida, latência touch e estabilidade de memória ainda precisam de testes em dispositivo.
- O menu oferece as categorias Um Jogador, Multiplayer, Cinema, Opções, História, Editor, Créditos e Sair. A partida MVP e a reprodução da abertura estão ligadas; campanhas, rede, editor e enciclopédia aparecem indisponíveis, sem simular suporte.
- O Cinema reproduz `aom-logo.mp4` e `introcg.mp4` via janela Android fullscreen centralizada. Os vídeos são convertidos localmente e empacotados fora do controle de versão.

## Riscos
- Memória: mitigado por streaming de BAR.
- Performance: mitigado por cache de texturas + mipmaps.
- Escopo: controlado pelo MVP acima.
