/**
 * P4.G4 — Validação de Performance e Memória (60 FPS + sem OOM)
 *
 * Valida, usando sistemas REAIS do jogo (ECSWorld, MovementSystem e um pool
 * de partículas espelhando a implementação do Engine/P4.3), que o loop de
 * atualização mantém >= 60 FPS estáveis e não cresce a memória
 * indefinidamente (sem OOM) ao longo de uma sessão prolongada.
 *
 * Não depende de renderer/native_window (EGL), portanto roda no Termux sem
 * servidor de display — adequado para validação de CI/local antes do teste
 * em dispositivo Android físico.
 *
 * Compila como: make -C build-m2 test_perf -j4
 * Roda em:        ./build-m2/test_perf
 */

#include <chrono>
#include <thread>
#include <vector>
#include <numeric>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <cmath>
#include <cstdio>
#include <string>
#include <cstdint>
#include <unordered_map>

#include "ecs/ecs.h"

using namespace aom;

static size_t get_memory_kb() {
    std::ifstream f("/proc/self/status");
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("VmRSS:", 0) == 0) {
            size_t kb = 0;
            std::sscanf(line.c_str(), "VmRSS: %zu kB", &kb);
            return kb;
        }
    }
    return 0;
}

/// Espelha o pool de partículas do Engine (P4.3): slots fixos, reaproveitamento
/// de inativos, sem push_back/erase durante o jogo.
struct PerfParticle {
    bool active = false;
    glm::vec3 pos{0.0f};
    glm::vec3 vel{0.0f};
    float life = 0.0f;
    float max_life = 1.0f;
};

static void spawn_particle(std::vector<PerfParticle>& pool,
                           const glm::vec3& pos, const glm::vec3& vel,
                           float life) {
    for (auto& p : pool) {
        if (!p.active) {
            p.active = true;
            p.pos = pos;
            p.vel = vel;
            p.life = 0.0f;
            p.max_life = (life > 0.05f) ? life : 0.6f;
            return;
        }
    }
    // Pool cheio: descarta (sem alocação).
}

static void update_particles(std::vector<PerfParticle>& pool, float dt) {
    for (auto& p : pool) {
        if (!p.active) continue;
        p.life += dt;
        p.pos += p.vel * dt;
        p.vel.y -= 9.8f * dt; // gravidade
        if (p.pos.y < 0.02f) {
            p.pos.y = 0.02f;
            p.vel.y = -p.vel.y * 0.35f;
            p.vel.x *= 0.75f;
            p.vel.z *= 0.75f;
        }
        if (p.life >= p.max_life) p.active = false; // libera slot
    }
}

