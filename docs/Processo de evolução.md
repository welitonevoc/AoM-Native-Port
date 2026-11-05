# Processo de Evolução do AoM-Native-Port

**Documento de planejamento técnico, acompanhamento e validação**\
**Projeto:** AoM-Native-Port --- port nativo de *Age of Mythology*\
**Versão do documento:** 1.0\
**Data de elaboração:** 05/11/2026\
**Idioma:** Português

------------------------------------------------------------------------

## 1. Finalidade deste documento

Este documento estabelece um processo organizado para conduzir, avaliar
e acompanhar a evolução do AoM-Native-Port. Ele reúne:

-   o diagnóstico inicial descrito na documentação técnica analisada;
-   as lacunas funcionais e técnicas identificadas;
-   uma proposta de arquitetura para organizar a implementação;
-   prioridades, dependências e critérios objetivos de conclusão;
-   um roteiro incremental de desenvolvimento;
-   procedimentos de compilação, execução, testes e registro de
    evidências;
-   riscos conhecidos, inconsistências documentais e decisões que ainda
    precisam ser confirmadas;
-   um modelo de relatório para os acompanhamentos semanais.

O objetivo principal não é apenas fazer o jogo abrir. É alcançar uma
versão nativa estável, jogável e progressivamente completa, com
controles adequados ao dispositivo Android e sem confundir
funcionalidades prototipadas com funcionalidades realmente concluídas.

**Regra fundamental:** nenhuma funcionalidade deve ser classificada como
concluída apenas porque existe uma classe, um método, uma referência na
documentação ou um recurso carregado. A conclusão exige evidência
compatível com o tipo de funcionalidade: implementação examinada, teste
reproduzível e, quando aplicável, validação em execução no
dispositivo-alvo.

------------------------------------------------------------------------

## 2. Escopo e limites do diagnóstico inicial

### 2.1 Materiais considerados

O diagnóstico que motivou este plano foi baseado nos materiais
disponibilizados na conversa:

-   `aomxnocd.exe`;
-   `aomxnocd.md`.

O executável analisado possui formato PE do Windows. A análise do
binário e de sua documentação pode revelar imports, estruturas,
símbolos, strings, recursos e indícios sobre o funcionamento do jogo
original.

Isso **não comprova**, por si só, que o port nativo já implementa esses
sistemas, que os componentes estão integrados ou que o jogo funciona em
execução.

### 2.2 Limite importante

O documento técnico analisado descreve uma análise estática do
executável Windows. Portanto, as observações sobre o port devem ser
tratadas como um ponto de partida para auditoria do código-fonte e
testes reais.

Não se deve afirmar que o projeto compila, inicia, carrega uma partida
ou executa corretamente determinada funcionalidade sem ter realizado o
teste e guardado evidências.

### 2.3 Níveis de evidência obrigatórios

Todo item do projeto deverá receber um dos seguintes níveis:

  -----------------------------------------------------------------------
  Nível                   Classificação           Significado
  ----------------------- ----------------------- -----------------------
  E0                      Não verificado          Não há evidência
                                                  suficiente para
                                                  avaliar.

  E1                      Documentado             A documentação afirma
                                                  que existe ou descreve
                                                  o comportamento.

  E2                      Encontrado no código    Há implementação
                                                  identificável no
                                                  código-fonte, mas ainda
                                                  não foi validada em
                                                  execução.

  E3                      Testado isoladamente    O componente passou por
                                                  um teste reproduzível,
                                                  ainda que fora de uma
                                                  partida completa.

  E4                      Validado em jogo        O comportamento foi
                                                  observado em uma
                                                  partida real e o teste
                                                  foi registrado.

  E5                      Validado no             O comportamento foi
                          dispositivo-alvo        validado no
                                                  Android/dispositivo de
                                                  referência, com
                                                  estabilidade e
                                                  desempenho aceitáveis.
  -----------------------------------------------------------------------

Uma funcionalidade só deve ser considerada **concluída** quando atingir
o nível de evidência definido para ela. Para funcionalidades centrais de
jogabilidade, o alvo mínimo é E4; para interface, desempenho e
compatibilidade Android, o alvo é E5.

### 2.4 Status de implementação

Além do nível de evidência, cada item deverá ter um status:

-   **Pendente:** ainda não implementado.
-   **Em desenvolvimento:** implementação em andamento.
-   **Parcial:** existe apenas parte do comportamento esperado.
-   **Bloqueado:** depende de uma correção, ferramenta, informação ou
    componente externo.
-   **Implementado, não validado:** código presente, mas sem teste
    suficiente.
-   **Validado:** passou pelos critérios de aceitação.
-   **Regredido:** funcionava anteriormente e deixou de funcionar após
    uma alteração.

Não usar "pronto" como sinônimo de "há código".

------------------------------------------------------------------------

## 3. Visão geral do estado inicial

A tabela abaixo consolida o estado descrito no relatório técnico
analisado. É uma **linha de base documental**, não uma certificação do
estado atual do código-fonte.

  -----------------------------------------------------------------------
  Subsistema              Estado descrito         Leitura para o
                                                  planejamento
  ----------------------- ----------------------- -----------------------
  Carregamento BAR e      Implementado no Sprint  Auditar integração,
  overlay                 11                      erros de recursos e
                                                  estabilidade.

  Protótipo XMB /         Implementado como       Confirmar navegação,
  techtree                protótipo               dados e ligação com a
                                                  partida.

  God Powers              Recursos carregados e   Completar regras,
                          lançamento parcial      alvos, custos, efeitos
                                                  e interface.

  Renderizador GLES3      Descrito como concluído Validar cenas
                                                  completas, dispositivos
                                                  e desempenho.

  Controles de toque      Parciais                Necessário para uma
                                                  experiência Android
                                                  utilizável.

  Ações de unidades       Protótipo               Completar seleção,
                                                  movimento, ataque,
                                                  coleta e construção.

  Pathfinding A\*         Protótipo               Validar obstáculos
                                                  dinâmicos, grupos e
                                                  desempenho.

  Formações               Pendentes               Dependem de seleção em
                                                  grupo e movimentação
                                                  confiável.

  IA / planos / knowledge Pendentes               Priorizar depois do
  base                                            ciclo básico de jogo.

  Centro da Cidade e      Parciais; Centro da     Bloqueador para o ciclo
  economia                Cidade ausente segundo  de jogo econômico
                          o relatório             completo.

  Vitória no modo         Bug conhecido           Corrigir e testar
  Conquest                                        condições de
                                                  vitória/derrota.

  Animações Granny        Dados não interpretados Necessário pipeline de
                          pelo runtime            conversão ou
                                                  interpretação.

  Mapas procedurais       Básicos                 Evoluir geração,
                                                  validação e variedade.

  Campanhas               Detectadas, mas início  Investigar
                          provoca ANR             carregamento, memória e
                                                  dependências.

  Áudio WAV               PCM básico              Validar formatos,
                                                  mixagem, streaming e
                                                  consumo de memória.

  Save / record           Experimental            Definir formato,
                                                  integridade e
                                                  restauração de estado.

  Relíquias / maravilhas  Pendentes               Conteúdo de prioridade
                                                  posterior ao ciclo
                                                  principal.

  Multiplayer             Fora do MVP             Não deve competir com a
                                                  estabilização do modo
                                                  local.
  -----------------------------------------------------------------------

