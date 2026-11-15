# AoM Native Port - Particle Fix (.prt)

Repositório documentando a correção de arquivos de partículas (`.prt`) faltantes no **Age of Mythology Native Port** para Android.

## 🎯 Problema

O port nativo do Age of Mythology para Android exibia erros no logcat procurando por 5 arquivos de partículas que não existiam nos dados do jogo:

```
[E] [AoM-BAR] BAR: File 'particles/fire_torch.prt' not found
[E] [AoM-BAR] BAR: File 'particles/combat_hit.prt' not found
[E] [AoM-BAR] BAR: File 'particles/construction_sparks.prt' not found
[E] [AoM-BAR] BAR: File 'particles/death_smoke.prt' not found
[E] [AoM-BAR] BAR: File 'particles/harvest_chips.prt' not found
```

Esses arquivos **não existem no jogo original do PC** (Age of Mythology / Age of Mythology: The Titans). O engine do port Android está hardcoded para procurar por esses nomes, mas eles nunca foram incluídos nos arquivos `.bar` do jogo.

## 🔍 Investigação

### Arquivos analisados
Todos os 14 arquivos `.bar` da instalação original foram inspecionados:

| Arquivo .bar | Localização | Contém .prt? |
|-------------|-------------|--------------|
| `data.bar` | `/data/` | ❌ Não |
| `data2.bar` | `/data/` | ❌ Não |
| `textures.bar` | `/textures/` | ❌ Não |
| `textures2.bar` | `/textures/` | ❌ Não |
| `models.bar` | `/models/` | ✅ Sim (585 arquivos) |
| `models2.bar` | `/models/` | ✅ Sim (198 arquivos) |
| `anim.bar` | `/anim/` | ❌ Não |
| `anim2.bar` | `/anim/` | ❌ Não |
| `sounds.bar` | `/sound/` | ❌ Não |
| `sounds2.bar` | `/sound/` | ❌ Não |
| `scenarios.bar` | `/scenario/` | ❌ Não |
| `scenarios2.bar` | `/scenario/` | ❌ Não |
| `godpowers.bar` | `/god powers/` | ❌ Não |
| `godpowers2.bar` | `/god powers/` | ❌ Não |
| `ppdata.bar` | `/ppdata/` | ❌ Não |

**Total extraído:** 783 arquivos `.prt` reais do jogo original.

### Os 5 arquivos que NÃO existem

Após extrair todos os 783 arquivos `.prt` do jogo original, confirmamos que **nenhum** tem os nomes procurados pelo port Android:

| Nome procurado pelo Android | Existe no original? | Arquivo original similar usado como substituto |
|----------------------------|---------------------|------------------------------------------------|
| `fire_torch.prt` | ❌ Não | `sfx a building fire small.prt` |
| `combat_hit.prt` | ❌ Não | `sfx a arrow fire.prt` |
| `construction_sparks.prt` | ❌ Não | `sfx a building dust.prt` |
| `death_smoke.prt` | ❌ Não | `sfx a generic smoke large.prt` |
| `harvest_chips.prt` | ❌ Não | `sfx a villager digging dust.prt` |

## 🛠️ Solução Aplicada

### 1. Extração dos arquivos `.bar`

Foi criado um extrator Python que lê os arquivos `.bar` do Age of Mythology e extrai todos os arquivos `.prt` contidos neles.

O script `extract_aom.py`:
- Percorre recursivamente a pasta do jogo procurando por arquivos `.bar`
- Encontra strings terminadas em `.prt` dentro dos dados binários
- Extrai cada arquivo com seu respectivo nome e tamanho
- Salva em pasta de saída organizada

### 2. Cópia para o Android

Os 783 arquivos `.prt` extraídos foram copiados para:

```
/storage/emulated/0/Download/Games/PC/Age of Mythology/Age of Mythology/particles/
```

### 3. Mapeamento dos 5 arquivos faltantes

Os 5 nomes que o engine do Android procura foram preenchidos com cópias dos arquivos originais mais similares:

```bash
cp "sfx a building fire small.prt"     "fire_torch.prt"
cp "sfx a arrow fire.prt"             "combat_hit.prt"
cp "sfx a building dust.prt"          "construction_sparks.prt"
cp "sfx a generic smoke large.prt"    "death_smoke.prt"
cp "sfx a villager digging dust.prt"  "harvest_chips.prt"
```

## 📁 Estrutura de Pastas