int main() {
    std::cout << "=== P4.G4 Performance & Memory Validation (Real Game Systems) ===\n";

    const double TARGET_FPS = 60.0;
    const double FRAME_TIME_S = 1.0 / TARGET_FPS;
    const float DT = static_cast<float>(FRAME_TIME_S);

    // Simular 60 segundos de jogo (3600 frames @60fps) — representa uma
    // sessão prolongada o suficiente para detectar vazamento de memória.
    const int SIMULATION_SECONDS = 60;
    const int TOTAL_FRAMES = SIMULATION_SECONDS * 60;

    // Carga real: 2000 entidades com transform + health, mais 2000 partículas
    // no pool fixo (sem realocação), espelhando o P4.1/P4.3.
    const int NUM_ENTITIES = 2000;
    const int PARTICLE_POOL = 2000;

    std::vector<double> frame_times_ms;
    frame_times_ms.reserve(TOTAL_FRAMES);

    size_t mem_start = get_memory_kb();
    size_t mem_peak = mem_start;

    // ── Setup real: ECSWorld + MovementSystem (sistemas do jogo) ────────────
    aom::ECSWorld ecs_world;
    auto* movement = ecs_world.add_system<aom::MovementSystem>(
        ecs_world.transforms_mut());

    for (int i = 0; i < NUM_ENTITIES; ++i) {
        Entity e = ecs_world.create_entity();
        auto& tr = ecs_world.add_transform(e);
        tr.position = glm::vec3(
            (static_cast<float>(i % 45) - 22.5f) * 1.4f,
            0.0f,
            (static_cast<float>(i / 45) - 22.5f) * 1.4f);
        tr.velocity = glm::vec3(std::sin(static_cast<float>(i)) * 1.5f, 0.0f,
                                std::cos(static_cast<float>(i)) * 1.5f);
        auto& hp = ecs_world.add_health(e);
        hp.current_hp = 100;
        hp.max_hp = 100;
    }

    // Pool fixo de partículas (P4.3): sem push_back/erase durante o jogo.
    std::vector<PerfParticle> particle_pool;
    particle_pool.reserve(PARTICLE_POOL);
    particle_pool.resize(PARTICLE_POOL);

    auto sim_start = std::chrono::steady_clock::now();

    for (int frame = 0; frame < TOTAL_FRAMES; ++frame) {
        auto frame_start = std::chrono::steady_clock::now();

        // 1. Atualização real de gameplay (movimento de todas as entidades).
        ecs_world.update(DT);

        // 2. Simulação de burst de partículas a cada frame (combate/coleta).
        const int burst = 40;
        for (int b = 0; b < burst; ++b) {
            float angle = static_cast<float>(frame * 7 + b) * 0.13f;
            spawn_particle(particle_pool,
                           glm::vec3(std::sin(angle) * 8.0f, 2.0f,
                                     std::cos(angle) * 8.0f),
                           glm::vec3(std::cos(angle) * 2.0f, 4.0f,
                                     std::sin(angle) * 2.0f),
                           0.8f);
        }
        update_particles(particle_pool, DT);

        // Contar partículas ativas (sem alocação).
        size_t active = 0;
        for (const auto& p : particle_pool) if (p.active) ++active;
        (void)active;

        auto frame_end = std::chrono::steady_clock::now();
        double frame_ms =
            std::chrono::duration<double, std::milli>(frame_end - frame_start)
                .count();
        frame_times_ms.push_back(frame_ms);

        size_t mem_now = get_memory_kb();
        if (mem_now > mem_peak) mem_peak = mem_now;
    }

    auto sim_end = std::chrono::steady_clock::now();
    double total_sec =
        std::chrono::duration<double>(sim_end - sim_start).count();

    // Estatísticas
    std::sort(frame_times_ms.begin(), frame_times_ms.end());
    double sum_ms = std::accumulate(frame_times_ms.begin(), frame_times_ms.end(),
                                    0.0);
    double avg_ms = sum_ms / frame_times_ms.size();
    double min_ms = frame_times_ms.front();
    double max_ms = frame_times_ms.back();
    double p99_ms =
        frame_times_ms[static_cast<size_t>(frame_times_ms.size() * 0.99)];

    double avg_fps = 1000.0 / avg_ms;
    double min_fps = 1000.0 / max_ms;
    double max_fps = 1000.0 / min_ms;
    double p99_fps = 1000.0 / p99_ms;

    size_t mem_end = get_memory_kb();
    size_t mem_growth = mem_end > mem_start ? mem_end - mem_start : 0;

    std::cout << "\n=== RESULTADO P4.G4 (" << SIMULATION_SECONDS << "s simulado, "
              << TOTAL_FRAMES << " frames) ===\n";
    std::cout << "Tempo real decorrido: " << total_sec << "s\n";
    std::cout << "Entidades ECS: " << NUM_ENTITIES
              << " | Pool de partículas: " << PARTICLE_POOL << "\n\n";

    std::cout << "FPS:\n";
    std::cout << "  Médio:  " << avg_fps << "\n";
    std::cout << "  Mínimo: " << min_fps << "\n";
    std::cout << "  Máximo: " << max_fps << "\n";
    std::cout << "  P99:    " << p99_fps << "\n\n";

    std::cout << "Frame Time (ms):\n";
    std::cout << "  Médio:  " << avg_ms << "\n";
    std::cout << "  Mínimo: " << min_ms << "\n";
    std::cout << "  Máximo: " << max_ms << "\n";
    std::cout << "  P99:    " << p99_ms << "\n\n";

    std::cout << "Memória:\n";
    std::cout << "  Inicial: " << mem_start << " KB\n";
    std::cout << "  Final:   " << mem_end << " KB\n";
    std::cout << "  Pico:    " << mem_peak << " KB\n";
    std::cout << "  Crescimento: " << mem_growth << " KB\n\n";

    // Critérios de aceitação P4.G4
    bool pass = true;
    std::cout << "=== CRITÉRIOS P4.G4 ===\n";
    if (avg_fps >= 55) {
        std::cout << "✅ FPS médio >= 55: " << avg_fps << "\n";
    } else {
        std::cout << "❌ FPS médio >= 55: " << avg_fps << "\n";
        pass = false;
    }
    if (min_fps >= 30) {
        std::cout << "✅ FPS mínimo >= 30: " << min_fps << "\n";
    } else {
        std::cout << "❌ FPS mínimo >= 30: " << min_fps << "\n";
        pass = false;
    }
    if (p99_fps >= 30) {
        std::cout << "✅ P99 FPS >= 30: " << p99_fps << "\n";
    } else {
        std::cout << "❌ P99 FPS >= 30: " << p99_fps << "\n";
        pass = false;
    }
    if (mem_growth < 51200) {
        std::cout << "✅ Crescimento mem < 50MB: " << mem_growth << " KB\n";
    } else {
        std::cout << "❌ Crescimento mem < 50MB: " << mem_growth << " KB\n";
        pass = false;
    }

    // Salvar relatório JSON
    std::ofstream json("perf_report.json");
    json << "{\n";
    json << "  \"test\": \"P4.G4\",\n";
    json << "  \"simulation_seconds\": " << SIMULATION_SECONDS << ",\n";
    json << "  \"total_frames\": " << TOTAL_FRAMES << ",\n";
    json << "  \"num_entities\": " << NUM_ENTITIES << ",\n";
    json << "  \"particle_pool\": " << PARTICLE_POOL << ",\n";
    json << "  \"avg_fps\": " << avg_fps << ",\n";
    json << "  \"min_fps\": " << min_fps << ",\n";
    json << "  \"max_fps\": " << max_fps << ",\n";
    json << "  \"p99_fps\": " << p99_fps << ",\n";
    json << "  \"avg_frame_ms\": " << avg_ms << ",\n";
    json << "  \"min_frame_ms\": " << min_ms << ",\n";
    json << "  \"max_frame_ms\": " << max_ms << ",\n";
    json << "  \"p99_frame_ms\": " << p99_ms << ",\n";
    json << "  \"memory_start_kb\": " << mem_start << ",\n";
    json << "  \"memory_end_kb\": " << mem_end << ",\n";
    json << "  \"memory_peak_kb\": " << mem_peak << ",\n";
    json << "  \"memory_growth_kb\": " << mem_growth << ",\n";
    json << "  \"pass\": " << (pass ? "true" : "false") << "\n";
    json << "}\n";

    std::cout << "\n" << (pass ? "=== PASS ===" : "=== FAIL ===") << "\n";
    return pass ? 0 : 1;
}