### 3.1 Leitura estratégica

O projeto apresenta sinais de avanço em infraestrutura de carregamento e
renderização, mas a maior lacuna parece estar na integração dos sistemas
que transformam uma cena renderizada em uma partida funcional.

O risco principal é investir em efeitos, conteúdo e recursos isolados
antes de existir um ciclo de jogo completo e testável.

A prioridade será construir primeiro uma partida mínima que possa ser
iniciada, jogada, vencida e encerrada de maneira reproduzível.

------------------------------------------------------------------------

## 4. Objetivo final e definição de MVP

### 4.1 Objetivo geral

Entregar uma versão nativa de AoM que:

1.  inicialize sem travamentos;
2.  carregue os dados e recursos necessários;
3.  permita iniciar uma partida local;
4.  apresente mapa, câmera, terreno, unidades e edifícios;
5.  permita selecionar unidades e emitir comandos por toque;
6.  permita coletar recursos e construir;
7.  permita treinar unidades;
8.  implemente combate funcional;
9.  reconheça vitória e derrota;
10. encerre e reinicie partidas sem corrupção de estado;
11. mantenha desempenho e consumo de memória compatíveis com o
    dispositivo Android de referência.

### 4.2 Definição do MVP jogável

O MVP não precisa reproduzir imediatamente todo o conteúdo do jogo
original. Precisa, porém, implementar um fluxo completo e coerente:

**Iniciar partida → carregar mapa → criar Centro da Cidade → selecionar
aldeão → coletar recurso → depositar recurso → construir edifício →
treinar unidade → selecionar exército → atacar inimigo → calcular
vitória ou derrota → exibir resultado.**

Esse fluxo é a principal referência de progresso. Sistemas que não
contribuem para ele podem ser adiados, salvo quando forem dependências
técnicas.

### 4.3 Fora do escopo inicial

Até o MVP estar estável, ficam em prioridade posterior:

-   multiplayer;
-   campanhas completas;
-   todos os poderes divinos;
-   todas as unidades e civilizações;
-   todos os mapas e cenários;
-   efeitos visuais avançados;
-   editor completo de mapas;
-   reprodução integral de gravações do jogo original.

Esses itens não estão descartados. Apenas não devem atrasar os
fundamentos de jogabilidade e estabilidade.

------------------------------------------------------------------------

## 5. Prioridades de desenvolvimento

### P0 --- Bloqueadores do MVP

  ---------------------------------------------------------------------------
  Item                    Trabalho necessário         Critério de conclusão
  ----------------------- --------------------------- -----------------------
  Inicialização           Identificar e corrigir ANR, Iniciar, entrar no menu
                          falhas de carregamento e    e iniciar partida em
                          dependências ausentes       testes repetidos sem
                                                      ANR.

  Centro da Cidade        Implementar entidade,       Centro da Cidade
                          construção/posicionamento   aparece, recebe
                          quando aplicável, vida,     comandos e participa do
                          seleção e interação         estado econômico.

  Economia                Recursos, coleta,           Recursos
                          transporte, depósito,       aumentam/diminuem
                          custos e população          corretamente e a
                                                      interface reflete o
                                                      estado real.

  Unidades                Seleção, movimento,         Unidades executam
                          comandos e estados básicos  ordens sem travar ou
                                                      perder referência.

  Construção              Construção, progresso,      Aldeão inicia e conclui
                          custos, trabalhadores e     uma construção válida.
                          conclusão                   

  Treinamento             Filas, custos, tempo,       Edifício produz unidade
                          população e criação de      e respeita regras.
                          unidades                    

  Combate                 Ataque, alcance, dano,      Unidades lutam e o
                          defesa, recarga e morte     estado de vida é
                                                      atualizado
                                                      corretamente.

  Vitória/derrota         Condições de fim de partida Partida termina
                          e transição de estado       corretamente, inclusive
                                                      no modo Conquest.
  ---------------------------------------------------------------------------

### P1 --- Jogabilidade e experiência

-   pathfinding robusto;
-   movimentação de grupos;
-   formações;
-   interface de toque;
-   câmera e minimapa;
-   animações básicas;
-   feedback visual de comandos;
-   gerenciamento de seleção;
-   pausa e velocidade de simulação;
-   salvamento local confiável.

### P2 --- Conteúdo e sistemas avançados

-   poderes divinos completos;
-   árvore tecnológica funcional;
-   IA adversária;
-   mapas procedurais mais completos;
-   relíquias;
-   maravilhas;
-   áudio completo;
-   efeitos de ambiente e combate.

### P3 --- Expansão

-   campanhas;
-   cenários especiais;
-   conteúdo avançado de civilizações;
-   replay completo;
-   ferramentas de modificação;
-   multiplayer, após arquitetura de rede e sincronização estarem
    justificadas.

------------------------------------------------------------------------

## 6. Arquitetura técnica recomendada

A arquitetura abaixo é uma proposta de organização. Deve ser adaptada ao
código existente, evitando reescrever componentes que já funcionem.

### 6.1 Princípios

1.  Separar simulação de renderização.
2.  Evitar que a interface altere diretamente regras internas do jogo.
3.  Centralizar regras de economia e combate.
4.  Manter entidades com identificadores estáveis.
5.  Separar carregamento de recursos da execução da partida.
6.  Tratar operações demoradas de forma assíncrona quando apropriado.
7.  Tornar os sistemas testáveis sem exigir sempre uma partida completa.
8.  Registrar erros e eventos importantes.
9.  Não duplicar a mesma regra em diferentes componentes.
10. Preservar compatibilidade com os dados originais sempre que
    tecnicamente viável.

