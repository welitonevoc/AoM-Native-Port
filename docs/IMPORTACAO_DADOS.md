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

### O que a engine carrega

| Caminho relativo à raiz | Conteúdo | Observação |
|---|---|---|
| `data/data.bar` | `proto.xmb`, techtree, dados de balanceamento | **Principal** |
| `textures/textures.bar` | Texturas DDT, UI, terreno | **Principal** |
| `models/models.bar` | Modelos e animações BRG | Recomendado |
| `scenario/scenarios.bar` | Cenários e campanhas | Recomendado |
| `sound/sounds.bar` | Efeitos sonoros e vozes | Opcional |
| `anim/anim.bar` | Scripts de animação | Opcional/experimental |
| `god powers/godpowers.bar` | Poderes dos deuses | Opcional |
| `god powers/godpowers2.bar` | Poderes da expansão The Titans | Opcional |
| `data/main campaign.cpn` | Definição da campanha | Opcional (há cópia no APK) |
| `avi/*.mp4` | Cinemáticas convertidas | Opcional |

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

```text
Android/data/com.aom.nativeport/files/AoM/
├── data/
│   ├── data.bar
│   └── main campaign.cpn
├── textures/
│   └── textures.bar
├── models/
│   └── models.bar
├── scenario/
│   └── scenarios.bar
├── sound/
│   └── sounds.bar
├── anim/
│   └── anim.bar
├── god powers/
│   ├── godpowers.bar
│   └── godpowers2.bar
└── avi/
    ├── aom-logo.mp4
    └── introcg.mp4
```

As pastas `anim`, `god powers` e `avi` são opcionais. Se você selecionou a pasta
`Age of Mythology` inteira, elas serão copiadas junto.

---

## 3. Instalação manual

### 3.1. Android 10 ou anterior (ou com acesso total)

1. Crie a pasta:
   ```text
   /storage/emulated/0/Download/AoM/
   ```
2. Copie **o conteúdo** da pasta do jogo para dentro dela:
   ```text
   Download/AoM/data/data.bar
   Download/AoM/textures/textures.bar
   Download/AoM/models/models.bar
   ...
   ```
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

adb push "C:\caminho\Age of Mythology\data"     /sdcard/Android/data/com.aom.nativeport/files/AoM/
adb push "C:\caminho\Age of Mythology\textures" /sdcard/Android/data/com.aom.nativeport/files/AoM/
adb push "C:\caminho\Age of Mythology\models"   /sdcard/Android/data/com.aom.nativeport/files/AoM/
adb push "C:\caminho\Age of Mythology\scenario" /sdcard/Android/data/com.aom.nativeport/files/AoM/
adb push "C:\caminho\Age of Mythology\sound"    /sdcard/Android/data/com.aom.nativeport/files/AoM/
```

Opcionais:

```bash
adb push "C:\caminho\Age of Mythology\anim"       /sdcard/Android/data/com.aom.nativeport/files/AoM/
adb push "C:\caminho\Age of Mythology\god powers" /sdcard/Android/data/com.aom.nativeport/files/AoM/
adb push "C:\caminho\Age of Mythology\avi"        /sdcard/Android/data/com.aom.nativeport/files/AoM/
```

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
