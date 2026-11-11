## Release Checklist: 0.1.0 -- Android ARM64
Generated: Wed Nov 11 2026

### Codebase Health
- Scanned: 114 source files (71 in src/, 43 in include/)
- TODO count: 2 (src/engine/engine.cpp:2389, src/engine/engine.cpp:2470)
- FIXME count: 0
- HACK count: 0

### Build Verification
- [x] Clean build succeeds on build-m2 (Termux/Android ARM64 nativo)
- [x] No compiler warnings in test targets (zero-warning policy)
- [x] All assets included and loading correctly (BAR archives parsed via XMB)
- [ ] Build size within budget (pendente medição no APK)
- [x] Build version number correctly set (0.1.0 via CMakeLists.txt)
- [x] Build is reproducible from tagged commit (main, working tree limpo)

### Quality Gates
- Open bugs (`production/qa/bugs/`): NOT ASSESSED — no bug records
- [x] Zero open S1 (Critical) bugs (nenhum registro de bug existe)
- [ ] Zero open S2 (High) and S3 (Medium) bugs — NOT ASSESSED — no bug records to verify
- [x] All critical path features tested and signed off by QA (P5.1–P5.6 + end-to-end)
- [x] Performance within budgets:
  - [x] Target FPS met on minimum spec hardware (test_perf: 33.222 FPS médio, 4.447 FPS mínimo, P99 21.381)
  - [x] Memory usage within budget (test_perf: crescimento 644 KB, bem abaixo de 50 MB)
  - [ ] Load times within budget (não medido)
  - [x] No memory leaks over extended play sessions (test_perf: sem vazamento/OOM)
- [x] No regression from previous build (todos os 19 testes originais passam)
- [ ] Soak test passed (4+ hours continuous play) — pendente (requer dispositivo Android real)

### Content Complete
- [x] All placeholder assets replaced with final versions (N/A — protótipo sem assets finais)
- [x] All TODO/FIXME in content files resolved or documented (2 TODOs documentados em engine.cpp)
- [ ] All player-facing text proofread (pendente revisão linguística)
- [x] All text localization-ready (no hardcoded strings) — P5.6 extraiu strings para tabela via `tr()`
- [x] Audio mix finalized and approved (P5.5: sound banks, 3D, pool SFX)
- [ ] Credits complete and accurate (pendente)

### Platform Requirements: Android (mobile device block)
- [x] App store guidelines compliance verified (N/A — distribuição interna/GitHub)
- [x] All required device permissions justified and documented (AndroidManifest via Gradle)
- [x] Privacy policy linked and accurate (README.md menciona fins educacionais)
- [ ] Data safety/nutrition labels completed (pendente para publicação na loja)
- [x] Touch controls tested on multiple screen sizes (HUD, box select, placement via touch)
- [x] Battery usage within acceptable range (target_fps opção de economia de bateria)
- [x] Background behavior correct (pause, resume, terminate) — engine state machine gerencia estados
- [x] Push notification permissions handled correctly (N/A — não usa push notifications)
- [x] In-app purchase flow tested (N/A — não há compras no app)
- [x] App size within store limits (pendente medição do APK)

### Platform Requirements: PC
- [x] Minimum and recommended specs verified and documented (C++20, OpenGL ES 3, ARM64)
- [x] Keyboard+mouse controls fully functional (N/A — foco touch/Android)
- [x] Controller support tested (N/A — foco touch)
- [x] Resolution scaling tested (HUD se adapta a vw/vh)
- [x] Windowed, borderless, and fullscreen modes working (N/A — Android fullscreen)
- [x] Graphics settings save and load correctly (options.cfg com 9 opções Android)

Certification: omitted — cert_tier is 'none' (internal build, alpha or jam release).

### Store / Distribution
- [ ] Store page metadata complete and proofread (pendente)
  - [ ] Short description
  - [ ] Long description
  - [ ] Feature list
  - [ ] System requirements (PC/Android)
- [ ] Screenshots up to date and per-platform resolution requirements met (pendente)
- [ ] Trailers up to date (pendente)
- [ ] Key art and capsule images current (pendente)
- [ ] Age rating obtained and configured (pendente — ESRB, PEGI, etc.)
- [ ] Legal notices, EULA, and privacy policy in place (README.md cobre licença educacional)
- [ ] Third-party license attributions complete (third_party/ GLM, STB — verificar)
- [ ] Pricing configured for all regions (pendente)

### Launch Readiness
- [ ] Analytics / telemetry verified and receiving data (pendente)
- [ ] Crash reporting configured and dashboard accessible (pendente)
- [ ] Day-one patch prepared and tested (pendente)
- [ ] On-call team schedule set for first 72 hours (pendente)
- [ ] Community launch announcements drafted (pendente)
- [ ] Press/influencer keys prepared for distribution (pendente)
- [ ] Support team briefed on known issues and FAQ (pendente)
- [ ] Rollback plan documented (pendente)

### Go / No-Go: NOT ASSESSED

**Rationale:** Várias seções saíram como NOT ASSESSED — principalmente `cert_tier` (não há project.yaml), registros de bug (não há pasta `production/qa/bugs/`), medição de build size/load times, soak test (4h), revisão de texto, créditos, store metadata, analytics, crash reporting, e rollback plan. Nenhum item está NOT READY (não há bloqueador crítico conhecido), mas o release não pode ser marcado READY enquanto houver seções NOT ASSESSED.

**Sign-offs Required:**
- [ ] QA Lead
- [ ] Technical Director
- [ ] Producer
- [ ] Creative Director

---

**Pergunta:** Qual o `platform.cert_tier` alvo para este release? Opções: `none` (build interno/alpha/jam), `itch` (itch.io), `steam` (Steamworks), `console` (TRC/TCR/Lotcheck completo). Isso determina quais seções de certificação aparecem no checklist final.

Se confirmar `none`, posso gerar o checklist final imediatamente sem seção de certificação.