### 6.2 Componentes sugeridos

  -----------------------------------------------------------------------
  Componente                          Responsabilidade
  ----------------------------------- -----------------------------------
  `GameDataManager`                   Carregar e validar dados de
                                      unidades, edifícios, tecnologias e
                                      civilizações.

  `AssetManager`                      Localizar, decodificar, armazenar
                                      em cache e liberar recursos.

  `AsyncLoadingManager`               Coordenar carregamentos demorados,
                                      progresso, cancelamento e erros.

  `GameState`                         Manter estado da partida,
                                      jogadores, mapa, entidades e
                                      resultado.

  `UnitManager`                       Criar, remover, selecionar e
                                      atualizar unidades.

  `BuildingManager`                   Posicionamento, construção, vida e
                                      estado de edifícios.

  `EconomySystem`                     Recursos, coleta, transporte,
                                      depósito, custos e população.

  `CombatSystem`                      Alvos, alcance, dano, armadura,
                                      bônus, recarga e morte.

  `PathfindingSystem`                 Caminhos, navegação, obstáculos e
                                      movimentação.

  `FormationSystem`                   Distribuição de unidades e
                                      manutenção de formações.

  `AnimationSystem`                   Estados de animação, esqueleto,
                                      transições e reprodução.

  `GodPowerSystem`                    Regras, alvos, custos, recargas e
                                      efeitos dos poderes.

  `AISystem`                          Planejamento, prioridades, produção
                                      e decisões do adversário.

  `SaveSystem`                        Serialização, versionamento e
                                      restauração da partida.

  `AudioSystem`                       Carregamento, reprodução, mixagem e
                                      liberação de áudio.

  `InputController`                   Converter gestos e toques em
                                      comandos de jogo.

  `UIController`                      Exibir informações e encaminhar
                                      ações à camada de comandos.

  `VictorySystem`                     Avaliar condições de vitória,
                                      derrota e encerramento.
  -----------------------------------------------------------------------

### 6.3 Simulação e taxa de quadros

A simulação do jogo não deve depender diretamente da taxa de quadros da
tela.

Recomenda-se:

-   passo fixo de simulação;
-   renderização independente;
-   acumulação controlada de tempo;
-   limite de atualizações atrasadas por quadro;
-   política clara para pausa e retomada;
-   testes com taxas de quadros diferentes.

Isso evita que unidades se movam, ataquem ou coletem recursos em
velocidades diferentes conforme o desempenho do dispositivo.

### 6.4 Sistema de eventos

Eventos podem desacoplar os sistemas. Exemplos:

-   `UnitCreated`;
-   `UnitSelected`;
-   `MoveCommandIssued`;
-   `ResourceCollected`;
-   `ResourceDeposited`;
-   `BuildingStarted`;
-   `BuildingCompleted`;
-   `TrainingQueued`;
-   `UnitTrained`;
-   `AttackStarted`;
-   `EntityDamaged`;
-   `EntityDestroyed`;
-   `TechnologyResearched`;
-   `GodPowerActivated`;
-   `VictoryAchieved`;
-   `DefeatAchieved`.

Os eventos devem ter contratos definidos, evitar ciclos de dependência e
não substituir validações de regras.

------------------------------------------------------------------------

## 7. Plano detalhado por subsistema

### 7.1 Inicialização e estabilidade

**Problema:** o relatório registra campanhas detectadas cujo início
resulta em ANR. O comportamento exato e sua causa ainda precisam ser
confirmados no código e em execução.

**Investigações:**

1.  mapear a sequência de inicialização;
2.  registrar tempo gasto em cada etapa;
3.  identificar operações síncronas na thread principal;
4.  medir alocação de memória;
5.  verificar decodificação de texturas e modelos;
6.  identificar loops de espera e deadlocks;
7.  verificar dependências ausentes;
8.  separar erro de recurso ausente de erro de parser;
9.  adicionar logs com identificador de etapa e recurso;
10. reproduzir o problema com o mesmo arquivo/cenário.

**Critérios de aceitação:**

-   menu inicial abre sem ANR;
-   partida de teste carrega;
-   falhas de recurso são reportadas com contexto;
-   carregamento pode apresentar progresso;
-   operações canceláveis não deixam estado parcialmente inicializado;
-   memória retorna a níveis esperados após sair da partida.

**Evidências:** log completo, tempo de carregamento, memória máxima,
versão do build, dispositivo e passos para reproduzir.

### 7.2 Centro da Cidade e economia

O Centro da Cidade é central para o ciclo econômico. A economia deve ser
implementada como um sistema coerente, não como contadores independentes
na interface.

**Entidades e dados necessários:**

-   jogador proprietário;
-   tipo de recurso;
-   quantidade disponível;
-   capacidade de transporte;
-   aldeão trabalhador;
-   recurso coletável;
-   ponto de depósito;
-   custo de construção;
-   custo de treinamento;
-   limite populacional;
-   fila de produção;
-   progresso de construção.

**Fluxo de coleta:**

1.  jogador seleciona aldeão;
2.  jogador escolhe recurso ou alvo;
3.  sistema valida distância e acessibilidade;
4.  aldeão encontra caminho;
5.  aldeão coleta durante determinado intervalo;
6.  inventário de transporte é atualizado;
7.  aldeão procura ponto de depósito válido;
8.  caminho até o depósito é calculado;
9.  recurso é creditado ao jogador;
10. aldeão retoma a tarefa ou aguarda nova ordem.

**Regras que precisam ser testadas:**

-   não coletar recurso inexistente;
-   não ultrapassar capacidade de transporte;
-   não creditar recurso duas vezes;
-   não perder recurso ao cancelar ordem;
-   não depositar para jogador incorreto;
-   não permitir custo negativo;
-   não permitir treinamento sem recursos suficientes;
-   respeitar limite populacional;
-   tratar destruição do ponto de depósito durante o trajeto.

**Critérios de aceitação:** realizar pelo menos dez ciclos de coleta e
depósito em sequência, comparar os valores esperados com os valores
exibidos e verificar se não há duplicação ou perda indevida.

### 7.3 Construção e edifícios

**Funcionalidades necessárias:**

-   seleção do edifício a construir;
-   verificação de custo;
-   verificação de terreno;
-   verificação de colisão e espaço;
-   posicionamento por toque;
-   confirmação/cancelamento;
-   atribuição de trabalhador;
-   progresso de construção;
-   conclusão;
-   vida e dano;
-   destruição e remoção;
-   atualização da população ou capacidades quando aplicável.

**Testes:**

-   construção em terreno válido;
-   tentativa em terreno inválido;
-   tentativa sobre outro edifício;
-   cancelamento antes do início;
-   cancelamento durante a construção;
-   trabalhador interrompido;
-   edifício destruído antes da conclusão;
-   salvamento e carregamento durante a construção.

### 7.4 Treinamento e tecnologias

**Treinamento:**

-   validar requisitos do edifício;
-   validar recursos;
-   validar população;
-   manter fila;
-   calcular progresso;
-   cancelar item;
-   criar unidade;
-   encontrar ponto de saída;
-   impedir criação dentro de obstáculos;
-   persistir fila em save/load.

