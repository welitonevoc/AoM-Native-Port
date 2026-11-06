#include "anim/prt_particle.h"
#include <fstream>
#include <sstream>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <random>

#ifdef __ANDROID__
#include <android/log.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  "AoM-PRT", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "AoM-PRT", __VA_ARGS__)
#else
#define LOGI(...) std::printf(__VA_ARGS__)
#define LOGE(...) std::fprintf(stderr, __VA_ARGS__)
#endif

namespace aom {

namespace {

// --- Helpers de parse de .prt ---
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

bool parse_bool(const std::string& s) {
    std::string l = trim(s);
    for (auto& c : l) c = static_cast<char>(::tolower(c));
    return l == "true" || l == "1" || l == "yes" || l == "on";
}

float parse_float(const std::string& s, float def = 0.0f) {
    try { return std::stof(trim(s)); } catch (...) { return def; }
}

uint32_t parse_color(const std::string& s) {
    std::string t = trim(s);
    // Aceita #RRGGBB ou RRGGBB ou 0xRRGGBB
    if (t.size() >= 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X')) {
        t = t.substr(2);
    }
    if (t.size() >= 1 && t[0] == '#') {
        t = t.substr(1);
    }
    if (t.size() == 6) {
        unsigned int r, g, b;
        if (std::sscanf(t.c_str(), "%02x%02x%02x", &r, &g, &b) == 3) {
            return (0xFFu << 24) | (static_cast<uint32_t>(r) << 16) |
                   (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
        }
    }
    if (t.size() == 8) {
        unsigned int a, r, g, b;
        if (std::sscanf(t.c_str(), "%02x%02x%02x%02x", &a, &r, &g, &b) == 4) {
            return (static_cast<uint32_t>(a) << 24) |
                   (static_cast<uint32_t>(r) << 16) |
                   (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
        }
    }
    return 0xFFFFFFFFu;
}

EmitterType parse_emitter_type(const std::string& s) {
    std::string l = trim(s);
    for (auto& c : l) c = static_cast<char>(::tolower(c));
    if (l == "sphere") return EmitterType::SPHERE;
    if (l == "cone") return EmitterType::CONE;
    if (l == "box") return EmitterType::BOX;
    return EmitterType::POINT;
}

} // anonymous namespace

// ============================================================================
// parse_prt_file
// ============================================================================

bool parse_prt_file(const std::string& prt_content, ParticleType& out_def) {
    std::istringstream stream(prt_content);
    std::string line;
    std::string current_section;

    while (std::getline(stream, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty()) continue;
        if (trimmed[0] == '#' || trimmed[0] == ';') continue;  // comentário

        // Seções: [Emitter], [Particle], [Render], etc.
        if (trimmed.front() == '[' && trimmed.back() == ']') {
            current_section = trim(trimmed.substr(1, trimmed.size() - 2));
            continue;
        }

        // Chave = Valor
        size_t eq = trimmed.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(trimmed.substr(0, eq));
        std::string val = trim(trimmed.substr(eq + 1));

        for (auto& c : key) c = static_cast<char>(::tolower(c));

        if (current_section == "Emitter" || current_section.empty()) {
            if (key == "name") out_def.name = val;
            else if (key == "emitter_type") out_def.emitter_type = parse_emitter_type(val);
            else if (key == "emission_rate") out_def.emission_rate = parse_float(val, 10.0f);
            else if (key == "initial_velocity_x") out_def.initial_velocity.x = parse_float(val);
            else if (key == "initial_velocity_y") out_def.initial_velocity.y = parse_float(val);
            else if (key == "initial_velocity_z") out_def.initial_velocity.z = parse_float(val);
            else if (key == "velocity_variation_x") out_def.velocity_variation.x = parse_float(val);
            else if (key == "velocity_variation_y") out_def.velocity_variation.y = parse_float(val);
            else if (key == "velocity_variation_z") out_def.velocity_variation.z = parse_float(val);
            else if (key == "acceleration_x") out_def.acceleration.x = parse_float(val);
            else if (key == "acceleration_y") out_def.acceleration.y = parse_float(val);
            else if (key == "acceleration_z") out_def.acceleration.z = parse_float(val);
        }
        else if (current_section == "Particle") {
            if (key == "life") out_def.particle_life = parse_float(val, 1.0f);
            else if (key == "size_start") out_def.size_start = parse_float(val, 0.15f);
            else if (key == "size_end") out_def.size_end = parse_float(val, 0.05f);
            else if (key == "color_start") out_def.color_start = parse_color(val);
            else if (key == "color_end") out_def.color_end = parse_color(val);
            else if (key == "size_variation") out_def.size_variation = parse_float(val, 0.0f);
            else if (key == "max_particles") out_def.max_particles = static_cast<int>(parse_float(val, 500.0f));
        }
        else if (current_section == "Emission") {
            if (key == "duration") out_def.emission_duration = parse_float(val, -1.0f);
            else if (key == "loop") out_def.loop = parse_bool(val);
        }
        else if (current_section == "Render") {
            if (key == "texture") out_def.texture_name = val;
        }
    }

    return !out_def.name.empty() || out_def.emission_rate > 0.0f;
}

// ============================================================================
// load_prt_definition
// ============================================================================

bool load_prt_definition(const std::string& prt_name, class BarArchive* archive,
                         ParticleType& out_def) {
    if (archive) {
        std::vector<uint8_t> data = archive->extract(prt_name);
        if (!data.empty()) {
            std::string content(data.begin(), data.end());
            return parse_prt_file(content, out_def);
        }
    }

    std::ifstream file(prt_name);
    if (file) {
        std::ostringstream ss;
        ss << file.rdbuf();
        return parse_prt_file(ss.str(), out_def);
    }

    LOGE("load_prt_definition: arquivo '%s' não encontrado", prt_name.c_str());
    return false;
}

// ============================================================================
// PrtParticleSystem
// ============================================================================

bool PrtParticleSystem::load_definition(const std::string& prt_name,
                                        class BarArchive* archive) {
    if (!load_prt_definition(prt_name, archive, def_)) return false;

    // Carrega textura se especificada
    if (!def_.texture_name.empty() && archive) {
        // Tentar carregar do archive (será conectado ao texture_cache do engine)
        // Por enquanto deixa INVALID_TEXTURE, o engine define depois
        texture_ = INVALID_TEXTURE;
    }

    LOGI("PrtParticleSystem: carregado '%s' — taxa=%.1f, vida=%.1fs, max=%d",
         def_.name.c_str(), def_.emission_rate, def_.particle_life, def_.max_particles);
    return true;
}

ActiveParticle PrtParticleSystem::spawn_particle(const glm::vec3& emitter_pos) {
    ActiveParticle p;
    p.max_life = def_.particle_life;
    p.life = 0.0f;
    p.pos = emitter_pos;
    p.size = def_.size_start;
    p.color = def_.color_start;
    p.active = true;

    // Velocidade base com variação aleatória
    std::random_device rd;
    std::mt19937 gen(rd());
    auto randf = [&](float min_v, float max_v) {
        return min_v + static_cast<float>(gen()) / static_cast<float>(gen.max()) * (max_v - min_v);
    };

    glm::vec3 vel = def_.initial_velocity;
    vel.x += randf(-def_.velocity_variation.x, def_.velocity_variation.x);
    vel.y += randf(-def_.velocity_variation.y, def_.velocity_variation.y);
    vel.z += randf(-def_.velocity_variation.z, def_.velocity_variation.z);

    // Para emissores SPHERE/CONE, direciona para fora
    if (def_.emitter_type == EmitterType::SPHERE) {
        float angle1 = randf(0.0f, 6.2831853f);
        float angle2 = randf(-1.5707963f, 1.5707963f);
        vel = glm::vec3(std::cos(angle1) * std::cos(angle2),
                        std::sin(angle2),
                        std::sin(angle1) * std::cos(angle2)) * glm::length(def_.initial_velocity);
    }

    p.vel = vel;
    return p;
}

uint32_t PrtParticleSystem::interpolate_color(float t) const {
    t = glm::clamp(t, 0.0f, 1.0f);
    auto unorm = [](uint32_t c, int shift) -> float {
        return static_cast<float>((c >> shift) & 0xFF) / 255.0f;
    };
    float r = unorm(def_.color_start, 16) * (1.0f - t) + unorm(def_.color_end, 16) * t;
    float g = unorm(def_.color_start, 8)  * (1.0f - t) + unorm(def_.color_end, 8)  * t;
    float b = unorm(def_.color_start, 0)  * (1.0f - t) + unorm(def_.color_end, 0)  * t;
    float a = unorm(def_.color_start, 24) * (1.0f - t) + unorm(def_.color_end, 24) * t;
    return (static_cast<uint32_t>(a * 255) << 24) |
           (static_cast<uint32_t>(r * 255) << 16) |
           (static_cast<uint32_t>(g * 255) << 8) |
           static_cast<uint32_t>(b * 255);
}

float PrtParticleSystem::interpolate_size(float t) const {
    t = glm::clamp(t, 0.0f, 1.0f);
    return def_.size_start * (1.0f - t) + def_.size_end * t;
}

void PrtParticleSystem::emit(float dt, const glm::vec3& emitter_pos) {
    if (def_.emission_rate <= 0.0f) return;

    emission_accumulator_ += def_.emission_rate * dt;

    // Verifica se ainda está emitindo (duração finita)
    if (def_.emission_duration >= 0.0f) {
        static float elapsed = 0.0f;
        elapsed += dt;
        if (elapsed > def_.emission_duration) return;
    }

    int to_emit = static_cast<int>(emission_accumulator_);
    if (to_emit <= 0) return;
    emission_accumulator_ -= static_cast<float>(to_emit);

    int space = def_.max_particles - static_cast<int>(active_particles_.size());
    if (space <= 0) return;
    if (to_emit > space) to_emit = space;

    for (int i = 0; i < to_emit; ++i) {
        active_particles_.push_back(spawn_particle(emitter_pos));
    }
}

void PrtParticleSystem::update(float dt) {
    for (auto& p : active_particles_) {
        if (!p.active) continue;
        p.life += dt;
        p.pos += p.vel * dt;
        p.vel += def_.acceleration * dt;

        // Quicar no chão (y = 0)
        if (p.pos.y < 0.02f) {
            p.pos.y = 0.02f;
            p.vel.y = -p.vel.y * 0.35f;
            p.vel.x *= 0.75f;
            p.vel.z *= 0.75f;
        }

        // Atualiza cor e tamanho com base na idade
        float t = p.life / p.max_life;
        p.color = interpolate_color(t);
        p.size = interpolate_size(t);

        if (p.life >= p.max_life) {
            p.active = false;
        }
    }

    // Remove partículas inativas
    active_particles_.erase(
        std::remove_if(active_particles_.begin(), active_particles_.end(),
                       [](const ActiveParticle& p) { return !p.active; }),
        active_particles_.end());
}

void PrtParticleSystem::render(IRenderer* renderer, TextureHandle default_tex) {
    if (!renderer || active_particles_.empty()) return;

    TextureHandle tex = (texture_ != INVALID_TEXTURE) ? texture_ : default_tex;
    if (tex == INVALID_TEXTURE) return;  // Sem textura, não há como renderizar partículas

    for (const auto& p : active_particles_) {
        if (!p.active) continue;
        // Desenha cada partícula como um quad (billboard) simples.
        // O renderer não tem draw_particle, então usamos draw_mesh com um quad unitário.
        // Implementação simplificada: desenha como um ponto/quad via debug_line ou
        // deixa para o engine fazer o batching.
        (void)p; (void)tex;
    }
}

} // namespace aom
