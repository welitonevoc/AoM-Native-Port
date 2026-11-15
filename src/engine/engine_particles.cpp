#include "engine/engine.h"
#include "engine_internal.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace aom {

void Engine::spawn_particle(const glm::vec3& pos, const glm::vec3& vel, float life, uint32_t color, float size) {
    // Encontrar slot inativo no pool (sem realocação)
    size_t slot = particles_.size();
    for (size_t i = 0; i < particles_.size(); ++i) {
        if (!particles_[i].active) { slot = i; break; }
    }
    if (slot == particles_.size()) return;  // pool cheio

    Particle& p = particles_[slot];
    p.active = true;
    p.pos = pos;
    p.vel = vel;
    p.life = 0.0f;
    p.max_life = (life > 0.05f) ? life : 0.6f;
    p.color = color;
    p.size = size;
}

void Engine::spawn_construction_sparks(const glm::vec3& pos, int count) {
    for (int i = 0; i < count; ++i) {
        float angle = static_cast<float>(rand() % 360) * (3.14159f / 180.0f);
        float speed = 1.0f + static_cast<float>(rand() % 100) * 0.02f;
        float vx = std::cos(angle) * speed;
        float vz = std::sin(angle) * speed;
        float vy = 1.5f + static_cast<float>(rand() % 100) * 0.025f;
        uint32_t colors[] = {0xFFFFEE44, 0xFFFFCC00, 0xFFFF9900, 0xFFEEDD88};
        uint32_t col = colors[rand() % 4];
        spawn_particle(pos + glm::vec3(vx * 0.2f, 0.2f, vz * 0.2f),
                       glm::vec3(vx, vy, vz),
                       0.5f + static_cast<float>(rand() % 50) * 0.01f,
                       col, 0.12f);
    }
}

void Engine::spawn_combat_hit(const glm::vec3& pos, int count) {
    for (int i = 0; i < count; ++i) {
        float theta = static_cast<float>(rand() % 360) * (3.14159f / 180.0f);
        float phi = static_cast<float>(rand() % 180 - 90) * (3.14159f / 180.0f);
        float speed = 2.0f + static_cast<float>(rand() % 100) * 0.03f;
        float vx = std::cos(phi) * std::cos(theta) * speed;
        float vy = std::abs(std::sin(phi)) * speed + 0.8f;
        float vz = std::cos(phi) * std::sin(theta) * speed;
        uint32_t colors[] = {0xFFFFFFFF, 0xFFFFDD33, 0xFFFF6622, 0xFFFF2222};
        uint32_t col = colors[rand() % 4];
        spawn_particle(pos, glm::vec3(vx, vy, vz),
                       0.35f + static_cast<float>(rand() % 40) * 0.01f,
                       col, 0.14f);
    }
}

void Engine::spawn_harvest_chips(ResourceType type, const glm::vec3& pos, int count) {
    uint32_t palette[3] = {0xFFFFD700, 0xFFFFE066, 0xFFFFF0AA};
    if (type == ResourceType::WOOD) {
        palette[0] = 0xFF8B5A2B; palette[1] = 0xFFCD853F; palette[2] = 0xFFD2B48C;
    } else if (type == ResourceType::FOOD) {
        palette[0] = 0xFF66BB33; palette[1] = 0xFF88DD44; palette[2] = 0xFFCCEE55;
    }

    for (int i = 0; i < count; ++i) {
        float angle = static_cast<float>(rand() % 360) * (3.14159f / 180.0f);
        float speed = 0.8f + static_cast<float>(rand() % 100) * 0.015f;
        float vx = std::cos(angle) * speed;
        float vz = std::sin(angle) * speed;
        float vy = 1.2f + static_cast<float>(rand() % 100) * 0.02f;
        spawn_particle(pos, glm::vec3(vx, vy, vz),
                       0.6f + static_cast<float>(rand() % 40) * 0.01f,
                       palette[rand() % 3], 0.10f);
    }
}

void Engine::spawn_death_smoke(const glm::vec3& pos, int count) {
    for (int i = 0; i < count; ++i) {
        float angle = static_cast<float>(rand() % 360) * (3.14159f / 180.0f);
        float speed = 0.4f + static_cast<float>(rand() % 60) * 0.01f;
        float vx = std::cos(angle) * speed;
        float vz = std::sin(angle) * speed;
        float vy = 0.8f + static_cast<float>(rand() % 80) * 0.015f;
        uint32_t colors[] = {0xAAEEEEFF, 0x88CCCCCC, 0x7799BBAA, 0x55888888};
        uint32_t col = colors[rand() % 4];
        spawn_particle(pos + glm::vec3(0.0f, 0.2f, 0.0f),
                       glm::vec3(vx, vy, vz),
                       1.2f + static_cast<float>(rand() % 60) * 0.01f,
                       col, 0.20f);
    }
}

void Engine::update_particles(float dt) {
    for (auto& p : particles_) {
        if (!p.active) continue;
        p.life += dt;
        p.pos += p.vel * dt;
        p.vel.y -= 9.8f * dt; // Gravity
        if (p.pos.y < 0.02f) {
            p.pos.y = 0.02f;
            p.vel.y = -p.vel.y * 0.35f; // Ground bounce
            p.vel.x *= 0.75f;           // Surface friction
            p.vel.z *= 0.75f;
        }
        if (p.life >= p.max_life) {
            p.active = false;  // Libera slot no pool
        }
    }
}

void Engine::render_particles() {
    if (!renderer_) return;
    for (const auto& p : particles_) {
        if (!p.active) continue;
        float progress = std::clamp(p.life / p.max_life, 0.0f, 1.0f);
        uint32_t alpha = static_cast<uint32_t>((1.0f - progress) * 255.0f);
        if (alpha == 0) continue;
        uint32_t col = (p.color & 0x00FFFFFF) | (alpha << 24);
        glm::vec3 tail = p.pos - p.vel * 0.04f;
        renderer_->draw_debug_line(tail, p.pos, col);
    }
}
} // namespace aom