**Tecnologias:**

-   requisitos por idade;
-   requisitos de edifício;
-   custo;
-   duração;
-   efeitos;
-   aplicação aos objetos existentes;
-   aplicação às unidades criadas posteriormente;
-   impedir pesquisa duplicada quando não permitida.

A interface deve refletir o estado real do sistema e não apenas
apresentar botões visualmente ativos.

### 7.5 Combate

Recomenda-se um `CombatSystem` centralizado para evitar regras
divergentes entre unidades.

**Dados essenciais:**

-   atacante e alvo;
-   tipo de ataque;
-   alcance;
-   linha de visão, se aplicável;
-   tempo de recarga;
-   dano base;
-   tipo de dano;
-   armadura;
-   bônus contra classes;
-   modificadores tecnológicos;
-   modificadores de poderes;
-   projétil, quando aplicável;
-   estado de vida;
-   proprietário.

**Fluxo básico:**

1.  receber ordem de ataque;
2.  validar alvo;
3.  verificar alcance;
4.  mover até posição de ataque se necessário;
5.  iniciar animação/ação;
6.  aguardar recarga;
7.  calcular dano;
8.  aplicar modificadores;
9.  atualizar vida;
10. emitir evento de dano;
11. tratar morte;
12. verificar condições de vitória.

**Testes mínimos:**

-   ataque corpo a corpo;
-   ataque à distância;
-   alvo fora de alcance;
-   alvo destruído durante perseguição;
-   atacante destruído durante ataque;
-   dano com armadura;
-   bônus contra classe;
-   edifício como alvo;
-   projétil que não encontra alvo;
-   combate com vários atacantes;
-   pausa e retomada durante combate.

### 7.6 Pathfinding e movimentação

O A\* descrito como protótipo precisa ser avaliado em mapas reais e com
obstáculos dinâmicos.

**Evolução recomendada:**

-   definir grade ou representação espacial;
-   definir custo de terreno;
-   distinguir células bloqueadas e transitáveis;
-   validar início e destino;
-   evitar obstáculos estáticos;
-   reagir a unidades e edifícios;
-   recalcular caminho quando necessário;
-   evitar recalcular em excesso;
-   suavizar trajetórias;
-   tratar grupos;
-   impedir que unidades fiquem presas;
-   definir comportamento quando não há rota.

**Testes objetivos:**

-   caminho livre;
-   obstáculo único;
-   corredor estreito;
-   destino bloqueado;
-   obstáculo móvel;
-   grupo grande;
-   unidades em sentidos opostos;
-   mapa com áreas desconectadas;
-   rota longa;
-   ordem repetida durante movimento.

Registrar comprimento do caminho, tempo de cálculo, quantidade de nós
visitados e número de recálculos.

### 7.7 Formações e grupos

Formações dependem de seleção múltipla e pathfinding confiável.

**Etapas:**

1.  selecionar várias unidades;
2.  identificar tipos e velocidades;
3.  calcular posição de destino por unidade;
4.  manter espaçamento;
5.  evitar sobreposição;
6.  preservar a formação durante o movimento;
7.  tratar obstáculos e corredores;
8.  tratar unidades destruídas ou adicionadas ao grupo.

Não implementar formações avançadas antes de o movimento individual
estar estável.

### 7.8 Animações Granny

O relatório indica que os dados de animação Granny não são interpretados
pelo runtime. Isso deve ser tratado como uma lacuna técnica específica,
não como um problema meramente visual.

**Caminho de investigação sugerido:**

1.  inventariar arquivos e versões de recursos;
2.  identificar formatos e estruturas efetivamente presentes;
3.  localizar ferramentas de extração/conversão compatíveis;
4.  verificar se a conversão preserva esqueleto, hierarquia, pesos,
    materiais e keyframes;
5.  escolher um formato intermediário adequado ao renderizador;
6.  implementar carregamento do esqueleto;
7.  implementar interpolação de keyframes;
8.  implementar mistura/transição de estados;
9.  conectar animação ao estado lógico da unidade.

A conversão por ferramenta externa, por exemplo para glTF quando
compatível, é uma hipótese de pipeline a ser validada. Não assumir que
todos os arquivos Granny podem ser convertidos sem perda.

**Estados mínimos:** idle, walk, attack, hit, death.

**Critérios de aceitação:**

-   esqueleto e hierarquia corretos;
-   animação sem deformação inesperada;
-   transição entre estados;
-   sincronização com movimento e combate;
-   desempenho aceitável em várias unidades simultâneas.

### 7.9 Renderização GLES3 e mapa

Embora o renderizador GLES3 seja descrito como concluído, é necessário
avaliar a qualidade e a estabilidade em cenários completos.

**Checklist:**

-   terreno;
-   elevação;
-   texturas;
-   água;
-   árvores e florestas;
-   edifícios;
-   unidades;
-   sombras;
-   iluminação;
-   transparência;
-   partículas;
-   efeitos de combate;
-   fog of war;
-   minimapa;
-   câmera;
-   zoom;
-   rotação, se prevista;
-   descarte de objetos fora da área visível;
-   gerenciamento de memória gráfica.

Priorizar correção visual e estabilidade antes de efeitos avançados.
Medir FPS, tempo de quadro, uso de memória e travamentos em mapas
pequenos, médios e grandes.

### 7.10 Interface e controles de toque

A interface deve ser desenhada para tela sensível ao toque, sem depender
de comandos de mouse ou teclado.

**Controles mínimos:**

-   toque simples para selecionar;
-   toque no terreno para mover;
-   toque em alvo para interagir/atacar;
-   seleção por área, se viável;
-   painel contextual;
-   botões de construir e treinar;
-   botão de cancelar;
-   minimapa;
-   controle de câmera;
-   pausa;
-   feedback de comando;
-   indicação de recurso, população e idade.

**Requisitos de usabilidade:**

-   alvos de toque suficientemente grandes;
-   evitar botões próximos que causem comandos acidentais;
-   feedback visual imediato;
-   distinguir seleção, destino e alvo;
-   permitir cancelar ordens;
-   evitar bloquear a visão do mapa;
-   manter legibilidade em diferentes resoluções;
-   considerar orientação e proporção da tela.

### 7.11 IA

A IA deve entrar depois de o ciclo de economia, construção, treinamento
e combate estar estável.

**Etapas sugeridas:**

1.  IA passiva para testes;
2.  coleta de recursos;
3.  construção básica;
4.  treinamento;
5.  defesa;
6.  ataque;
7.  reação a ameaças;
8.  planejamento por idade e tecnologia;
9.  avaliação de força e risco;
10. dificuldade configurável.

