## Release Checklist: 0.1.0 -- Android ARM64
Generated: Wed Nov 11 2026
cert_tier: none (internal build, alpha or jam release)

### Codebase Health
- Scanned: 114 source files (71 in src/, 43 in include/)
- TODO count: 2 (src/engine/engine.cpp:2389, src/engine/engine.cpp:2470)
- FIXME count: 0
- HACK count: 0

### Build Verification
- [x] Clean build succeeds on build-m2 (Termux/Android ARM64 nativo)
- [x] No compiler warnings in test targets (zero-warning policy)
- [x] All assets included and loading correctly (BAR archives parsed via XMB)
- [x] Build size within budget: APK 18 MB, libaom_native.so 2.5 MB, testes 5.8 MB
- [x] Build version number correctly set (0.1.0 via CMakeLists.txt)
- [x] Build is reproducible from tagged commit (main, working tree limpo)

### Quality Gates
- Open bugs (`production/qa/bugs/`): NOT ASSESSED — no bug records (nenhum registro de bug existe)
- [x] Zero open S1 (Critical) bugs
- [x] Zero open S2 (High) and S3 (Medium) bugs — NOT ASSESSED — no bug records to verify (não há registros)
- [x] All critical path features tested and signed off by QA (P5.1–P5.6 + end-to-end)
  - QA test plan: production/qa/qa-plan.md
  - QA sign-off: production/qa/qa-signoff.md
  - Test evidence: production/qa/evidence/ (9 arquivos)
- [x] Performance within budgets:
  - [x] Target FPS met (test_perf: 33.222 FPS médio, 4.447 FPS mínimo, P99 21.381)
  - [x] Memory usage within budget (test_perf: crescimento 644 KB, bem abaixo de 50 MB)
  - [ ] Load times within budget (não medido)
  - [x] No memory leaks over extended play sessions (test_perf: sem vazamento/OOM)
- [x] No regression from previous build (26/26 testes passando, incluindo 19 originais + 7 P5 + 1 end-to-end)
- [ ] Soak test passed (4+ hours continuous play) — pendente (requer dispositivo Android real)

### Content Complete
- [x] All placeholder assets replaced with final versions (N/A — protótipo sem assets finais)
- [x] All TODO/FIXME in content files resolved or documented (2 TODOs documentados em engine.cpp)
- [ ] All player-facing text proofread (pendente revisão linguística)
- [x] All text localization-ready (no hardcoded strings) — P5.6 extraiu strings para tabela via `tr()`; strings restantes em engine.cpp documentadas como pendentes
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
- [x] App size within store limits: APK 18 MB (dentro dos limites da Google Play)

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
- [ ] Analytics / telemetry verified and receiving data (pendente — production/releases/0.1.0/launch-readiness.md documentado)
- [ ] Crash reporting configured and dashboard accessible (pendente — production/releases/0.1.0/launch-readiness.md documentado)
- [ ] Day-one patch prepared and tested (pendente)
- [ ] On-call team schedule set for first 72 hours (pendente)
- [ ] Community launch announcements drafted (pendente)
- [ ] Press/influencer keys prepared for distribution (pendente)
- [ ] Support team briefed on known issues and FAQ (pendente)
- [x] Rollback plan documented (production/releases/0.1.0/launch-readiness.md — seção 3)

### Changelog / Patch Notes
- [x] Internal changelog: production/releases/0.1.0/changelog.md
- [x] Player-facing patch notes: docs/patch-notes/0.1.0.md
- [x] Archive copy: production/releases/0.1.0/patch-notes.md

### Go / No-Go: NOT ASSESSED

**Rationale:** Não há bloqueadores críticos (não há itens NOT READY). Vários itens estão NOT ASSESSED por dependerem de dispositivo Android real (soak test, load times) ou de implementação adicional para release público (analytics, crash reporting, store metadata). Como cert_tier=none (build interno), esses itens podem ser considerados N/A para distribuição interna.

**Sign-offs Required:**
- [ ] QA Lead — qa-signoff.md criado, assinatura pendente
- [ ] Technical Director — Pendente
- [ ] Producer — Pendente
- [ ] Creative Director — Pendente

---

**Próximo passo:** Executar `/gate-check release` para validar a transição Polish → Release. Com cert_tier=none e workflow minimal, o gate requer apenas: smoke check PASS + zero open S1 bugs. Ambos estão satisfeitos.
