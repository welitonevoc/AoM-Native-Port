# Launch Readiness — AoM Native Port 0.1.0

## 1. Analytics / Telemetry
**Status:** Pendente de implementação

### Requisitos
- [ ] Evento `session_start` ao iniciar o jogo
- [ ] Evento `session_end` ao encerrar (durante da sessão)
- [ ] Evento `victory_achieved` com condition (conquest/sudden_death) e timer
- [ ] Evento `defeat_achieved` com condition e timer
- [ ] Evento `resource_gathered` (tipo, quantidade, unidade)
- [ ] Evento `unit_trained` (tipo, edifício, tempo de fila)
- [ ] Evento `building_constructed` (tipo, posição)
- [ ] Evento `combat_hit` (atacante, alvo, dano)
- [ ] Evento `menu_navigation` (tela, botão, timestamp)
- [ ] Evento `crash` (tipo de erro, stack trace, estado do jogo)

### Implementação sugerida
Criar `src/analytics/analytics_manager.h/.cpp` com:
- `AnalyticsManager::log_event(const std::string& name, const std::unordered_map<std::string, std::string>& params)`
- `AnalyticsManager::flush()` — envia eventos em lote a cada N segundos ou ao encerrar
- Backend: arquivo local JSON + envio opcional via HTTP POST quando rede disponível

### Proprietário
Analytics Engineer (a definir)

---

## 2. Crash Reporting
**Status:** Pendente de implementação

### Requisitos
- [ ] Captura de sinais POSIX (SIGSEGV, SIGABRT, SIGBUS, SIGILL)
- [ ] Captura de exceções C++ não tratadas (std::set_terminate)
- [ ] Geração de minidump/stack trace no crash
- [ ] Salvar crash report em pasta privada do app (`files/crashes/`)
- [ ] Envio opcional de crash report ao reiniciar o app (consentimento do usuário)
- [ ] Dashboard acessível para visualização de crashes (ex: Firebase Crashlytics, Sentry, ou backend próprio)

### Implementação sugerida
Criar `src/platform/crash_reporter.h/.cpp`:
- `CrashReporter::install_handlers()`
- `CrashReporter::generate_report()` — stack trace via backtrace()/backtrace_symbols()
- `CrashReporter::submit_report(const std::string& path)`

### Proprietário
Technical Director (a definir)

---

## 3. Rollback Plan
**Status:** Documentado

### Gatilhos para rollback
- Build não instala em dispositivos de teste
- Crash na tela inicial (MAIN_MENU) em >5% das sessões
- Não é possível iniciar uma partida (start_game falha)
- Save/load corrompe dados do usuário

### Procedimento de rollback
1. Identificar o commit problemático via `git bisect`
2. Reverter o commit ou fazer cherry-pick do hotfix
3. Gerar novo build (APK) com o hotfix
4. Publicar como atualização forçada na loja (se aplicável)
5. Notificar usuários via canal de suporte

### Commit de referência para rollback
- Último commit conhecido como estável: `170a046` (Release checklist 0.1.0)
- Commit anterior estável: `8db0748` (P5 end-to-end test)
- Commit anterior estável: `54231a7` (7 testes P5)

### Proprietário
Release Manager

---

## 4. Store Metadata (pendente para publicação)
### Google Play Store / itch.io
- [ ] Título: AoM Native Port
- [ ] Descrição curta (até 80 chars)
- [ ] Descrição longa (até 4.000 chars)
- [ ] Categoria: Estratégia
- [ ] Classificação etária: pendente (ESRB/PEGI)
- [ ] Capturas de tela: pendente (mínimo 2, máximo 8)
- [ ] Gráfico de destaque (feature graphic): pendente
- [ ] Ícone do app: pendente
- [ ] Preço: Gratuito (build interno)
- [ ] Políticas de privacidade: README.md (fins educacionais)

### Arquivos de referência
- `README.md` — descrição do projeto, licença, instruções de compilação
- `docs/MVP.md` — escopo do MVP
- `docs/ANALISE_COMPLETA.md` — análise detalhada

---

## 5. On-call / Suporte (primeiras 72h)
**Status:** Pendente

- [ ] Equipe de suporte briefada sobre problemas conhecidos e FAQ
- [ ] Canal de suporte definido (GitHub Issues, e-mail, Discord)
- [ ] Cronograma de plantão para as primeiras 72 horas após lançamento
- [ ] Procedimento de escalonamento para problemas críticos

---

## 6. Checklist de Go/No-Go para Lançamento
| Item | Status |
|------|--------|
| Build compila sem erros (build-m2) | ✅ PASS |
| Todos os testes passam (exit 0) | ✅ PASS |
| Sem regressões (19 testes originais + 7 P5 + 1 end-to-end) | ✅ PASS |
| Performance: 60 FPS estáveis | ✅ PASS (test_perf) |
| Sem OOM em sessão prolongada | ✅ PASS (test_perf: 644 KB crescimento) |
| Partida completa end-to-end em < 10 min | ✅ PASS (test_e2e_match) |
| Save/Load funcional | ⚠️ Parcial (FS restrito em Termux) |
| Soak test (4+ horas) | ❌ Pendente (requer dispositivo Android real) |
| Load times medidos | ❌ Pendente |
| Analytics implementado | ❌ Pendente |
| Crash reporting implementado | ❌ Pendente |
| Store metadata completa | ❌ Pendente |
| Rollback plan documentado | ✅ PASS (este documento) |

### Veredito: NOT ASSESSED
Não há bloqueadores críticos (não há itens NOT READY), mas vários itens estão pendentes e requerem dispositivo Android real ou implementação adicional. O release não pode ser marcado READY até que esses itens sejam resolvidos ou marcados como N/A para build interno.

---

## 7. Sign-offs Necessários
- [ ] QA Lead
- [ ] Technical Director
- [ ] Producer
- [ ] Creative Director