Separar planejamento de execução. A IA deve emitir ordens usando as
mesmas interfaces disponíveis ao jogador, sempre que possível. Isso
evita regras especiais que não são testadas pelo jogo normal.

### 7.12 Poderes divinos

O carregamento parcial de dados não equivale a um sistema de poderes
completo.

Cada poder deverá declarar:

-   requisitos;
-   custo;
-   recarga;
-   alvo;
-   área de efeito;
-   duração;
-   efeitos;
-   restrições;
-   interação com unidades e edifícios;
-   feedback visual e sonoro;
-   persistência ou não após save/load.

Implementar primeiro um poder simples ponta a ponta e só depois ampliar
o catálogo.

### 7.13 Áudio

O suporte básico a WAV PCM precisa ser caracterizado.

**Verificar:**

-   frequência de amostragem;
-   canais;
-   profundidade de bits;
-   formatos realmente usados pelos recursos;
-   streaming versus carregamento integral;
-   mixagem;
-   volume por categoria;
-   interrupção e liberação;
-   consumo de memória;
-   comportamento em segundo plano.

Não declarar suporte geral a áudio com base em um único formato testado.

### 7.14 Salvamento e restauração

O sistema de save deve preservar o estado lógico, não apenas a aparência
da cena.

**Dados que podem precisar ser persistidos:**

-   versão do formato;
-   mapa e cenário;
-   jogadores;
-   recursos;
-   população;
-   entidades;
-   posições;
-   vida;
-   ordens;
-   filas de produção;
-   construções em andamento;
-   tecnologias;
-   poderes e recargas;
-   relógio da simulação;
-   condição de vitória;
-   estado aleatório necessário para reproduzir o jogo.

**Critérios:**

-   salvar em momento seguro;
-   validar arquivo antes de carregar;
-   tratar arquivo corrompido;
-   compatibilidade entre versões ou migração;
-   não duplicar entidades;
-   preservar economia;
-   preservar filas e construções;
-   restaurar estado de vitória/derrota;
-   teste de save/load repetido.

### 7.15 Campanhas, relíquias e maravilhas

São sistemas de conteúdo importantes, mas devem ser adicionados depois
da estabilidade do modo local básico.

Para campanhas, investigar o ANR separadamente do carregamento de
partidas skirmish. Cenários podem exigir scripts, gatilhos, diálogos,
objetivos e recursos específicos.

Relíquias e maravilhas precisam ser integradas às regras de vitória,
economia, objetivos e interface. Não devem ser apenas objetos
renderizados no mapa.

### 7.16 Multiplayer

Multiplayer fica fora do MVP inicial.

Antes de começar, será necessário definir:

-   autoridade da simulação;
-   sincronização determinística ou modelo equivalente;
-   protocolo;
-   latência;
-   reconexão;
-   validação de comandos;
-   segurança;
-   versão de conteúdo;
-   tratamento de divergência de estado;
-   lobby e descoberta de partidas.

A decisão deve ser tomada com base na arquitetura real e no custo de
manutenção, não apenas na possibilidade de conectar dois dispositivos.

------------------------------------------------------------------------

## 8. Roteiro de execução por sprints

As sprints abaixo são uma sequência sugerida, não uma promessa de
duração. A duração real depende do tamanho do código, da disponibilidade
de ferramentas e dos bloqueios encontrados.

### Sprint 0 --- Auditoria reproduzível

**Objetivo:** conhecer o estado real do repositório.

Tarefas:

-   obter e inventariar o código-fonte;
-   identificar linguagem, build system e dependências;
-   documentar estrutura de diretórios;
-   localizar pontos de entrada;
-   localizar carregador de recursos;
-   identificar renderizador;
-   localizar sistema de entidades;
-   localizar sistema de input;
-   identificar testes existentes;
-   compilar a versão atual;
-   executar o build disponível;
-   registrar warnings e erros.

**Entrega:** mapa do projeto, instruções de build, matriz de
dependências e relatório de baseline.

### Sprint 1 --- Estabilidade e inicialização

**Objetivo:** iniciar menu e partida de teste de forma previsível.

Tarefas:

-   reproduzir ANR;
-   instrumentar carregamento;
-   remover bloqueios da thread principal;
-   validar recursos;
-   tratar erros;
-   medir memória;
-   criar teste repetido de inicialização.

**Entrega:** inicialização estável e logs úteis.

### Sprint 2 --- Estado da partida e Centro da Cidade

**Objetivo:** criar uma partida mínima com estado consistente.

Tarefas:

-   formalizar `GameState`;
-   criar entidades essenciais;
-   implementar Centro da Cidade;
-   implementar jogador e propriedade;
-   definir ciclo de atualização;
-   implementar seleção básica.

**Entrega:** partida carregada com Centro da Cidade selecionável.

### Sprint 3 --- Economia e aldeões

**Objetivo:** completar coleta e depósito.

Tarefas:

-   recursos;
-   aldeões;
-   ordens;
-   coleta;
-   transporte;
-   depósito;
-   custos;
-   população;
-   interface econômica.

**Entrega:** ciclo de coleta e depósito validado.

### Sprint 4 --- Construção e treinamento

**Objetivo:** permitir expansão e produção de unidades.

Tarefas:

-   posicionamento;
-   validação de terreno;
-   construção;
-   progresso;
-   edifícios de produção;
-   filas;
-   treinamento;
-   requisitos tecnológicos básicos.

**Entrega:** construir um edifício e treinar uma unidade.

### Sprint 5 --- Combate e vitória

**Objetivo:** completar o ciclo jogável.

Tarefas:

-   ataque;
-   dano;
-   armadura;
-   recarga;
-   morte;
-   destruição de edifícios;
-   condições de vitória e derrota;
-   correção do bug Conquest.

**Entrega:** partida local que pode ser vencida e encerrada.

### Sprint 6 --- Movimento, pathfinding e formações

**Objetivo:** tornar comandos confiáveis em mapas reais.

Tarefas:

-   melhorar A\*;
-   obstáculos dinâmicos;
-   recálculo;
-   grupos;
-   espaçamento;
-   formações básicas;
-   prevenção de unidades presas.

**Entrega:** movimentação de unidades individuais e grupos validada.

### Sprint 7 --- Animações e apresentação visual

**Objetivo:** conectar estados lógicos a animações.

Tarefas:

-   investigar pipeline Granny;
-   testar conversão;
-   implementar esqueleto;
-   idle/walk/attack/death;
-   transições;
-   otimização de renderização.

**Entrega:** unidades animadas em combate e movimento.

### Sprint 8 --- Interface Android e otimização

**Objetivo:** tornar o jogo confortável no dispositivo-alvo.

Tarefas:

-   gestos;
-   seleção por toque;
-   painéis contextuais;
-   câmera;
-   minimapa;
-   escalas de interface;
-   otimização de CPU/GPU/memória;
-   testes de sessão longa.

**Entrega:** experiência de controle funcional e estável no Android.

### Sprint 9 --- IA e conteúdo

**Objetivo:** aumentar a qualidade da partida.

Tarefas:

-   IA econômica;
-   produção;
-   defesa;
-   ataque;
-   tecnologias;
-   poderes divinos;
-   áudio;
-   mapas e conteúdo adicional.

**Entrega:** adversário local funcional e conteúdo prioritário
integrado.

### Sprint 10 --- Persistência e robustez

**Objetivo:** garantir continuidade e confiabilidade.

Tarefas:

-   save/load;
-   validação;
-   migração;
-   recuperação de erro;
-   testes prolongados;
-   regressão completa.

**Entrega:** partida salva e restaurada sem perda crítica de estado.

### Sprint 11 --- Campanhas e expansão

**Objetivo:** ampliar o escopo depois do MVP.

Tarefas:

-   investigar scripts de campanha;
-   resolver ANR específico;
-   implementar objetivos;
-   diálogos e gatilhos;
-   relíquias;
-   maravilhas;
-   cenários especiais.

**Entrega:** primeiro cenário de campanha validado.

### Sprint 12 --- Preparação para distribuição

**Objetivo:** preparar uma versão consistente para uso externo.

Tarefas:

-   matriz de dispositivos;
-   compatibilidade Android;
-   permissões;
-   armazenamento;
-   instalação e atualização;
-   logs de falha;
-   documentação de limitações;
-   checklist de release.

**Entrega:** build de release testado e documentação de instalação.

------------------------------------------------------------------------

## 9. Estratégia de testes

### 9.1 Pirâmide de testes

1.  **Testes unitários:** regras isoladas de economia, dano, custos e
    estados.
2.  **Testes de integração:** comunicação entre sistemas.
3.  **Testes de cenário:** sequências de jogo reproduzíveis.
4.  **Testes de dispositivo:** renderização, toque, memória e
    desempenho.
5.  **Testes de regressão:** verificar se correções anteriores continuam
    funcionando.

### 9.2 Cenários mínimos de regressão

  -----------------------------------------------------------------------
  ID                      Cenário                 Resultado esperado
  ----------------------- ----------------------- -----------------------
  T01                     Abrir aplicativo        Menu inicial aparece
                                                  sem falha.

  T02                     Iniciar partida         Mapa e entidades
                                                  carregam.

  T03                     Selecionar Centro da    Seleção e painel
                          Cidade                  corretos.

  T04                     Selecionar aldeão       Unidade selecionada
                                                  corretamente.

  T05                     Coletar recurso         Quantidade coletada é
                                                  correta.

  T06                     Depositar recurso       Saldo do jogador é
                                                  atualizado uma única
                                                  vez.

  T07                     Construir edifício      Custo e progresso
                                                  corretos.

  T08                     Treinar unidade         Fila, custo e população
                                                  corretos.

  T09                     Mover unidade           Unidade chega ao
                                                  destino ou reporta
                                                  impossibilidade.

  T10                     Atacar inimigo          Dano e recarga
                                                  corretos.

  T11                     Destruir alvo           Entidade removida e
                                                  eventos emitidos.

  T12                     Condição de vitória     Partida termina
                                                  corretamente.

  T13                     Salvar e carregar       Estado restaurado sem
                                                  duplicação.

  T14                     Pausar e retomar        Simulação respeita
                                                  pausa.

  T15                     Sessão prolongada       Sem crescimento anormal
                                                  de memória ou
                                                  travamento.
  -----------------------------------------------------------------------

### 9.3 Testes de desempenho

Registrar, no mínimo:

-   FPS médio;
-   1% low ou métrica equivalente, se disponível;
-   tempo de quadro;
-   tempo de carregamento;
-   memória RAM máxima;
-   memória gráfica, quando mensurável;
-   temperatura e throttling, quando disponíveis;
-   número de unidades;
-   tamanho do mapa;
-   duração do teste.

Não estabelecer metas numéricas definitivas sem medir primeiro o
dispositivo e o build de referência. Após a baseline, definir metas
realistas e comparar versões sob as mesmas condições.

### 9.4 Testes de estabilidade

Executar:

-   dez inicializações consecutivas;
-   dez entradas e saídas de partida;
-   sessão prolongada;
-   alternância entre pausa e retomada;
-   mudança de aplicativo e retorno;
-   rotação/orientação, se suportada;
-   teste com memória pressionada;
-   carregamento de recursos ausentes ou inválidos.

Todo teste deve registrar build, dispositivo, sistema operacional,
passos, resultado e log.

------------------------------------------------------------------------

## 10. Ferramentas, formatos e dependências

O relatório técnico menciona ferramentas e bibliotecas relacionadas à
exploração e conversão de recursos, incluindo referências a AME,
AoMEngineLibrary e conversores. Essas referências devem ser avaliadas
antes de se tornarem dependências obrigatórias.

Para cada ferramenta:

-   confirmar origem e licença;
-   confirmar compatibilidade com os formatos usados;
-   testar com arquivos reais do projeto;
-   documentar versão;
-   registrar comandos;
-   verificar se a saída preserva dados;
-   automatizar apenas após validar o processo manual.

### 10.1 Inventário de recursos

Criar uma planilha ou arquivo de inventário com:

-   nome do arquivo;
-   extensão/formato;
-   tamanho;
-   categoria;
-   dependências;
-   ferramenta de leitura;
-   ferramenta de conversão;
-   estado do parser;
-   estado no runtime;
-   evidência de teste;
-   observações.

### 10.2 Dependências e DLLs

O relatório técnico apresenta uma questão que precisa ser esclarecida: a
seção sobre `aomac.dll` descreve sua importância como componente
central/obrigatório, enquanto a análise de imports indica que
`ShellExecuteA` é explicitamente identificado e que outras funções podem
ser obtidas por mecanismos dinâmicos, como `GetProcAddress`.

Essas afirmações não devem ser tratadas como equivalentes. É necessário:

1.  verificar a tabela de imports completa;
2.  procurar referências dinâmicas;
3.  identificar chamadas em execução, se possível;
4.  verificar dependências transitivas;
5.  separar dependência do executável original de dependência do port
    nativo.

A existência de uma DLL no ambiente original não prova que o port
precise carregá-la.

------------------------------------------------------------------------

