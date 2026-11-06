# Pasta: tests/reference/databar

## Descrição
Esta pasta contém arquivos XML de interface do usuário (UI) do Age of Mythology original, extraídos dos arquivos BAR do jogo. Estes arquivos definem a aparência, comportamento e layout dos menus, diálogos e telas do jogo.

## Arquivos e suas funções

| Arquivo | Tamanho | Descrição |
|---------|---------|-----------|
| `uiscensetdlg.xml` | 1.9 KB | Diálogo de configuração de cenário (Scenario Setup Dialog) - Opções para configurar parâmetros de mapa aleatório |
| `uisessionconnect.xml` | 2.5 KB | Diálogo de conexão de sessão multijogador |
| `uisingleplayersetup.xml` | 25.6 KB | Configuração de jogador único (Single Player Setup) - Tela principal de configuração de partida |
| `uisounddlg.xml` | 2.3 KB | Diálogo de configuração de som |
| `uisoundeditdlg.xml` | 1.4 KB | Diálogo de edição de som |
| `uisoundsetdlg.xml` | 5.3 KB | Configuração de conjunto de sons |
| `uisprmsetup.xml` | 13.2 KB | Configuração de primavera/primavera (Primavera Setup) |
| `uistatsupload.xml` | 2.0 KB | Upload de estatísticas |
| `uistatusmsg.xml` | 1.7 KB | Mensagem de status |
| `uitechtree.xml` | 3.3 KB | Árvore de tecnologias (Tech Tree) - Exibe tecnologias disponíveis por civilização |
| `uitributedlg.xml` | 21.1 KB | Diálogo de tributo (Tribute Dialog) - Envio de recursos entre jogadores |
| `uitriggereditor.xml` | 5.0 KB | Editor de triggers (gatilhos) para cenários |
| `uitriggroupeditor.xml` | 3.2 KB | Editor de grupos de triggers |
| `uiwaterdialog.xml` | 1.1 KB | Diálogo de configuração de água |
| `uixsdebugger.xml` | 3.6 KB | Depurador de scripts XS (eXtended Script) - Linguagem de scripts do AoM |
| `unittransform.xml` | 319 bytes | Transformações de unidade (ex: parede para portão) |
| `waterbodies.xml` | 5.4 KB | Definições de corpos d'água (lagos, rios, oceanos) |

## Uso no projeto
Estes arquivos são usados como referência para:
1. Implementar menus e diálogos UI no port nativo
2. Entender a estrutura XMB/XML original do Age of Mythology
3. Validar a implementação contra os dados originais
4. Comparar elementos de UI durante o desenvolvimento

## Observações
- Todos os arquivos estão em formato XML
- Extraídos dos arquivos BAR originais do jogo
- Usados como gabarito (reference data) para testes de regressão
- Não são os arquivos originais do jogo, são dados de referência para estudo
