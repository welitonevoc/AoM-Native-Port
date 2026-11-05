# Importação de dados e recursos do jogo

Este documento explica, em detalhe, como a engine localiza e carrega os
arquivos `.bar` da sua cópia legal de **Age of Mythology**, como instalar esses
arquivos no Android e como diagnosticar problemas.

> O APK **não redistribui** os arquivos proprietários do jogo. Sem a
> importação, a engine usa apenas os recursos de fallback empacotados no APK
> (algumas texturas, fontes, dados de exemplo e mapas procedurais).

---

## 1. Como a engine encontra os dados

Na inicialização, `src/platform/android/native_activity.cpp` monta uma lista de
pastas candidatas e escolhe a **primeira** que contenha um dos arquivos-base:

- `data/data.bar`
- `textures/textures.bar`
- `data.bar` ou `textures.bar` (layout plano)
- `models/models.bar`

### Ordem de prioridade

1. `Android/data/com.aom.nativeport/files/AoM` — pasta privada do app; é o
   destino do importador interno e **não exige** permissão de armazenamento.
2. `Android/data/com.aom.nativeport/files` — caso os `.bar` tenham sido
   colocados diretamente na raiz da pasta do app.
3. `files/AoM` no armazenamento interno do app
   (`/data/data/com.aom.nativeport/files/AoM`), equivalente privado usado em
   builds de desenvolvimento.
4. `/storage/emulated/0/Documents/AoM`
5. `/storage/emulated/0/Download/AoM`
6. `/storage/emulated/0/AoM`
7. Pastas legadas de desenvolvimento, como
   `/storage/emulated/0/Download/Games/PC/Age of Mythology/Age of Mythology`.

A busca também aceita **um nível de contêiner**. Por exemplo, se você copiou a
pasta inteira do jogo para `Download/AoM`, o resultado pode ser:

```text
Download/AoM/Age of Mythology/data/data.bar
Download/AoM/Age of Mythology/textures/textures.bar
```

Nesse caso a engine desce automaticamente um nível e usa
`Download/AoM/Age of Mythology` como raiz dos dados.

### O que a engine carrega hoje

Chamadas em `Engine::load_archives()` e `Engine::load_game_config()`:

| Caminho relativo à raiz | Conteúdo | Uso atual |
|---|---|---|
| `data/data.bar` | `proto.xmb`, techtree e balanceamento base | **Obrigatório** |
| `textures/textures.bar` | Texturas DDT/BTX, UI e terreno | **Obrigatório** |
| `models/models.bar` | Modelos e animações BRG | Carregado |
| `scenario/scenarios.bar` | Cenários e campanhas | Carregado |
| `sound/sounds.bar` | Efeitos, vozes e músicas | Carregado |
| `data/main campaign.cpn` | Campanha base | Carregado (fallback no APK) |
| `data/data2.bar` | Dados da expansão The Titans | **Carregado (overlay)** |
| `textures/textures2.bar` | Texturas da expansão | **Carregado (overlay)** |
| `models/models2.bar` | Modelos da expansão | **Carregado (overlay)** |
| `scenario/scenarios2.bar` | Cenários da expansão | **Carregado (overlay)** |
| `sound/sounds2.bar` | Áudio da expansão | **Carregado (overlay)** |
| `anim/anim.bar` + `anim2.bar` | Scripts de animação | Montado (runtime usa BRG de `models.bar`) |
| `god powers/godpowers.bar` + `godpowers2.bar` | Poderes dos deuses | Montado e carregado no `GodPowerSystem` |
| `data/xpack campaign.cpx` | Campanha The Titans | Carregado (2º `CampaignManager`) |
| `fonts/*.msk` / `fonts2.xml` | Fontes bitmap | Runtime usa fonte do sistema |
| `gfxconfig/` e `gfxconfig2/` | Perfis gráficos e pipelines | Apenas testes/subsistema |
| `startup/*.con` / `gamesettings*.bar` | Configuração de inicialização | Ainda não usado |
| `ai/*.xs` e `ai2/*.xs` | Scripts de IA | IA nativa própria; não consome `.xs` |
| `rm/` e `rm2/` | Random maps (`.xml`/`.xs`) | Mapa procedural; não consome |
| `language.dll`, `xpacklanguage.dll`, `<LCID>/dwintl.dll` | Strings localizadas | Apenas testes/subsistema |
| `avi/*.bik` | Cinemáticas originais | O app usa MP4 em `assets/avi` |

> **Importante:** só `data.bar` e `textures.bar` são realmente obrigatórios.
> As demais pastas podem ser copiadas para preservar a instalação completa,
> mas a engine ignora com segurança o que ainda não sabe ler.

### Como a expansão é montada (overlay)