## 11. Riscos e medidas de mitigação

  -----------------------------------------------------------------------
  Risco                   Consequência            Mitigação
  ----------------------- ----------------------- -----------------------
  Diagnóstico baseado     Conclusões incorretas   Auditoria do código e
  apenas em análise       sobre o runtime         testes reproduzíveis.
  estática                                        

  Ausência de             Impossibilidade de      Solicitar
  código-fonte completo   confirmar implementação repositório/ZIP e
                                                  mapear arquivos.

  Sistemas prototipados   Jogo parece avançado,   Priorizar vertical
  não integrados          mas não é jogável       slice ponta a ponta.

  ANR no carregamento     Partidas/campanhas não  Instrumentar etapas e
                          iniciam                 operações assíncronas.

  Formatos proprietários  Recursos não carregam   Inventário e testes de
                          ou perdem dados         conversão.

  Animação incompatível   Unidades sem animação   Pipeline validado com
                          ou deformadas           poucos assets antes de
                                                  escalar.

  Pathfinding instável    Unidades presas ou      Testes de obstáculos e
                          comportamento           grupos.
                          imprevisível            

  Simulação acoplada ao   Velocidade              Passo fixo de
  FPS                     inconsistente           simulação.

  Economia duplicada em   Recursos incorretos     `EconomySystem`
  várias classes                                  centralizado.

  Interface ligada        Bugs e difícil          Camada de
  diretamente à lógica    manutenção              comandos/eventos.

  Escopo excessivo        Atraso do MVP           Prioridades P0--P3 e
                                                  critérios de entrada.

  Otimização prematura    Retrabalho              Medir baseline e
                                                  otimizar gargalos
                                                  reais.

  Save sem versionamento  Arquivos incompatíveis  Versão do formato e
                                                  migração.

  Multiplayer antecipado  Complexidade alta antes Adiar até modo local
                          da base                 estável.
  -----------------------------------------------------------------------

------------------------------------------------------------------------

## 12. Processo de trabalho recomendado

### 12.1 Ciclo de desenvolvimento

Para cada funcionalidade:

1.  **Definir:** descrever o comportamento esperado.
2.  **Localizar:** identificar arquivos e sistemas envolvidos.
3.  **Investigar:** entender o comportamento atual e dependências.
4.  **Planejar:** definir menor alteração coerente.
5.  **Implementar:** alterar o código com escopo controlado.
6.  **Compilar:** verificar erros e warnings.
7.  **Testar isoladamente:** validar a regra.
8.  **Integrar:** testar com os sistemas dependentes.
9.  **Executar:** validar no jogo e no dispositivo, quando aplicável.
10. **Registrar:** salvar logs, capturas e resultados.
11. **Revisar regressões:** executar cenários anteriores.
12. **Atualizar documentação:** registrar o que mudou e o que permanece
    pendente.

### 12.2 Regras para alterações

-   Uma mudança deve resolver um problema identificável.
-   Evitar misturar refatoração ampla e correção funcional no mesmo
    passo.
-   Preservar comportamento que já foi validado.
-   Não substituir APIs sem verificar todos os usos.
-   Registrar arquivos alterados e motivo.
-   Manter instruções de compilação atualizadas.
-   Não remover logs úteis antes da estabilização.
-   Evitar "correções" que apenas escondam erros.
-   Se um teste falhar, registrar a regressão e interromper a expansão
    da mesma área até compreender a causa.

### 12.3 Definição de pronto

Uma tarefa só pode ser marcada como validada quando:

-   comportamento esperado está descrito;
-   implementação foi localizada;
-   compilação foi bem-sucedida;
-   testes aplicáveis passaram;
-   resultado foi observado;
-   logs/evidências foram guardados;
-   regressões relevantes foram verificadas;
-   documentação foi atualizada;
-   limitações conhecidas foram registradas.

------------------------------------------------------------------------

## 13. Acompanhamento semanal

O acompanhamento semanal deve comparar o estado atual com o último
relatório, sem inventar progresso quando não houver novos materiais.

### 13.1 Conteúdo obrigatório do relatório

1.  **Resumo executivo:** situação geral e principal mudança.
2.  **Novidades:** commits, arquivos, funcionalidades ou testes novos.
3.  **Comparação com a semana anterior:** o que avançou, regrediu ou
    permaneceu igual.
4.  **Estado por prioridade:** P0, P1, P2 e P3.
5.  **Evidências:** código, logs, testes, capturas ou documentação.
6.  **Bloqueios:** problema, impacto e dependência.
7.  **Riscos:** riscos novos ou agravados.
8.  **Próximas três ações:** tarefas concretas e ordenadas.
9.  **Testes necessários:** como comprovar cada ação.
10. **Decisões pendentes:** perguntas técnicas que exigem confirmação.

### 13.2 Regra quando não houver novos arquivos

Se não houver novos materiais acessíveis:

-   informar que não foi possível verificar alterações desde o último
    acompanhamento;
-   manter o último estado documentado;
-   não declarar que o projeto ficou parado;
-   não declarar que houve progresso;
-   indicar quais arquivos, logs ou commits seriam necessários para
    atualizar o diagnóstico.

### 13.3 Formato de status

  Área              Status                  Evidência              Próxima ação
  ----------------- ----------------------- ---------------------- --------------------
  Inicialização     Pendente de validação   Log/build              Reproduzir ANR
  Economia          Parcial                 Código/teste           Completar depósito
  Combate           Pendente de validação   Teste de cenário       Validar dano
  Animações         Em investigação         Assets/conversor       Provar pipeline
  Interface touch   Parcial                 Build no dispositivo   Testar seleção

Essa tabela é apenas um modelo; o relatório deve refletir os dados
realmente disponíveis em cada execução.

------------------------------------------------------------------------

## 14. Informações necessárias para uma auditoria completa do código

Para transformar este plano em um roteiro diretamente ligado aos
arquivos do projeto, é necessário disponibilizar:

-   código-fonte completo ou repositório;
-   instruções de compilação;
-   arquivos de configuração;
-   lista de dependências;
-   logs de inicialização;
-   logs de ANR/crash;
-   build atual, se disponível;
-   modelo do dispositivo Android;
-   versão do Android;
-   passos exatos para reproduzir os problemas;
-   lista do que já funciona em execução;
-   lista do que não funciona;
-   capturas de tela ou gravações curtas dos testes.

O executável Windows e a documentação de análise são úteis para
compreender o jogo original, mas não substituem o código do port.

### 14.1 O que a auditoria deverá produzir

Com o código-fonte disponível, a auditoria deverá gerar:

1.  árvore real do projeto;
2.  mapa de módulos e dependências;
3.  inventário de classes e sistemas;
4.  identificação de código morto ou duplicado;
5.  análise de pontos de entrada;
6.  análise do carregamento de assets;
7.  análise do loop de jogo;
8.  análise de input e renderização;
9.  análise de memória e operações bloqueantes;
10. lista de bugs reproduzíveis;
11. matriz entre funcionalidades documentadas e implementadas;
12. plano de correções por arquivo;
13. testes automatizáveis;
14. roteiro de compilação e validação.

