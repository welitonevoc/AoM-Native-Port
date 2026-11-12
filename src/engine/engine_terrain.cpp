#include "engine/engine.h"
#include "engine_internal.h"
#include "assets/bar_archive.h"
#include "data/water_bodies.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <vector>

namespace aom {

void Engine::set_water_color_from_body(const std::string& body_name) {
    const auto& wb = game_data_.water_bodies();
    glm::vec3 color{0, 50, 170}; // default

    if (const auto* river = wb.find_river(body_name); river) {
        color = river->color / 255.0f;
    } else if (const auto* ocean = wb.find_ocean(body_name); ocean) {
        color = ocean->color / 255.0f;
    } else if (const auto* lake = wb.find_lake(body_name); lake) {
        color = lake->color / 255.0f;
    }

    water_color_ = color;
    LOGI("set_water_color_from_body: '%s' -> RGB(%.0f, %.0f, %.0f)",
         body_name.c_str(), water_color_.r * 255.0f, water_color_.g * 255.0f, water_color_.b * 255.0f);
}

void Engine::spawn_water_body_clusters(const std::string& body_name, const glm::vec3& center) {
    if (!gameplay_world_ || !textures_bar_) return;

    const auto& wb = game_data().water_bodies();
    const RiverDef* river = wb.find_river(body_name);
    const OceanDef* ocean = wb.find_ocean(body_name);
    const LakeDef* lake = wb.find_lake(body_name);

    const std::vector<ObjectCluster>* clusters = nullptr;
    float curve_angle = 15.0f;
    float distance_between = 30.0f;
    float distance_variation = 10.0f;
    bool lock_to_water_edge = false;

    if (river) {
        clusters = &river->object_clusters;
        curve_angle = river->curve_angle;
        distance_between = river->distance_between;
        distance_variation = river->distance_variation;
        lock_to_water_edge = river->lock_to_water_edge;
    } else if (ocean) {
        clusters = &ocean->object_clusters;
        curve_angle = 0.0f; // oceanos não têm objectplacement no XML de referência
        distance_between = 30.0f;
        distance_variation = 10.0f;
    } else if (lake) {
        clusters = &lake->object_clusters;
        curve_angle = lake->curve_angle;
        distance_between = lake->distance_between;
        distance_variation = lake->distance_variation;
        lock_to_water_edge = lake->lock_to_water_edge;
    } else {
        LOGW("spawn_water_body_clusters: corpo d'agua '%s' nao encontrado", body_name.c_str());
        return;
    }

    if (clusters->empty()) return;

    // Número de posições ao redor do centro: baseado no perímetro e distance_between.
    // Usamos um raio de referência de ~distance_between * 2 para clusters próximos à beira.
    const float radius = distance_between * 1.5f;
    const int num_positions = std::max(4, static_cast<int>(6.28318f * radius / distance_between));

    // Semente determinística baseada no nome do corpo d'água
    uint32_t seed = 0;
    for (char c : body_name) seed = seed * 31u + static_cast<uint8_t>(c);

    int spawned = 0;
    for (const auto& cluster : *clusters) {
        for (int i = 0; i < num_positions; ++i) {
            // Ângulo distribuído ao redor do centro, com variação baseada em curve_angle
            float angle = (6.28318f * i) / static_cast<float>(num_positions);
            if (curve_angle > 0.0f) {
                angle += glm::radians(curve_angle * 0.5f);
            }

            // Variação de distância determinística
            seed = seed * 1103515245u + 12345u;
            float dist_var = (static_cast<float>(seed % 1000) / 1000.0f - 0.5f) * 2.0f * distance_variation;
            float dist = radius + dist_var;

            // Posição no mundo (Y = 0, o terreno vai ajustar via heightmap se necessário)
            glm::vec3 pos = center + glm::vec3(
                std::cos(angle) * dist,
                0.0f,
                std::sin(angle) * dist
            );

            // Limita ao mundo jogável (aprox. ±16 unidades conforme is_walkable)
            if (std::abs(pos.x) > 15.0f || std::abs(pos.z) > 15.0f) continue;

            for (const auto& proto_name : cluster.protounits) {
                // Tenta carregar a textura do protounit do textures.bar
                TextureHandle tex = load_ddt_texture(proto_name);
                if (tex == INVALID_TEXTURE) {
                    // Fallback: tenta com sufixo .ddt
                    tex = load_ddt_texture(proto_name + ".ddt");
                }

                gameplay_world_->spawn_scenery_object(proto_name, pos, 1.0f, tex);
                ++spawned;
            }
        }
    }

    LOGI("spawn_water_body_clusters: '%s' -> %d objetos spawnados (%d clusters, %d posicoes)",
         body_name.c_str(), spawned, static_cast<int>(clusters->size()), num_positions);
}

void Engine::create_skybox_mesh() {
    if (!renderer_) return;
    if (skybox_mesh_ != INVALID_MESH) return;

    const float R = 65.0f;
    std::vector<Vertex> verts;
    std::vector<uint32_t> indices;

    // Cubo com 6 faces, normais apontando para dentro (para vista de dentro para fora)
    // Face: UP (y = +R)
    {
        glm::vec3 corners[4] = {{-R, R, -R}, {R, R, -R}, {R, R, R}, {-R, R, R}};
        glm::vec3 normal(0.0f, 1.0f, 0.0f);
        // Ordem invertida para normais para dentro (vista interna)
        add_skybox_quad(verts, indices, corners[0], corners[3], corners[2], corners[1], normal,
                        glm::vec2(0,0), glm::vec2(1,0), glm::vec2(1,1), glm::vec2(0,1));
    }
    // Face: DOWN (y = -R)
    {
        glm::vec3 corners[4] = {{-R, -R, R}, {R, -R, R}, {R, -R, -R}, {-R, -R, -R}};
        glm::vec3 normal(0.0f, -1.0f, 0.0f);
        add_skybox_quad(verts, indices, corners[0], corners[3], corners[2], corners[1], normal,
                        glm::vec2(0,0), glm::vec2(1,0), glm::vec2(1,1), glm::vec2(0,1));
    }
    // Face: FRONT (z = +R)
    {
        glm::vec3 corners[4] = {{-R, -R, R}, {R, -R, R}, {R, R, R}, {-R, R, R}};
        glm::vec3 normal(0.0f, 0.0f, 1.0f);
        add_skybox_quad(verts, indices, corners[0], corners[3], corners[2], corners[1], normal,
                        glm::vec2(0,0), glm::vec2(1,0), glm::vec2(1,1), glm::vec2(0,1));
    }
    // Face: BACK (z = -R)
    {
        glm::vec3 corners[4] = {{R, -R, -R}, {-R, -R, -R}, {-R, R, -R}, {R, R, -R}};
        glm::vec3 normal(0.0f, 0.0f, -1.0f);
        add_skybox_quad(verts, indices, corners[0], corners[3], corners[2], corners[1], normal,
                        glm::vec2(0,0), glm::vec2(1,0), glm::vec2(1,1), glm::vec2(0,1));
    }
    // Face: LEFT (x = -R)
    {
        glm::vec3 corners[4] = {{-R, -R, -R}, {-R, -R, R}, {-R, R, R}, {-R, R, -R}};
        glm::vec3 normal(-1.0f, 0.0f, 0.0f);
        add_skybox_quad(verts, indices, corners[0], corners[3], corners[2], corners[1], normal,
                        glm::vec2(0,0), glm::vec2(1,0), glm::vec2(1,1), glm::vec2(0,1));
    }
    // Face: RIGHT (x = +R)
    {
        glm::vec3 corners[4] = {{R, -R, R}, {R, -R, -R}, {R, R, -R}, {R, R, R}};
        glm::vec3 normal(1.0f, 0.0f, 0.0f);
        add_skybox_quad(verts, indices, corners[0], corners[3], corners[2], corners[1], normal,
                        glm::vec2(0,0), glm::vec2(1,0), glm::vec2(1,1), glm::vec2(0,1));
    }

    skybox_mesh_ = renderer_->create_mesh(verts.data(), static_cast<uint32_t>(verts.size()),
                                          indices.data(), static_cast<uint32_t>(indices.size()));
    LOGI("create_skybox_mesh: %zu verts, %zu indices", verts.size(), indices.size());
}

void Engine::add_skybox_quad(std::vector<Vertex>& verts, std::vector<uint32_t>& indices,
                             const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2,
                             const glm::vec3& p3, const glm::vec3& normal,
                             const glm::vec2& t0, const glm::vec2& t1,
                             const glm::vec2& t2, const glm::vec2& t3) {
    uint32_t base = static_cast<uint32_t>(verts.size());
    verts.push_back({p0, normal, t0, glm::vec4(1.0f)});
    verts.push_back({p1, normal, t1, glm::vec4(1.0f)});
    verts.push_back({p2, normal, t2, glm::vec4(1.0f)});
    verts.push_back({p3, normal, t3, glm::vec4(1.0f)});
    indices.push_back(base + 0);
    indices.push_back(base + 1);
    indices.push_back(base + 2);
    indices.push_back(base + 0);
    indices.push_back(base + 2);
    indices.push_back(base + 3);
}

void Engine::generate_terrain() {
    if (!renderer_) return;
    load_textures();
    LOGI("[GLHANDLES] terreno: base=%u blend=%u bank=%u agua=%u | UI: splash=%u loading=%u menu=%u botao=%u",
         terrain_texture_, terrain_blend_tex_, terrain_bank_tex_, water_normal_tex_,
         splash_tex_, loading_tex_, main_menu_tex_, button_main_tex_);
    if (terrain_texture_ != INVALID_TEXTURE &&
        (terrain_texture_ == splash_tex_ || terrain_texture_ == loading_tex_ ||
         terrain_texture_ == main_menu_tex_ || terrain_texture_ == button_main_tex_)) {
        LOGE("[GLHANDLES] COLISÃO: a textura do terreno tem o mesmo handle de uma textura de UI (handle obsoleto)");
    }

    const int GRID = 48;
    const float SIZE = 48.0f;
    const float STEP = SIZE / static_cast<float>(GRID);
    const float HALF = SIZE * 0.5f;

    auto calc_elevation = [](float wx, float wz) -> float {
        // Base rolling terrain
        float hills = std::sin(wx * 0.16f) * std::cos(wz * 0.16f) * 0.75f;
        // Central river valley dipping below water level (-0.4f)
        float river_dist = std::abs(wz - std::sin(wx * 0.14f) * 3.5f);
        float river = (river_dist < 5.0f) ? ((river_dist - 5.0f) * 0.18f) : 0.0f;
        // Elevated cliff/plateau on corner
        float cliff = (wx > 6.0f && wz < -4.0f) ? (std::sin((wx - 6.0f) * 0.28f) * 2.2f) : 0.0f;
        return hills + river + cliff;
    };

    std::vector<TerrainVertex> verts;
    verts.reserve((GRID + 1) * (GRID + 1));
    std::vector<uint32_t> indices;
    indices.reserve(GRID * GRID * 6);

    for (int z = 0; z <= GRID; ++z) {
        for (int x = 0; x <= GRID; ++x) {
            float wx = -HALF + x * STEP;
            float wz = -HALF + z * STEP;
            float wy = calc_elevation(wx, wz);

            // Finite difference normal computation
            float hL = calc_elevation(wx - STEP, wz);
            float hR = calc_elevation(wx + STEP, wz);
            float hD = calc_elevation(wx, wz - STEP);
            float hU = calc_elevation(wx, wz + STEP);
            glm::vec3 normal = glm::normalize(glm::vec3(hL - hR, 2.0f * STEP, hD - hU));

            // Vertex color & slope blend weight
            float slope = std::clamp(1.0f - normal.y, 0.0f, 1.0f);
            float alpha = std::clamp(1.0f - slope * 2.8f, 0.0f, 1.0f);
            float tint = 0.92f + std::sin(wx * 0.4f + wz * 0.4f) * 0.08f;

            TerrainVertex v;
            v.position = glm::vec3(wx, wy, wz);
            v.normal = normal;
            v.texcoord = glm::vec2(static_cast<float>(x) / GRID * 8.0f, static_cast<float>(z) / GRID * 8.0f);
            v.color = glm::vec4(tint, tint, tint, alpha);
            verts.push_back(v);
        }
    }

    for (int z = 0; z < GRID; ++z) {
        for (int x = 0; x < GRID; ++x) {
            uint32_t tl = z * (GRID + 1) + x;
            uint32_t tr = tl + 1;
            uint32_t bl = (z + 1) * (GRID + 1) + x;
            uint32_t br = bl + 1;

            indices.push_back(tl);
            indices.push_back(bl);
            indices.push_back(tr);

            indices.push_back(tr);
            indices.push_back(bl);
            indices.push_back(br);
        }
    }

    terrain_mesh_ = renderer_->create_terrain_mesh(verts.data(), static_cast<uint32_t>(verts.size()),
                                                   indices.data(), static_cast<uint32_t>(indices.size()));
    LOGI("Generated authentic AoM 3D terrain mesh (%zu verts, %zu indices)", verts.size(), indices.size());
}

// ── P2.1: Terreno Real — Loader de Height-Map de Cenários .scn ───────────────
bool Engine::load_scenario_heightmap(const std::string& scenario_path) {
    if (!renderer_) return false;
    if (terrain_texture_ == INVALID_TEXTURE) {
        load_textures();
    }

    // Ler arquivo do scenarios.bar ou do filesystem
    std::vector<uint8_t> data;
    if (scenarios_bar_ && scenarios_bar_->file_count() > 0) {
        // Preferir o cenário pedido: a campanha usa 'spc\aomsp04.scn' e o
        // height-map precisa ser o do mapa realmente carregado. Sem isso,
        // o primeiro '.scn' do arquivo era usado e o terreno não batia com a
        // partida (ou nem existia, pois o archive usa separadores '\').
        const BarEntry* wanted = scenarios_bar_->find(scenario_path);
        if (!wanted) {
            std::string alt = scenario_path;
            for (char& c : alt) { if (c == '/') c = '\\'; else if (c == '\\') c = '/'; }
            wanted = scenarios_bar_->find(alt);
        }
        if (!wanted) {
            size_t slash = scenario_path.find_last_of("/\\");
            if (slash != std::string::npos) {
                wanted = scenarios_bar_->find(scenario_path.substr(slash + 1));
            }
        }
        if (wanted) {
            data = scenarios_bar_->extract(*wanted);
            if (!data.empty()) {
                LOGI("Height-map: usando o cenario pedido '%s'", wanted->filename.c_str());
            }
        }
        if (data.empty()) {
            // Fallback: primeiro '.scn' ou '.scx' disponivel no archive
            for (const BarEntry& entry : scenarios_bar_->entries()) {
                std::string fn = to_lower(entry.filename);
                if (ends_with(fn, ".scn") || ends_with(fn, ".scx")) {
                    data = scenarios_bar_->extract(entry);
                    if (!data.empty()) {
                        LOGW("Height-map: cenario '%s' nao encontrado no BAR, usando '%s'",
                             scenario_path.c_str(), entry.filename.c_str());
                        break;
                    }
                }
            }
        }
    }
    if (data.empty()) {
        // Fallback: tentar ler diretamente do filesystem
        std::vector<std::string> fs_candidates = {
            scenario_path,
            "assets/scenario/" + scenario_path,
            config_.data_path + "/scenario/" + scenario_path,
            "/sdcard/Documents/AoM/scenario/" + scenario_path,
            "/sdcard/AoM/scenario/" + scenario_path
        };
        for (const auto& p : fs_candidates) {
            std::ifstream file(p, std::ios::binary);
            if (file) {
                file.seekg(0, std::ios::end);
                size_t sz = static_cast<size_t>(file.tellg());
                file.seekg(0, std::ios::beg);
                data.resize(sz);
                file.read(reinterpret_cast<char*>(data.data()), sz);
                if (!data.empty()) break;
            }
        }
    }

    if (data.size() < 64) {
        LOGE("Scenario file too small for height-map: %zu bytes", data.size());
        return false;
    }

    // Verificar magic do formato .scn (R3SG / SM4 / RTS3.exe)
    bool has_r3sg = false, has_sm4 = false, has_rts3 = false;
    for (size_t i = 0; i + 4 <= data.size() && i < 1024; ++i) {
        if (std::memcmp(data.data() + i, "R3SG", 4) == 0) has_r3sg = true;
        if (std::memcmp(data.data() + i, "SM4", 3) == 0) has_sm4 = true;
        if (std::memcmp(data.data() + i, "RTS3", 4) == 0) has_rts3 = true;
    }

    if (!has_r3sg && !has_sm4 && !has_rts3) {
        LOGE("Scenario file does not contain valid AoM .scn magic");
        return false;
    }

    LOGI("Scenario magic found: R3SG=%d SM4=%d RTS3=%d, size=%zu bytes",
         has_r3sg, has_sm4, has_rts3, data.size());

    // ── Extrair height-map do formato .scn ──────────────────────────────────
    // O formato .scn do Age of Mythology armazena o height-map como uma grade
    // de valores de elevação (float32 ou int16) após o cabeçalho.
    // A localização exata varia entre versões, então tentamos múltiplos offsets.

    const int HM_WIDTH = 64;
    const int HM_HEIGHT = 64;
    const size_t HM_COUNT = static_cast<size_t>(HM_WIDTH) * HM_HEIGHT;
    const size_t HM_DATA_SIZE = HM_COUNT * sizeof(float);

    // Procurar por dados de height-map após o cabeçalho.
    //
    // A busca valida a GRADE INTEIRA, não apenas o primeiro valor: um único
    // float "razoável" aparece por acaso em qualquer blob binário, e acceptá-lo
    // gerava um terreno com elevações de ~1e38 (cena invisível e vértices
    // degenerados). Só é aceito um offset em que todos os 4096 valores são
    // finitos, cabem numa faixa de elevação plausível e apresentam variação
    // real — um grid constante é padding/zeros e não um mapa.
    constexpr float kMinElevation = -500.0f;
    constexpr float kMaxElevation = 3000.0f;
    constexpr float kMinVariation = 0.05f;

    auto read_value = [&](size_t offset, size_t index, bool as_int16) -> float {
        if (as_int16) {
            int16_t raw;
            std::memcpy(&raw, data.data() + offset + index * sizeof(int16_t), sizeof(int16_t));
            return static_cast<float>(raw);
        }
        float raw;
        std::memcpy(&raw, data.data() + offset + index * sizeof(float), sizeof(float));
        return raw;
    };

    // Amostragem rápida: descarta o offset logo que um valor fora da faixa
    // aparece, sem varrer a grade inteira (o .scn tem ~1 MB e a busca é feita
    // sobre cada byte). A amostragem também exige variação entre os valores
    // — regiões de padding (zeros) são rejeitadas de imediato, o que evita
    // varreduras completas sobre áreas chapadas do arquivo.
    auto sample_is_sane = [&](size_t offset, bool as_int16) -> bool {
        static const size_t kSamples[] = {0, 1, 1023, 1024, 2047, 2048, 4094, 4095};
        float lo = 0.0f;
        float hi = 0.0f;
        for (size_t i = 0; i < sizeof(kSamples) / sizeof(kSamples[0]); ++i) {
            const float v = read_value(offset, kSamples[i], as_int16);
            if (as_int16) {
                if (v < -5000.0f || v > 30000.0f) return false;
            } else if (!std::isfinite(v) || v < kMinElevation || v > kMaxElevation) {
                return false;
            }
            if (i == 0) { lo = hi = v; }
            else { lo = std::min(lo, v); hi = std::max(hi, v); }
        }
        return (hi - lo) > kMinVariation;
    };

    auto grid_is_sane = [&](size_t offset, bool as_int16) -> bool {
        float lo = 0.0f;
        float hi = 0.0f;
        for (size_t i = 0; i < HM_COUNT; ++i) {
            const float v = read_value(offset, i, as_int16);
            if (as_int16) {
                if (v < -5000.0f || v > 30000.0f) return false;
            } else if (!std::isfinite(v) || v < kMinElevation || v > kMaxElevation) {
                return false;
            }
            if (i == 0) { lo = hi = v; }
            else { lo = std::min(lo, v); hi = std::max(hi, v); }
        }
        // Uma região constante (padding/zeros) não é um mapa.
        return (hi - lo) > kMinVariation;
    };

    size_t hm_offset = std::string::npos;
    bool hm_is_int16 = false;

    // A busca fica restrita aos primeiros 512 KB do .scn: no archive do AoM
    // os dados do mapa estão nessa faixa, e limitar a janela mantém o custo
    // previsível na thread da UI (o setup da partida não pode se aproximar
    // do limite de ANR do Android).
    constexpr size_t kScanWindow = 512 * 1024;
    const size_t scan_end = std::min(data.size(), kScanWindow);

    for (size_t offset = 128; offset + HM_DATA_SIZE <= scan_end; offset += 4) {
        if (!sample_is_sane(offset, false)) continue;
        if (grid_is_sane(offset, false)) {
            hm_offset = offset;
            break;
        }
    }

    if (hm_offset == std::string::npos) {
        // Tentar ler como int16 (formato alternativo)
        const size_t HM_DATA_SIZE_16 = HM_COUNT * sizeof(int16_t);
        for (size_t offset = 128; offset + HM_DATA_SIZE_16 <= scan_end; offset += 2) {
            if (!sample_is_sane(offset, true)) continue;
            if (grid_is_sane(offset, true)) {
                hm_offset = offset;
                hm_is_int16 = true;
                break;
            }
        }
        if (hm_offset == std::string::npos) {
            LOGW("Could not locate a sane height-map grid in .scn file (%zu bytes)", data.size());
            return false;
        }
    }

    // Extrair dados de elevação
    heightmap_width_ = HM_WIDTH;
    heightmap_height_ = HM_HEIGHT;
    heightmap_scale_ = hm_is_int16 ? 0.1f : 1.0f;
    heightmap_data_.resize(HM_COUNT);

    if (hm_is_int16) {
        // Formato int16
        for (size_t i = 0; i < HM_COUNT; ++i) {
            int16_t val;
            std::memcpy(&val, data.data() + hm_offset + i * sizeof(int16_t), sizeof(int16_t));
            heightmap_data_[i] = static_cast<float>(val) * heightmap_scale_;
        }
    } else {
        // Formato float32
        for (size_t i = 0; i < HM_COUNT; ++i) {
            std::memcpy(&heightmap_data_[i], data.data() + hm_offset + i * sizeof(float), sizeof(float));
            if (!std::isfinite(heightmap_data_[i])) heightmap_data_[i] = 0.0f;
        }
    }

    LOGI("Loaded height-map from .scn: %dx%d, offset=%zu, escala=%.2f, range=[%.2f, %.2f]",
         heightmap_width_, heightmap_height_, hm_offset, heightmap_scale_,
         *std::min_element(heightmap_data_.begin(), heightmap_data_.end()),
         *std::max_element(heightmap_data_.begin(), heightmap_data_.end()));

    // Gerar malha de terreno a partir do height-map
    generate_terrain_from_heightmap(heightmap_data_, heightmap_width_, heightmap_height_, 1.0f);

    // Atualizar NavigationGrid com base no height-map (encostas íngremes = intransponíveis)
    if (gameplay_world_) {
        gameplay_world_->rebuild_navigation_from_heightmap(
            heightmap_data_, heightmap_width_, heightmap_height_, 1.0f);
    }

    return true;
}

void Engine::generate_terrain_from_heightmap(const std::vector<float>& heightmap, int width, int height, float scale) {
    if (!renderer_) return;
    if (width <= 0 || height <= 0 || heightmap.empty()) return;

    const float STEP = scale;
    const float HALF_W = (width * STEP) * 0.5f;
    const float HALF_H = (height * STEP) * 0.5f;

    // Normaliza a amplitude vertical para a mesma escala do terreno
    // procedural (~±3 unidades). As elevações do .scn vêm em unidades
    // próprias do formato e podem chegar a centenas ou milhares; sem esta
    // normalização a malha ficaria com picos verticais e a câmera ficaria
    // dentro ou fora do terreno.
    std::vector<float> heights = heightmap;
    const float h_min = *std::min_element(heights.begin(), heights.end());
    const float h_max = *std::max_element(heights.begin(), heights.end());
    constexpr float kTargetAmplitude = 3.0f;
    if (h_max > h_min) {
        const float factor = kTargetAmplitude / (h_max - h_min);
        const float mid = (h_max + h_min) * 0.5f;
        for (float& h : heights) h = (h - mid) * factor;
    } else {
        std::fill(heights.begin(), heights.end(), 0.0f);
    }

    std::vector<TerrainVertex> verts;
    verts.reserve(static_cast<size_t>(width) * height);
    std::vector<uint32_t> indices;
    indices.reserve(static_cast<size_t>(width) * height * 6);

    auto get_height = [&](int x, int z) -> float {
        if (x < 0 || x >= width || z < 0 || z >= height) return 0.0f;
        return heights[static_cast<size_t>(z) * width + x];
    };

    for (int z = 0; z < height; ++z) {
        for (int x = 0; x < width; ++x) {
            float wx = -HALF_W + x * STEP;
            float wz = -HALF_H + z * STEP;
            float wy = get_height(x, z);

            // Finite difference normal computation
            float hL = get_height(x - 1, z);
            float hR = get_height(x + 1, z);
            float hD = get_height(x, z - 1);
            float hU = get_height(x, z + 1);
            glm::vec3 normal = glm::normalize(glm::vec3(hL - hR, 2.0f * STEP, hD - hU));

            // Vertex color & slope blend weight (splatting: grama em áreas planas, rocha em encostas)
            float slope = std::clamp(1.0f - normal.y, 0.0f, 1.0f);
            float alpha = std::clamp(1.0f - slope * 2.8f, 0.0f, 1.0f); // Alpha para blend de textura
            float tint = 0.92f;

            TerrainVertex v;
            v.position = glm::vec3(wx, wy, wz);
            v.normal = normal;
            v.texcoord = glm::vec2(static_cast<float>(x) / width * 8.0f,
                                   static_cast<float>(z) / height * 8.0f);
            v.color = glm::vec4(tint, tint, tint, alpha);
            verts.push_back(v);
        }
    }

    for (int z = 0; z < height - 1; ++z) {
        for (int x = 0; x < width - 1; ++x) {
            uint32_t tl = static_cast<uint32_t>(z * width + x);
            uint32_t tr = tl + 1;
            uint32_t bl = static_cast<uint32_t>((z + 1) * width + x);
            uint32_t br = bl + 1;

            indices.push_back(tl);
            indices.push_back(bl);
            indices.push_back(tr);

            indices.push_back(tr);
            indices.push_back(bl);
            indices.push_back(br);
        }
    }

    terrain_mesh_ = renderer_->create_terrain_mesh(verts.data(), static_cast<uint32_t>(verts.size()),
                                                   indices.data(), static_cast<uint32_t>(indices.size()));
    LOGI("Generated terrain from height-map: %dx%d, %zu verts, %zu indices",
         width, height, verts.size(), indices.size());
}

void Engine::generate_skydome() {
    if (!renderer_) return;
    const int SLICES = 24;
    const int STACKS = 12;
    const float RADIUS = 65.0f;

    std::vector<Vertex> verts;
    std::vector<uint32_t> indices;

    for (int i = 0; i <= STACKS; ++i) {
        float phi = (static_cast<float>(i) / STACKS) * (3.14159265f * 0.5f);
        float cos_phi = std::cos(phi);
        float sin_phi = std::sin(phi);
        float ratio = static_cast<float>(i) / STACKS;

        glm::vec4 col = glm::mix(glm::vec4(0.85f, 0.80f, 0.68f, 1.0f), glm::vec4(0.20f, 0.45f, 0.85f, 1.0f), ratio);

        for (int j = 0; j <= SLICES; ++j) {
            float theta = (static_cast<float>(j) / SLICES) * 2.0f * 3.14159265f;
            float cos_theta = std::cos(theta);
            float sin_theta = std::sin(theta);

            Vertex v;
            v.position = glm::vec3(RADIUS * cos_phi * cos_theta, RADIUS * sin_phi - 5.0f, RADIUS * cos_phi * sin_theta);
            v.normal = glm::normalize(-v.position);
            v.texcoord = glm::vec2(static_cast<float>(j) / SLICES, ratio);
            v.color = col;
            verts.push_back(v);
        }
    }

    for (int i = 0; i < STACKS; ++i) {
        for (int j = 0; j < SLICES; ++j) {
            uint32_t first = i * (SLICES + 1) + j;
            uint32_t second = first + SLICES + 1;

            indices.push_back(first);
            indices.push_back(second);
            indices.push_back(first + 1);

            indices.push_back(first + 1);
            indices.push_back(second);
            indices.push_back(second + 1);
        }
    }

    skydome_mesh_ = renderer_->create_mesh(verts.data(), static_cast<uint32_t>(verts.size()),
                                           indices.data(), static_cast<uint32_t>(indices.size()));
    LOGI("Generated celestial skydome mesh (%zu verts, %zu indices)", verts.size(), indices.size());
}

} // namespace aom