O jogo original distribui a expansão como archives separados (`*2.bar`) que
**redeclaram** boa parte das entradas do jogo base com versões atualizadas e
acrescentam as entradas exclusivas dos Titãs. A engine reproduz esse
comportamento com *mounts* sobrepostos no `BarArchive`:

```text
BarArchive
├── mount 0 → data/data.bar        (jogo base)
└── mount 1 → data/data2.bar       (expansão, tem prioridade)
```

Regras aplicadas:

- **Último mount vence:** `proto.xmb`, por exemplo, resolve para a versão de
  `data2.bar` quando a expansão está instalada.
- **Entradas só do base continuam acessíveis:** `prwordlist.xmb` e
  `recentfiles.xmb` existem apenas em `data.bar` e não somem com o overlay.
- **Entradas só da expansão são adicionadas:** `protox.xmb` (801 unidades),
  `techtreex.xmb` (507 tecnologias), `main campaign.xmb`, `relics2.xmb`, …
- **A união não duplica:** `protox.xmb` é um *super-set* de `proto.xmb`; o
  `GameDataManager` substitui as definições repetidas em vez de duplicá-las.

Sem a expansão instalada, o jogo continua funcionando normalmente: apenas
um mount é aberto e nenhum arquivo `*2.bar` é exigido.

> No AoM Extended Edition, `proto.xmb` e os dados da expansão ficam **dentro**
> de `data.bar`/`data2.bar`. Arquivos soltos `proto.xml`/`protox.xml` só
> aparecem em instalações modificadas; o `protox.Voobly Balance Patch...` desta
> cópia é um arquivo de mod, não o proto original.

Os arquivos `.bar` são lidos **diretamente**. Não é necessário extrair,
descompactar ou renomear o conteúdo interno.

---

## 2. Instalação recomendada: importar pelo próprio app

Este é o fluxo mais simples e funciona no Android 11+ sem conceder acesso
total ao armazenamento.

1. Instale o APK (`aom-native-debug.apk`).
2. Tenha a pasta do jogo no celular, no cartão SD ou acessível pelo seletor de
   arquivos (pode ser copiada por USB, nuvem ou gerenciador de arquivos).
3. Abra o app.
4. Toque em **OPÇÕES** no menu principal.
5. Toque em **IMPORTAR DADOS DO JOGO**.
6. Leia as instruções e toque em **Escolher pasta**.
7. Selecione a pasta que **contém** as subpastas `data` e `textures`
   (normalmente a pasta `Age of Mythology`).
8. Aguarde a mensagem **“Dados importados. Reiniciando o jogo...”**.

### O que acontece internamente

- O app usa a **Storage Access Framework (SAF)** (`ACTION_OPEN_DOCUMENT_TREE`).
  O usuário concede acesso somente à pasta escolhida; não é necessário
  `MANAGE_EXTERNAL_STORAGE`.
- A cópia roda em uma thread separada (`aom-data-import`), evitando ANR.
- O conteúdo é copiado para:
  ```text
  /storage/emulated/0/Android/data/com.aom.nativeport/files/AoM/
  ```
- A cópia é *best effort*: arquivos opcionais inacessíveis são registrados no
  log, mas a importação é considerada válida se os BARs principais chegaram.
- Ao terminar, a Activity é recriada e a engine reinicia, refazendo a busca e o
  carregamento dos arquivos.

### Estrutura final esperada

A pasta importada pode conter a instalação inteira; a engine ignora o que ainda
não sabe ler. O mínimo para abrir é `data/data.bar` + `textures/textures.bar`.
Uma instalação completa fica assim:

```text
Android/data/com.aom.nativeport/files/AoM/
├── data/
│   ├── data.bar                 # obrigatório
│   ├── main campaign.cpn        # campanha base
│   ├── data2.bar                # expansão The Titans (overlay)
│   ├── xpack campaign.cpx       # campanha da expansão
│   └── *.lgt                    # iluminação (não usada ainda)
├── textures/
│   ├── textures.bar             # obrigatório
│   └── textures2.bar            # expansão (overlay)
├── models/
│   ├── models.bar               # recomendado
│   └── models2.bar              # expansão (overlay)
├── scenario/
│   ├── scenarios.bar            # recomendado
│   ├── scenarios2.bar           # expansão (overlay)
│   └── *.scn / *.scx            # mapas customizados (não carregados diretamente)
├── sound/
│   ├── sounds.bar               # recomendado
│   └── sounds2.bar              # expansão (overlay)
├── anim/
│   ├── anim.bar
│   └── anim2.bar
├── god powers/
│   ├── godpowers.bar
│   └── godpowers2.bar
├── fonts/                       # .msk + fonts2.xml
├── gfxconfig/                   # perfis gráficos
├── gfxconfig2/                  # perfis da expansão
├── startup/                     # .con + gamesettings*.bar
├── ai/                          # scripts .xs (base)
├── ai2/                         # scripts .xs (expansão)
├── rm/                          # random maps (base)
├── rm2/                         # random maps (expansão)
├── history/ + history2/          # textos históricos (não usados)
├── cameratracks/                 # tracks de câmera (não usados)
├── trigger/ + trigger2/          # scripts de trigger (não usados)
├── docs/                         # documentação original (não usada)
├── ppdata/                       # dados auxiliares (não usados)
├── Users/                        # perfis de usuário (não usados)
├── avi/                         # .bik originais (não lidos diretamente)
├── language.dll                 # strings base
├── xpacklanguage.dll            # strings da expansão
└── <LCID>/dwintl.dll            # recursos de idioma (1033, 1046, ...)
```