------------------------------------------------------------------------

## 15. Decisões técnicas ainda abertas

As decisões abaixo devem ser tomadas com base no código real, não por
suposição:

-   qual é a linguagem e arquitetura efetiva do port;
-   qual sistema de entidades já existe;
-   qual é a representação espacial do mapa;
-   como os recursos originais são decodificados;
-   quais formatos Granny aparecem realmente nos assets;
-   se a conversão para glTF preserva os dados necessários;
-   como o estado da partida é serializado;
-   qual é o modelo de simulação temporal;
-   como a interface comunica comandos à simulação;
-   quais dispositivos Android são alvos oficiais;
-   qual é o orçamento de memória e desempenho;
-   qual é a situação real do modo Conquest;
-   se o ANR afeta apenas campanhas ou também partidas normais;
-   quais dependências do executável original são realmente necessárias
    ao port.

Cada decisão deverá registrar: contexto, opções consideradas, escolha,
justificativa, consequências e data.

------------------------------------------------------------------------

## 16. Indicadores de progresso

Evitar medir progresso apenas por quantidade de arquivos, linhas de
código ou telas criadas.

Indicadores mais úteis:

-   quantidade de cenários de regressão aprovados;
-   quantidade de sistemas P0 validados;
-   taxa de inicializações bem-sucedidas;
-   tempo de carregamento;
-   número de ANRs/crashes reproduzíveis;
-   percentual do ciclo de jogo validado;
-   número de unidades simultâneas testadas;
-   FPS e tempo de quadro;
-   memória máxima;
-   taxa de sucesso de save/load;
-   quantidade de recursos com parser e runtime validados.

O indicador mais importante do MVP é a proporção do ciclo de jogo que
pode ser executada de ponta a ponta sem intervenção externa ou falha.

------------------------------------------------------------------------

## 17. Checklist mestre do projeto

### Auditoria e estabilidade

-   [ ] Código-fonte completo obtido.
-   [ ] Build reproduzível documentado.
-   [ ] Dependências inventariadas.
-   [ ] Inicialização instrumentada.
-   [ ] ANR reproduzido e causa identificada.
-   [ ] Carregamento de mapa validado.
-   [ ] Gestão de memória medida.

### Núcleo de partida

-   [ ] Estado da partida definido.
-   [ ] Jogadores e propriedade definidos.
-   [ ] Centro da Cidade implementado.
-   [ ] Seleção de unidades validada.
-   [ ] Comandos básicos validados.
-   [ ] Pausa e retomada validadas.

### Economia

-   [ ] Recursos implementados.
-   [ ] Coleta implementada.
-   [ ] Transporte implementado.
-   [ ] Depósito implementado.
-   [ ] Custos implementados.
-   [ ] População implementada.
-   [ ] Interface econômica conectada ao estado real.

### Construção e treinamento

-   [ ] Posicionamento validado.
-   [ ] Colisão e terreno validados.
-   [ ] Construção com progresso.
-   [ ] Edifícios de produção.
-   [ ] Filas de treinamento.
-   [ ] Requisitos e custos.
-   [ ] Criação de unidade em posição válida.

### Combate e vitória

-   [ ] Ataque corpo a corpo.
-   [ ] Ataque à distância.
-   [ ] Dano e armadura.
-   [ ] Bônus e modificadores.
-   [ ] Morte e destruição.
-   [ ] Vitória e derrota.
-   [ ] Bug de Conquest corrigido e validado.

### Movimento e apresentação

-   [ ] Pathfinding em terreno livre.
-   [ ] Pathfinding com obstáculos.
-   [ ] Replanejamento de rota.
-   [ ] Grupos.
-   [ ] Formações.
-   [ ] Animações básicas.
-   [ ] Câmera e minimapa.
-   [ ] Renderização validada em mapa completo.

### Android e robustez

-   [ ] Controles de toque.
-   [ ] Layout adaptável.
-   [ ] Feedback visual.
-   [ ] Desempenho medido.
-   [ ] Sessão prolongada testada.
-   [ ] Save/load validado.
-   [ ] Testes de regressão automatizados ou documentados.

### Conteúdo avançado

-   [ ] IA econômica.
-   [ ] IA militar.
-   [ ] Tecnologias.
-   [ ] Poderes divinos.
-   [ ] Áudio ampliado.
-   [ ] Relíquias.
-   [ ] Maravilhas.
-   [ ] Campanha.
-   [ ] Multiplayer avaliado separadamente.

------------------------------------------------------------------------

## 18. Próximas ações imediatas recomendadas

A ordem mais segura para começar é:

### Ação 1 --- Fazer auditoria do código-fonte real

**Por quê:** o diagnóstico atual é principalmente documental/estático.
Sem o código do port, não é possível afirmar quais componentes estão
implementados e integrados.

**Resultado esperado:** mapa de módulos, estado real dos sistemas e
lista de pontos de entrada para correções.

### Ação 2 --- Reproduzir e corrigir o problema de inicialização

**Por quê:** qualquer sistema adicional fica difícil de validar se o
aplicativo não inicia ou se o carregamento bloqueia a interface.

**Resultado esperado:** sequência de inicialização mensurável, logs
úteis e teste repetível.

### Ação 3 --- Implementar o vertical slice do jogo

**Por quê:** é a forma mais rápida de descobrir falhas de integração
entre entidades, economia, construção, treinamento, combate e vitória.

**Resultado esperado:** uma partida local mínima que começa, permite
jogar e termina corretamente.

------------------------------------------------------------------------

## 19. Registro de alterações deste documento

  -----------------------------------------------------------------------
  Versão                  Data                    Alteração
  ----------------------- ----------------------- -----------------------
  1.0                     05/11/2026              Criação do processo de
                                                  evolução,
                                                  diagnóstico-base,
                                                  prioridades,
                                                  arquitetura, roadmap,
                                                  testes e acompanhamento
                                                  semanal.

  -----------------------------------------------------------------------

------------------------------------------------------------------------

## 20. Observação final

Este documento é um plano de trabalho vivo. Deve ser atualizado sempre
que houver mudança comprovada no código, nos testes, nos recursos ou nas
decisões de arquitetura.

A regra que orienta todo o desenvolvimento é:

> **Primeiro estabilidade; depois um ciclo de jogo completo; em seguida
> qualidade, conteúdo e expansão.**

Um port que abre, renderiza modelos ou carrega recursos ainda não é
necessariamente um jogo jogável. O progresso real será medido pela
capacidade de executar uma partida completa, com regras consistentes,
controles funcionais, desempenho aceitável e resultados reproduzíveis.