```
Age of Mythology/
├── particles/
│   ├── fire_torch.prt                 ← Cópia de sfx a building fire small.prt
│   ├── combat_hit.prt                 ← Cópia de sfx a arrow fire.prt
│   ├── construction_sparks.prt        ← Cópia de sfx a building dust.prt
│   ├── death_smoke.prt                ← Cópia de sfx a generic smoke large.prt
│   ├── harvest_chips.prt              ← Cópia de sfx a villager digging dust.prt
│   ├── sfx a fire ball.prt            ← Arquivo original extraído do models.bar
│   ├── sfx a generic smoke large.prt  ← Arquivo original extraído do models.bar
│   ├── world a gold sparkles.prt      ← Arquivo original extraído do models.bar
│   └── ... (783 arquivos .prt totais)
├── data/
│   ├── data.bar
│   └── data2.bar
├── textures/
│   ├── textures.bar
│   └── textures2.bar
├── models/
│   ├── models.bar
│   └── models2.bar
└── ... (outras pastas do jogo)
```

## 🚀 Como Reproduzir

### Pré-requisitos
- Python 3 instalado
- Acesso aos arquivos originais do Age of Mythology (PC)
- Dispositivo Android com o port nativo do AoM instalado

### Passo 1: Extrair os .prt do jogo original

```bash
# Clone ou baixe este repositório
git clone <seu-repositorio>
cd <seu-repositorio>

# Rode o extrator apontando para a pasta do jogo original
python3 extract_aom.py "/caminho/para/o/Age of Mythology" -o ./aom_prt_extracted
```

### Passo 2: Copiar para o Android

Via ADB (sem fio ou USB):

```bash
# Verificar conexão
adb devices

# Copiar todos os .prt extraídos para o Android
adb push ./aom_prt_extracted/ "/storage/emulated/0/Download/Games/PC/Age of Mythology/Age of Mythology/particles/"
```

### Passo 3: Verificar

Reinicie o jogo e rode:

```bash
adb logcat -d | grep -E "particles|not found" | tail -n 20
```

Os erros `[E] [AoM-BAR] BAR: File 'particles/... not found` não devem mais aparecer para os 5 arquivos mapeados.

## 📜 Script Extrator (`extract_aom.py`)

O script incluso neste repositório extrai automaticamente todos os arquivos `.prt` de todos os arquivos `.bar` encontrados na pasta do Age of Mythology.

### Uso
```bash
python3 extract_aom.py <caminho-do-aom> -o <pasta-de-saida>
```

### Exemplo
```bash
python3 extract_aom.py "/sdcard/Documents/aom" -o ./aom_prt_extracted
```

## ⚠️ Limitações

1. **Direitos autorais:** Os arquivos `.bar` e `.prt` originais do Age of Mythology são propriedade da Microsoft/Ensemble Studios. Este repositório contém apenas o script extrator e documentação, não os arquivos originais do jogo.

2. **Partículas visuais:** Os 5 arquivos mapeados são cópias de arquivos originais similares, não cópias exatas dos nomes procurados. As partículas podem não ser visualmente idênticas ao que o desenvolvedor do port pretendia, mas eliminam os erros de log.

3. **Formato binário:** Os arquivos `.prt` têm um formato binário específico do motor do Age of Mythology. Este repositório não inclui documentação completa do formato, apenas o extrator que já funciona para os arquivos do jogo.

## 🤝 Contribuição

Sinta-se à vontade para:
- Melhorar o extrator `.bar`
- Adicionar documentação sobre o formato `.prt`
- Reportar melhores substitutos para os 5 arquivos mapeados
- Adicionar suporte a outros tipos de arquivo do AoM

## 📄 Licença

Este projeto é documentação e ferramenta de extração. Os arquivos de jogo do Age of Mythology permanecem sob os direitos autorais originais da Microsoft/Ensemble Studios.

---

## 📝 Histórico de Alterações

| Data | Alteração |
|------|-----------|
| 15/11/2026 | Criação do extrator `.bar` e extração de 783 arquivos `.prt` |
| 15/11/2026 | Mapeamento dos 5 arquivos faltantes com substitutos originais |
| 15/11/2026 | Documentação completa para GitHub |

---

## 🔗 Links Úteis

- [Age of Mythology Heaven - Downloads](https://aom.heavengames.com/downloads/)
- [APC: AoM PRT Converter 2.0](https://aom.heavengames.com/downloads/lister.php?category=utilities)
- [Fórum AoM Heaven - Editing PRT Files](https://aom.heavengames.com/cgi-bin/forums/display.cgi?action=st&fn=19&tn=30065)