`savegame/`, `screenshots/`, `Voobly Mods/` e `_CommonRedist/` **não** são
recursos do jogo: são saves/replays, capturas, mods e instaladores. Não é
preciso copiá-los.

> Nesta instalação do AoM Extended Edition não existem as pastas `campaign/`,
> `gfx/` nem `screenshots/`. As campanhas ficam em `data/main campaign.cpn` e
> `data/xpack campaign.cpx`; os perfis gráficos ficam em `gfxconfig/` e
> `gfxconfig2/`; e `screenshots/` é criada pelo jogo original quando você tira
> uma captura. A pasta `Language/` está vazia; as strings ficam em
> `language.dll`, `xpacklanguage.dll` e nas pastas LCID (`1033`, `1046`, ...).

---

## 3. Instalação manual

### 3.1. Android 10 ou anterior (ou com acesso total)

1. Crie a pasta:
   ```text
   /storage/emulated/0/Download/AoM/
   ```
2. Copie **o conteúdo** da pasta do jogo para dentro dela. No mínimo:
   ```text
   Download/AoM/data/data.bar
   Download/AoM/textures/textures.bar
   Download/AoM/models/models.bar
   Download/AoM/scenario/scenarios.bar
   Download/AoM/sound/sounds.bar
   Download/AoM/data/main campaign.cpn
   ...
   ```
   Para preservar a instalação completa, copie também as pastas `anim`,
   `god powers`, `fonts`, `gfxconfig`, `gfxconfig2`, `startup`, `ai`, `ai2`,
   `rm` e `rm2`, além de `language.dll` e `xpacklanguage.dll`. Os arquivos
   `*2.bar` da expansão vão junto automaticamente ao copiar `data`,
   `textures`, `models`, `scenario` e `sound`.
3. Abra o app. A engine detecta `Download/AoM` automaticamente.

Se você copiou a pasta `Age of Mythology` inteira, ficando
`Download/AoM/Age of Mythology/...`, também funciona: a engine aceita esse
contêiner de primeiro nível.

### 3.2. Android 11+ sem acesso total

Gerenciadores de arquivos comuns podem não conseguir escrever em
`Android/data/...`. Use uma destas opções:

- o importador interno (seção 2); ou
- `adb push` (seção 3.3); ou
- conceda **Acesso a todos os arquivos** nas configurações do app e copie para
  `Download/AoM`.

### 3.3. Via ADB (recomendado para desktop)

Com o celular conectado e depuração USB ativada:

```bash
adb shell mkdir -p /sdcard/Android/data/com.aom.nativeport/files/AoM

# Opção mais simples: copiar a pasta inteira. A engine aceita a subpasta
# "Age of Mythology" como contêiner de primeiro nível.
adb push "C:\caminho\Age of Mythology" /sdcard/Android/data/com.aom.nativeport/files/AoM/
```

Se preferir copiar apenas os recursos usados pelo runtime:

```bash
adb push "C:\caminho\Age of Mythology\data"     /sdcard/Android/data/com.aom.nativeport/files/AoM/
adb push "C:\caminho\Age of Mythology\textures" /sdcard/Android/data/com.aom.nativeport/files/AoM/
adb push "C:\caminho\Age of Mythology\models"   /sdcard/Android/data/com.aom.nativeport/files/AoM/
adb push "C:\caminho\Age of Mythology\scenario" /sdcard/Android/data/com.aom.nativeport/files/AoM/
adb push "C:\caminho\Age of Mythology\sound"    /sdcard/Android/data/com.aom.nativeport/files/AoM/
```

Opcionais (a engine ainda não consome, mas preservam a instalação completa):

