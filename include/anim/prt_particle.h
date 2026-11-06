#pragma once
/// @file prt_particle.h
/// @brief Parser e sistema de partículas a partir de arquivos .prt (Particle)
/// para AoM Native Port
///
/// Os arquivos .prt definem sistemas de partículas: emissores, taxas de
• emissão, duração, velocidade inicial, cor, tamanho, textura e comportamento.

#include "renderer/renderer.h"
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <glm/glm.hpp>

namespace aom {

/// Tipo de emissor de partículas
enum class EmitterType {
    POINT,      ///< Partículas emanam de um único ponto
    SPHERE,     ///< Partículas emanam dentro de uma esfera
    CONE,       ///< Partículas emanam em um cone
    BOX,        ///< Partículas emanam dentro de uma caixa
};

/// Definição de um tipo de partícula (do arquivo .prt)
struct ParticleType {
    std::string name;
    EmitterType emitter_type = EmitterType::POINT;
    float emission_rate = 10.0f;      ///< Partículas por segundo
    float particle_life = 1.0f;       ///< Tempo de vida de cada partícula (s)
    float emission_duration = -1.0f;  ///< Duração da emissão (<0 = infinito)
    glm::vec3 initial_velocity{0.0f, 1.0f, 0.0f};
    glm::vec3 velocity_variation{0.5f, 0.5f, 0.5f};
    glm::vec3 acceleration{0.0f, -9.8f, 0.0f};  ///< Gravidade
    float size_start = 0.15f;
    float size_end = 0.05f;
    uint32_t color_start = 0xFFFFFFFFu;
    uint32_t color_end = 0x80FFFFFFu;
    float size_variation = 0.0f;
    bool loop = true;
    int max_particles = 500;
    std::string texture_name;  ///< Nome da textura DDT no textures.bar
};

/// Partícula ativa em simulação
struct ActiveParticle {
    glm::vec3 pos{0.0f};
    glm::vec3 vel{0.0f};
    float life = 0.0f;
    float max_life = 1.0f;
    uint32_t color = 0xFFFFFFFFu;
    float size = 0.15f;
    bool active = true;
};

/// Sistema de partículas baseado em definição .prt
class PrtParticleSystem {
public:
    PrtParticleSystem() = default;

    /// Carrega definição de um arquivo .prt
    bool load_definition(const std::string& prt_name, class BarArchive* archive = nullptr);

    /// Emite partículas com base no tempo decorrido (dt em segundos)
    void emit(float dt, const glm::vec3& emitter_pos);

    /// Atualiza todas as partículas ativas (física, vida)
    void update(float dt);

    /// Renderiza partículas ativas (chamado pelo engine)
    void render(IRenderer* renderer, TextureHandle default_tex = INVALID_TEXTURE);

    /// Retorna contagem de partículas ativas
    size_t active_count() const { return active_particles_.size(); }

    /// Limpa todas as partículas
    void clear() { active_particles_.clear(); }

    const ParticleType& definition() const { return def_; }

private:
    ParticleType def_;
    std::vector<ActiveParticle> active_particles_;
    float emission_accumulator_ = 0.0f;
    TextureHandle texture_ = INVALID_TEXTURE;

    ActiveParticle spawn_particle(const glm::vec3& emitter_pos);
    uint32_t interpolate_color(float t) const;
    float interpolate_size(float t) const;
};

/// Faz parse de um arquivo .prt a partir de texto
bool parse_prt_file(const std::string& prt_content, ParticleType& out_def);

/// Carrega definição .prt de um BarArchive
bool load_prt_definition(const std::string& prt_name, class BarArchive* archive,
                         ParticleType& out_def);

} // namespace aom
