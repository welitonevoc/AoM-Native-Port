# QA Test Plan — AoM Native Port 0.1.0
Generated: Wed Nov 11 2026
Release: 0.1.0 (cert_tier: none — build interno/alpha)

## 1. Escopo
Este plano cobre o MVP Completo / Vertical Slice (P5), validando que uma partida local pode iniciar, permitir jogar (economia, construção, combate) e terminar corretamente com condição de vitória.

## 2. Histórias de Usuário Cobertas (P5.1–P5.6)

| ID | História | Status |
|----|----------|--------|
| P5.1 | HUD Completo (população, minimapa, painel de comandos, fila de produção) | ✅ Implementado + testado |
| P5.2 | Town Center e Seleção de Grupo | ✅ Implementado + testado |
| P5.3 | Save/Load Completo e Condição de Vitória | ✅ Implementado + testado |
| P5.4 | IA Econômica e Militar Básica | ✅ Implementado + testado |
| P5.5 | Áudio Ampliado (MP3, Sound Banks, 3D, Pool SFX) | ✅ Implementado + testado |
| P5.6 | UI de XMB e Localização | ✅ Implementado + testado |

## 3. Casos de Teste

### 3.1 — HUD Completo (P5.1)
- [x] Contador de população mostra atual/cap
- [x] Minimapa renderiza blips de recursos e unidades
- [x] Painel de comandos aparece ao selecionar unidade/edifício
- [x] Barra de progresso de produção exibe tooltip dinâmico
- [x] Contagem de multi-seleção exibida no HUD ("N selecionadas")

### 3.2 — Town Center (P5.2)
- [x] TC é construível pelo jogador via HUD (custo: 400M 200O)
- [x] TC funciona como ponto de retorno de recursos
- [x] TC fornece +15 de limite populacional cada
- [x] Box select seleciona múltiplas unidades
- [x] Anéis de seleção aparecem em unidades selecionadas
- [x] Clique no vazio limpa seleção

### 3.3 — Save/Load e Vitória (P5.3)
- [x] Save header v2 com campos vitória (condition/result/timer)
- [x] Carregar save restaura recursos e estados
- [x] Vitória por Conquest (sem inimigos) aciona tela de vitória
- [x] Vitória por Sudden Death (TC inimigo destruído) aciona tela de vitória
- [x] Transição vitória → menu limpa estado

### 3.4 — IA Econômica e Militar (P5.4)
- [x] Inimigo treina unidades (Villager, Hoplite)
- [x] Inimigo coleta recursos e devolve ao TC
- [x] Inimigo constrói edifícios (House, Barracks)
- [x] Inimigo ataca unidades/jogador quando detecta alvo

### 3.5 — Áudio Ampliado (P5.5)
- [x] Sound banks consumidos do sounds.bar
- [x] Áudio posicional 3D funciona (distância/atualização do listener)
- [x] Múltiplos SFX simultâneos não se cortam
- [x] Controle de volume por categoria (música/SFX)

### 3.6 — UI de XMB e Localização (P5.6)
- [x] Menu principal montado a partir de uimain.xmb (com fallback)
- [x] Painel de comandos usa commandpanel.xmb (com fallback)
- [x] Strings da UI extraídas para tabela de localização (tr())

### 3.7 — Partida Completa End-to-End (P5 Gate)
- [x] Spawn → Economia → Combate → Vitória em < 10 min
- [x] Recursos coletados (Food/Wood/Gold > 0)
- [x] População friendly > 0 durante a partida
- [x] Vitória por Conquest após destruição do TC inimigo

### 3.8 — Performance (P4.G4 Revalidação)
- [x] 60 FPS estáveis (test_perf: 33.222 FPS médio)
- [x] Sem OOM em sessão prolongada (test_perf: 644 KB crescimento)

## 4. Evidências de Teste

| Evidência | Arquivo | Status |
|-----------|---------|--------|
| Testes P5 (7 suites) | src/tests/test_*.cpp | ✅ Todos passando (exit 0) |
| Teste end-to-end | src/tests/test_e2e_match.cpp | ✅ Passando (exit 0) |
| Performance (P4.G4) | build-m2/perf_report.json | ✅ Gerado |
| Build limpo | build-m2/libaom_native.so | ✅ Compilado |
| APK de debug | app-debug.apk | 18.3 MB |

## 5. Critérios de Aceitação
- [x] Todos os testes unitários passam (exit 0)
- [x] Build compila sem erros no build-m2 (Termux/Android ARM64)
- [x] Partida completa joga do início ao fim
- [x] Vitória/derrota funcionam corretamente
- [x] Performance dentro do orçamento (FPS + memória)
- [ ] Soak test (4+ horas) — pendente (requer dispositivo Android real)
- [ ] Validação em dispositivo Android físico — pendente

## 6. Riscos Conhecidos
- Save/Load roundtrip de entidades não validado em Termux (restrição de FS)
- Strings hardcoded em português ainda existem em engine.cpp (localização parcial)
- Nenhum teste executado em dispositivo Android físico

---

**QA Lead:** Pendente  
**Data:** 11/11/2026  
**Veredito:** APROVADO COM CONDIÇÕES (itens pendentes listados acima)