```bash
adb push "C:\caminho\Age of Mythology\anim"       /sdcard/Android/data/com.aom.nativeport/files/AoM/
adb push "C:\caminho\Age of Mythology\god powers" /sdcard/Android/data/com.aom.nativeport/files/AoM/
adb push "C:\caminho\Age of Mythology\fonts"      /sdcard/Android/data/com.aom.nativeport/files/AoM/
adb push "C:\caminho\Age of Mythology\gfxconfig"  /sdcard/Android/data/com.aom.nativeport/files/AoM/
adb push "C:\caminho\Age of Mythology\gfxconfig2" /sdcard/Android/data/com.aom.nativeport/files/AoM/
adb push "C:\caminho\Age of Mythology\startup"    /sdcard/Android/data/com.aom.nativeport/files/AoM/
adb push "C:\caminho\Age of Mythology\ai"         /sdcard/Android/data/com.aom.nativeport/files/AoM/
adb push "C:\caminho\Age of Mythology\ai2"        /sdcard/Android/data/com.aom.nativeport/files/AoM/
adb push "C:\caminho\Age of Mythology\rm"         /sdcard/Android/data/com.aom.nativeport/files/AoM/
adb push "C:\caminho\Age of Mythology\rm2"        /sdcard/Android/data/com.aom.nativeport/files/AoM/
adb push "C:\caminho\Age of Mythology\language.dll"      /sdcard/Android/data/com.aom.nativeport/files/AoM/
adb push "C:\caminho\Age of Mythology\xpacklanguage.dll" /sdcard/Android/data/com.aom.nativeport/files/AoM/
```

> A pasta `avi/` original contém vídeos `.bik`, que o app **não** reproduz
> diretamente. As cinemáticas usadas pelo APK são MP4 convertidos em
> `assets/avi/`; veja a seção 4.

Depois, feche e abra o app novamente (ou force a parada pelo Android).

> O caminho `Android/data/.../files` é específico do app e não exige
> `MANAGE_EXTERNAL_STORAGE`. O `adb` consegue gravá-lo em Android 11+.

---

## 4. Para desenvolvedores: empacotar no APK

`MyNativeActivity.copyGameAssetsIfNeeded()` copia, na primeira execução, os
seguintes assets do APK para a pasta privada `files/AoM`:

```text
assets/textures/textures.bar  ->  files/AoM/textures/textures.bar
assets/data/data.bar          ->  files/AoM/data/data.bar
assets/avi/*.mp4              ->  files/AoM/avi/*.mp4
```

Isso é útil para builds internos/licenciados. **Não** redistribua os arquivos
do jogo sem autorização. A cópia não sobrescreve arquivos já existentes; para
forçar a atualização, limpe os dados do app ou remova o arquivo de destino.

---

## 5. Verificação e diagnóstico

### Logs

```bash
adb logcat -s AoM-Native AoM-Engine AoM-Activity
```

Procure por:

```text
Discovered AoM data at: /storage/emulated/0/Android/data/com.aom.nativeport/files/AoM
Loaded archives: data=126, textures=..., models=..., scenarios=..., sounds=... (valid=1)
Data import picker requested: ok
Game data import finished (tree copy complete=true, usable=true)
```

Se aparecer:

```text
No AoM .bar archives found. Fallback directory: ...
```

significa que nenhuma pasta candidata continha os arquivos esperados.

### Problemas comuns

| Sintoma | Causa provável | Solução |
|---|---|---|
| “Pasta inválida” ao importar | A pasta selecionada não contém `data/data.bar` nem `textures/textures.bar` | Selecione a pasta `Age of Mythology`, não uma pasta acima ou um arquivo isolado |
| Dados importados, mas texturas não aparecem | `textures/textures.bar` ausente ou corrompido | Verifique o arquivo e importe novamente |
| App não detecta `Download/AoM` | Sem acesso ao armazenamento compartilhado no Android 11+ | Use o importador interno ou `adb push` |
| Nome do arquivo não encontrado | O sistema de arquivos Android diferencia maiúsculas/minúsculas | Use exatamente `data.bar`, `textures.bar` etc., em minúsculas |
| Importação demorou muito | Cópia de vários GB (texturas, sons, vídeos) | Normal; mantenha o app aberto e com espaço livre |
| App reinicia sozinho após importar | Comportamento esperado | A engine recarrega os novos arquivos |

### Limpar dados importados

```bash
adb shell rm -rf /sdcard/Android/data/com.aom.nativeport/files/AoM
```

Ou use **Configurações → Apps → AoM Native → Armazenamento → Limpar dados**.
Atenção: isso também apaga preferências e saves.

---

## 6. Observações legais

- *Age of Mythology* e seus recursos são propriedade da Microsoft
  Corporation / World's Edge.
- Use apenas arquivos da sua própria cópia legal do jogo.
- Este projeto é não oficial, educacional e sem fins lucrativos; não
  redistribua os arquivos proprietários.
