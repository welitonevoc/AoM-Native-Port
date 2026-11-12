#include "engine/engine.h"
#include "engine_internal.h"
#include "assets/brg_model.h"
#include "assets/bar_archive.h"
#include "assets/ddt_texture.h"
#include "assets/xmb_parser.h"
#include "assets/cube_map.h"
#include "anim/animation_system.h"
#include "anim/prt_particle.h"
#include "anim/grn_animation.h"
#include "renderer/renderer.h"
#include "data/game_data.h"
#include "data/sound_def.h"
#include "campaign/campaign.h"
#include "audio/audio.h"
#include "ui/ui.h"

extern "C" {
#include "stb_image.h"
}

#ifdef __ANDROID__
#include <android/asset_manager.h>
#endif

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace aom {

namespace {

TextureHandle create_procedural_grass(IRenderer* renderer) {
    if (!renderer) return INVALID_TEXTURE;
    const int W = 64, H = 64;
    std::vector<uint8_t> pixels(W * H * 4);
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            float n = std::sin(x * 0.4f) * std::cos(y * 0.4f) * 18.0f;
            float n2 = std::sin(x * 0.8f + y * 0.8f) * 10.0f;
            int r = std::clamp(static_cast<int>(60 + n * 0.5f), 0, 255);
            int g = std::clamp(static_cast<int>(130 + n + n2), 0, 255);
            int b = std::clamp(static_cast<int>(45 + n * 0.4f), 0, 255);
            size_t idx = (y * W + x) * 4;
            pixels[idx + 0] = static_cast<uint8_t>(r);
            pixels[idx + 1] = static_cast<uint8_t>(g);
            pixels[idx + 2] = static_cast<uint8_t>(b);
            pixels[idx + 3] = 255;
        }
    }
    return renderer->create_texture(W, H, pixels.data());
}

TextureHandle create_procedural_cliff(IRenderer* renderer) {
    if (!renderer) return INVALID_TEXTURE;
    const int W = 64, H = 64;
    std::vector<uint8_t> pixels(W * H * 4);
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            float n = std::sin(x * 0.6f) * std::cos(y * 0.3f) * 25.0f;
            float str = std::sin(y * 0.8f) * 15.0f;
            int v = std::clamp(static_cast<int>(115 + n + str), 0, 255);
            int r = std::clamp(static_cast<int>(v * 1.05f), 0, 255);
            int g = std::clamp(static_cast<int>(v * 0.95f), 0, 255);
            int b = std::clamp(static_cast<int>(v * 0.88f), 0, 255);
            size_t idx = (y * W + x) * 4;
            pixels[idx + 0] = static_cast<uint8_t>(r);
            pixels[idx + 1] = static_cast<uint8_t>(g);
            pixels[idx + 2] = static_cast<uint8_t>(b);
            pixels[idx + 3] = 255;
        }
    }
    return renderer->create_texture(W, H, pixels.data());
}

TextureHandle create_procedural_water_normal(IRenderer* renderer) {
    if (!renderer) return INVALID_TEXTURE;
    const int W = 64, H = 64;
    std::vector<uint8_t> pixels(W * H * 4);
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            float nx = std::sin(x * 0.35f) * 0.3f;
            float ny = std::cos(y * 0.35f) * 0.3f;
            glm::vec3 n = glm::normalize(glm::vec3(nx, ny, 1.0f));
            size_t idx = (y * W + x) * 4;
            pixels[idx + 0] = static_cast<uint8_t>((n.x * 0.5f + 0.5f) * 255.0f);
            pixels[idx + 1] = static_cast<uint8_t>((n.y * 0.5f + 0.5f) * 255.0f);
            pixels[idx + 2] = static_cast<uint8_t>((n.z * 0.5f + 0.5f) * 255.0f);
            pixels[idx + 3] = 255;
        }
    }
    return renderer->create_texture(W, H, pixels.data());
}

} // anonymous namespace

std::vector<uint8_t> Engine::read_asset(const std::string& name) {
#ifdef __ANDROID__
    if (asset_manager_) {
        auto* mgr = static_cast<AAssetManager*>(asset_manager_);

        std::string asset_name = name;
        if (asset_name.rfind("assets/", 0) == 0) {
            asset_name = asset_name.substr(7);
        }
        AAsset* asset = AAssetManager_open(mgr, asset_name.c_str(), AASSET_MODE_BUFFER);
        if (!asset && asset_name != name) {
            asset = AAssetManager_open(mgr, name.c_str(), AASSET_MODE_BUFFER);
        }
        if (asset) {
            off_t len = AAsset_getLength(asset);
            std::vector<uint8_t> buf(static_cast<size_t>(len));
            AAsset_read(asset, buf.data(), len);
            AAsset_close(asset);
            LOGI("read_asset loaded from APK: %s (%zu bytes)", asset_name.c_str(), buf.size());
            return buf;
        }
    }
#endif

    const std::string candidates[] = {
        name,
        "assets/" + name,
        config_.data_path + "/" + name,
        config_.data_path + "/assets/" + name,
        "/storage/emulated/0/Documents/AoM-Native-Port/assets/" + name,
        "/storage/emulated/0/Download/Games/PC/Age of Mythology/Age of Mythology/" + name
    };
    for (const auto& path : candidates) {
        std::ifstream file(path, std::ios::binary);
        if (file) {
            file.seekg(0, std::ios::end);
            size_t sz = static_cast<size_t>(file.tellg());
            file.seekg(0, std::ios::beg);
            std::vector<uint8_t> buf(sz);
            file.read(reinterpret_cast<char*>(buf.data()), sz);
            LOGI("read_asset loaded from FS: %s (%zu bytes)", path.c_str(), buf.size());
            return buf;
        }
    }
    LOGE("read_asset could not find: %s", name.c_str());
    return {};
}

bool Engine::load_core_ui_textures() {
    if (!renderer_) return false;

    // Helper: load PNG from asset, decode with stb_image
    auto load_png_asset = [&](const std::string& name) -> TextureHandle {
        auto bytes = read_asset(name);
        if (bytes.empty()) return INVALID_TEXTURE;
        int w = 0, h = 0, comp = 0;
        unsigned char* data = stbi_load_from_memory(
            bytes.data(), static_cast<int>(bytes.size()), &w, &h, &comp, 4);
        if (!data) return INVALID_TEXTURE;
        TextureHandle t = renderer_->create_texture(w, h, data);
        stbi_image_free(data);
        LOGI("Loaded PNG asset '%s' (%dx%d)", name.c_str(), w, h);
        return t;
    };

    // Helper: load DDT from textures.bar by name
    auto load_ddt_bar = [&](const std::string& name) -> TextureHandle {
        if (!textures_bar_ || textures_bar_->file_count() == 0) return INVALID_TEXTURE;
        const std::vector<uint8_t> raw = textures_bar_->extract(name);
        if (raw.empty()) return INVALID_TEXTURE;
        auto ddt = DdtLoader::load(raw);
        if (!ddt) return INVALID_TEXTURE;
        auto rgba = ddt->decode_rgba(0);
        if (rgba.empty()) return INVALID_TEXTURE;
        TextureHandle t = renderer_->create_texture(ddt->width, ddt->height, rgba.data());
        LOGI("Loaded DDT from bar '%s' (%dx%d)", name.c_str(), ddt->width, ddt->height);
        return t;
    };

    // --- 1. Splash / Intro background ---
    splash_tex_ = load_png_asset("splash.png");
    if (splash_tex_ == INVALID_TEXTURE)
        splash_tex_ = load_ddt_bar("loading screen.ddt");
    if (splash_tex_ == INVALID_TEXTURE)
        splash_tex_ = load_ddt_bar("loadingscreen.ddt");

    // --- 2. Loading screen background (1024x768 gods art) ---
    loading_tex_ = load_png_asset("loading.png");
    if (loading_tex_ == INVALID_TEXTURE)
        loading_tex_ = splash_tex_;

    // --- 3. Main Menu background (authentic stone frame + background) ---
    main_menu_tex_ = load_png_asset("main_menu.png");
    if (main_menu_tex_ == INVALID_TEXTURE)
        main_menu_tex_ = load_png_asset("mainmenu.png");
    // NÃO fazer fallback para loading_tex_ — MAIN_MENU usa cenário 3D, não imagem estática

    // --- 4. Authentic AoM Stone Buttons ---
    button_main_tex_ = load_png_asset("button_main.png");
    if (button_main_tex_ == INVALID_TEXTURE)
        button_main_tex_ = load_ddt_bar("greek button background.ddt");
    if (button_main_tex_ == INVALID_TEXTURE)
        button_main_tex_ = load_ddt_bar("ui\\button main.ddt");
    if (button_main_tex_ == INVALID_TEXTURE)
        button_main_tex_ = load_ddt_bar("icons\\ui_generic_button.ddt");

    button_clicked_tex_ = load_png_asset("button_clicked.png");
    if (button_clicked_tex_ == INVALID_TEXTURE)
        button_clicked_tex_ = load_ddt_bar("ui\\button main_clicked.ddt");
    if (button_clicked_tex_ == INVALID_TEXTURE)
        button_clicked_tex_ = button_main_tex_;

    if (ui_ && button_main_tex_ != INVALID_TEXTURE) {
        ui_->set_default_button_textures(button_main_tex_, button_clicked_tex_);
    }

    // --- 5. Font ---
    auto font_bytes = read_asset("font.ttf");
    LOGI("Font bytes read: %zu (from read_asset)", font_bytes.size());
    if (!font_bytes.empty()) {
        if (renderer_->set_font_data(font_bytes.data(), font_bytes.size())) {
            LOGI("Font loaded (%zu bytes)", font_bytes.size());
        } else {
            LOGE("Renderer rejected font data");
        }
    } else {
        LOGE("font.ttf not found — UI text will use built-in fallback");
    }

    LOGI("UI textures: splash=%u menu_bg=%u main_menu=%u btn=%u btn_pressed=%u",
         splash_tex_, loading_tex_, main_menu_tex_, button_main_tex_, button_clicked_tex_);
    return true;
}

bool Engine::load_animation_clips(size_t max_clips) {
    if (anim_clips_ready_ && anim_system_ && anim_system_->clip_count() > 0) {
        return true;  // já carregados em uma partida anterior
    }

    if (!anim_system_) {
        anim_system_ = std::make_unique<aom::anim::AnimationSystem>();
    }
    if (!models_bar_ || models_bar_->file_count() == 0) return false;

    const auto t0 = std::chrono::steady_clock::now();

    // Os nomes das entradas são baratos; só a extração dos dados é cara.
    // Prioriza os clips das unidades jogáveis e completa com os demais.
    static const char* kPreferred[] = {"hoplite", "villager", "toxotes", "minotaur",
                                        "archer",   "cavalry",  "infantry", "soldier"};

    std::vector<std::string> preferred;
    std::vector<std::string> fallback;
    for (const BarEntry& entry : models_bar_->entries()) {
        const std::string low = to_lower(entry.filename);
        if (!ends_with(low, ".brg")) continue;
        bool is_preferred = false;
        for (const char* pref : kPreferred) {
            if (low.find(pref) != std::string::npos) { is_preferred = true; break; }
        }
        if (is_preferred) {
            if (preferred.size() < max_clips) preferred.push_back(entry.filename);
        } else if (fallback.size() < max_clips) {
            fallback.push_back(entry.filename);
        }
        if (preferred.size() >= max_clips && fallback.size() >= max_clips) break;
    }

    std::vector<std::string> candidates = preferred;
    for (const std::string& name : fallback) {
        if (candidates.size() >= max_clips) break;
        candidates.push_back(name);
    }

    size_t loaded = 0;
    anim_clip_names_.clear();
    for (const std::string& name : candidates) {
        const BarEntry* entry = models_bar_->find(name);
        if (!entry) continue;
        const std::vector<uint8_t> data = models_bar_->extract(*entry);
        if (data.empty()) continue;
        if (anim_system_->load_brg_clip_from_memory(name, data.data(), data.size())) {
            anim_clip_names_.push_back(name);
            ++loaded;
        }
    }

    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - t0)
                                .count();
    LOGI("BRG clips: %zu carregados (de %zu candidatos) em %lld ms",
         loaded, candidates.size(), static_cast<long long>(elapsed_ms));

    anim_clips_ready_ = true;
    return loaded > 0;
}

bool Engine::load_grn_clips(size_t max_clips) {
    if (!anim_bar_ || anim_bar_->file_count() == 0) return false;

    const auto t0 = std::chrono::steady_clock::now();

    // Prioriza os clips das unidades jogáveis.
    static const char* kPreferred[] = {"hoplite", "villager", "toxotes", "minotaur",
                                        "archer",   "cavalry",  "infantry", "soldier"};

    std::vector<std::string> preferred;
    std::vector<std::string> fallback;
    for (const BarEntry& entry : anim_bar_->entries()) {
        const std::string low = to_lower(entry.filename);
        if (!ends_with(low, ".grn")) continue;
        bool is_preferred = false;
        for (const char* pref : kPreferred) {
            if (low.find(pref) != std::string::npos) { is_preferred = true; break; }
        }
        if (is_preferred) {
            if (preferred.size() < max_clips) preferred.push_back(entry.filename);
        } else if (fallback.size() < max_clips) {
            fallback.push_back(entry.filename);
        }
        if (preferred.size() >= max_clips && fallback.size() >= max_clips) break;
    }

    std::vector<std::string> candidates = preferred;
    for (const std::string& name : fallback) {
        if (candidates.size() >= max_clips) break;
        candidates.push_back(name);
    }

    size_t loaded = 0;
    grn_clips_.clear();
    for (const std::string& name : candidates) {
        const BarEntry* entry = anim_bar_->find(name);
        if (!entry) continue;
        const std::vector<uint8_t> data = anim_bar_->extract(*entry);
        if (data.empty()) continue;
        auto anim = aom::anim::parse_grn(data.data(), data.size(), name);
        if (anim && anim->is_valid()) {
            grn_clips_.emplace(name, std::move(anim));
            ++loaded;
        }
    }

    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - t0)
                                .count();
    LOGI("GRN clips: %zu carregados (de %zu candidatos) em %lld ms",
         loaded, candidates.size(), static_cast<long long>(elapsed_ms));

    return loaded > 0;
}

bool Engine::select_animation_clip(const aom::anim::BrgAnimationClip*& clip,
                                   TextureHandle& texture) const {
    clip = nullptr;
    texture = INVALID_TEXTURE;
    if (!anim_system_ || anim_clip_names_.empty()) return false;

    for (const std::string& name : anim_clip_names_) {
        const aom::anim::BrgAnimationClip* candidate = anim_system_->find_clip(name);
        if (!candidate || !candidate->is_valid()) continue;
        clip = candidate;

        if (renderer_ && textures_bar_ && textures_bar_->file_count() > 0) {
            const std::string base = name.substr(0, name.size() - 4);
            for (const char* ext : {".ddt", " diffuse.ddt"}) {
                const std::string tex_name = base + ext;
                const std::vector<uint8_t> tex_data = textures_bar_->extract(tex_name);
                if (tex_data.empty()) continue;
                auto ddt = DdtLoader::load(tex_data);
                if (!ddt) break;
                const auto rgba = ddt->decode_rgba(0);
                if (rgba.empty()) break;
                texture = renderer_->create_texture(ddt->width, ddt->height, rgba.data());
                LOGI("Loaded anim texture: %s", tex_name.c_str());
                break;
            }
        }
        break;
    }
    return clip != nullptr;
}

bool Engine::load_archives(const std::string& data_path) {
    namespace fs = std::filesystem;
    const fs::path base(data_path);

    data_bar_ = std::make_unique<BarArchive>();
    textures_bar_ = std::make_unique<BarArchive>();
    models_bar_ = std::make_unique<BarArchive>();
    scenarios_bar_ = std::make_unique<BarArchive>();
    sound_bar_ = std::make_unique<BarArchive>();
    anim_bar_ = std::make_unique<BarArchive>();
    godpowers_bar_ = std::make_unique<BarArchive>();

    auto find_archive = [](const fs::path& base, const std::string& sub, const std::string& name) -> fs::path {
        std::error_code ec;
        fs::path p1 = base / sub / name;
        if (fs::exists(p1, ec)) return p1;
        fs::path p2 = base / name;
        if (fs::exists(p2, ec)) return p2;
        return p1;
    };

    auto open_pair = [](BarArchive& bar, const fs::path& base_file, const fs::path& expansion_file) -> bool {
        std::error_code ec;
        bool ok = false;
        if (fs::exists(base_file, ec)) {
            ok = bar.open(base_file.string());
        }
        if (fs::exists(expansion_file, ec)) {
            if (bar.mount(expansion_file.string())) {
                ok = true;
            }
        }
        return ok;
    };

    open_pair(*data_bar_,
              find_archive(base, "data", "data.bar"),
              find_archive(base, "data", "data2.bar"));
    open_pair(*textures_bar_,
              find_archive(base, "textures", "textures.bar"),
              find_archive(base, "textures", "textures2.bar"));
    open_pair(*models_bar_,
              find_archive(base, "models", "models.bar"),
              find_archive(base, "models", "models2.bar"));
    open_pair(*scenarios_bar_,
              find_archive(base, "scenario", "scenarios.bar"),
              find_archive(base, "scenario", "scenarios2.bar"));
    open_pair(*sound_bar_,
              find_archive(base, "sound", "sounds.bar"),
              find_archive(base, "sound", "sounds2.bar"));
    open_pair(*anim_bar_,
              find_archive(base, "anim", "anim.bar"),
              find_archive(base, "anim", "anim2.bar"));
    open_pair(*godpowers_bar_,
              find_archive(base, "god powers", "godpowers.bar"),
              find_archive(base, "god powers", "godpowers2.bar"));

    if (audio_ && sound_bar_->is_open()) {
        audio_->set_sound_archive(sound_bar_.get());
    }

    if (sound_bar_->is_open()) {
        const BarEntry* ss_entry = sound_bar_->find("soundsets.xml");
        if (ss_entry) {
            const std::vector<uint8_t> ss_raw = sound_bar_->extract(*ss_entry);
            if (!ss_raw.empty()) {
                if (auto ss_root = XmbParser::parse(ss_raw)) {
                    if (sound_def_mgr_.load_soundsets_from_xmb(*ss_root)) {
                        LOGI("SoundDefManager: %zu soundsets carregados de soundsets.xml",
                             sound_def_mgr_.soundset_count());
                    }
                }
            }
        }
        const BarEntry* snd_entry = sound_bar_->find("protounit_snds.xml");
        if (!snd_entry) {
            snd_entry = sound_bar_->find("protounit_snds2.xml");
        }
        if (snd_entry) {
            const std::vector<uint8_t> snd_raw = sound_bar_->extract(*snd_entry);
            if (!snd_raw.empty()) {
                if (auto snd_root = XmbParser::parse(snd_raw)) {
                    if (sound_def_mgr_.load_protounit_sounds_from_xmb(*snd_root)) {
                        LOGI("SoundDefManager: %zu protounit sound defs carregados de *_snds.xml",
                             sound_def_mgr_.protounit_sound_count());
                    }
                }
            }
        }
    }

    {
        const fs::path gp_base = find_archive(base, "god powers", "godpowers.bar");
        const fs::path gp_exp = find_archive(base, "god powers", "godpowers2.bar");
        std::error_code ec;
        if (fs::exists(gp_base, ec)) {
            BarArchive gp;
            if (gp.open(gp_base.string())) {
                god_powers_.load_from_bar(&gp, false);
            }
        }
        if (fs::exists(gp_exp, ec)) {
            BarArchive gp;
            if (gp.open(gp_exp.string())) {
                god_powers_.load_expansion_from_bar(&gp);
            }
        }
    }

    campaign_mgr_ = std::make_unique<CampaignManager>();

    bool base_campaign_loaded = false;
    const std::string cpn_candidates[] = {
        data_path + "/data/main campaign.cpn",
        data_path + "/main campaign.cpn",
        "/storage/emulated/0/Download/Games/PC/Age of Mythology/Age of Mythology/data/main campaign.cpn"
    };
    for (const auto& c : cpn_candidates) {
        if (campaign_mgr_->load_campaign(c)) {
            LOGI("Loaded campaign from disk '%s': %zu scenarios", c.c_str(), campaign_mgr_->scenario_count());
            base_campaign_loaded = true;
            break;
        }
    }
    if (!base_campaign_loaded) {
        auto cpn_bytes = read_asset("main campaign.cpn");
        if (!cpn_bytes.empty()) {
            campaign_mgr_->load_campaign_from_memory(cpn_bytes.data(), cpn_bytes.size());
            LOGI("Loaded campaign from APK assets: %zu scenarios", campaign_mgr_->scenario_count());
        }
    }

    campaign_mgr_xpack_ = std::make_unique<CampaignManager>();
    const std::string cpx_candidates[] = {
        data_path + "/data/xpack campaign.cpx",
        data_path + "/xpack campaign.cpx",
        "/storage/emulated/0/Download/Games/PC/Age of Mythology/Age of Mythology/data/xpack campaign.cpx"
    };
    for (const auto& c : cpx_candidates) {
        if (campaign_mgr_xpack_->load_campaign(c)) {
            LOGI("Loaded Titans campaign '%s': %zu scenarios", c.c_str(),
                 campaign_mgr_xpack_->scenario_count());
            break;
        }
    }

    archives_loaded_ = (textures_bar_->file_count() > 0 || data_bar_->file_count() > 0);
    LOGI("Loaded archives: data=%zu, textures=%zu, models=%zu, scenarios=%zu, sounds=%zu, "
         "anim=%zu, godpowers=%zu (valid=%d)",
         data_bar_->file_count(), textures_bar_->file_count(), models_bar_->file_count(),
         scenarios_bar_->file_count(), sound_bar_->file_count(), anim_bar_->file_count(),
         godpowers_bar_->file_count(), archives_loaded_ ? 1 : 0);
    return true;
}

bool Engine::load_game_config() {
    if (data_bar_ && data_bar_->file_count() > 0) {
        if (game_data_.load_from_bar(*data_bar_)) {
            LOGI("Game data registry: %zu units, %zu gods, %zu techs",
                 game_data_.unit_count(), game_data_.god_count(), game_data_.tech_count());
        }

        const BarEntry* entry = data_bar_->find("proto.xmb");
        if (entry) {
            const std::vector<uint8_t> raw = data_bar_->extract(*entry);
            if (!raw.empty()) {
                auto root = XmbParser::parse(raw);
                if (root) {
                    LOGI("Game config loaded from BAR: root <%s>, %zu entities", root->name.c_str(), root->children.size());
                    game_config_root_ = std::move(root);
                    return true;
                }
            }
        }
    }

    auto proto_bytes = read_asset("proto.xmb");
    if (!proto_bytes.empty()) {
        auto root = XmbParser::parse(proto_bytes);
        if (root) {
            LOGI("Game config loaded from APK asset proto.xmb: root <%s>, %zu entities", root->name.c_str(), root->children.size());
            game_config_root_ = std::move(root);
            return true;
        }
    }
    return false;
}

bool Engine::load_textures() {
    if (!renderer_) return false;

    if (textures_bar_ && textures_bar_->file_count() > 0) {
        if (terrain_texture_ == INVALID_TEXTURE) {
            std::vector<uint8_t> data = textures_bar_->extract("terrain/grassa.ddt");
            if (data.empty()) data = textures_bar_->extract("terrain/dirta.ddt");
            if (!data.empty()) {
                auto ddt = DdtLoader::load(data);
                if (ddt) {
                    terrain_texture_ = renderer_->create_texture(*ddt);
                    LOGI("Loaded terrain base texture from BAR (handle %u)", terrain_texture_);
                }
            }
        }

        if (terrain_blend_tex_ == INVALID_TEXTURE) {
            std::vector<uint8_t> data = textures_bar_->extract("terrain/cliffgreeka.ddt");
            if (data.empty()) data = textures_bar_->extract("terrain/cliffa.ddt");
            if (!data.empty()) {
                auto ddt = DdtLoader::load(data);
                if (ddt) {
                    terrain_blend_tex_ = renderer_->create_texture(*ddt);
                    LOGI("Loaded terrain blend/cliff texture from BAR (handle %u)", terrain_blend_tex_);
                }
            }
        }

        if (water_normal_tex_ == INVALID_TEXTURE) {
            std::vector<uint8_t> data = textures_bar_->extract("terrain/waterbump.ddt");
            if (!data.empty()) {
                auto ddt = DdtLoader::load(data);
                if (ddt) {
                    water_normal_tex_ = renderer_->create_texture(*ddt);
                    LOGI("Loaded water normal map from BAR (handle %u)", water_normal_tex_);
                }
            }
        }
    }

    if (terrain_texture_ == INVALID_TEXTURE) {
        terrain_texture_ = create_procedural_grass(renderer_.get());
        LOGI("Created procedural terrain grass texture (handle %u)", terrain_texture_);
    }
    if (terrain_blend_tex_ == INVALID_TEXTURE) {
        terrain_blend_tex_ = create_procedural_cliff(renderer_.get());
        LOGI("Created procedural terrain cliff texture (handle %u)", terrain_blend_tex_);
    }
    if (water_normal_tex_ == INVALID_TEXTURE) {
        water_normal_tex_ = create_procedural_water_normal(renderer_.get());
        LOGI("Created procedural water normal texture (handle %u)", water_normal_tex_);
    }

    if (water_tex_ == INVALID_TEXTURE && textures_bar_) {
        for (const char* candidate : {
                 "water/water1.ddt",
                 "water/water2.ddt",
                 "water/water3.ddt",
                 "water/water.ddt",
                 "terrain/water1.ddt",
                 "terrain/water2.ddt",
                 "terrain/water.ddt",
                 "water.ddt",
             }) {
            std::vector<uint8_t> data = textures_bar_->extract(candidate);
            if (!data.empty()) {
                auto ddt = DdtLoader::load(data);
                if (ddt) {
                    water_tex_ = renderer_->create_texture(*ddt);
                    LOGI("Loaded water texture from BAR: '%s' (handle %u)", candidate, water_tex_);
                    break;
                }
            }
        }
        if (water_tex_ == INVALID_TEXTURE) {
            LOGW("No water texture found in textures.bar, shader will use procedural surface");
        }
    }

    if (skybox_mesh_ == INVALID_MESH && textures_bar_) {
        for (const char* cub_name : {"sfx a shiny water.cub", "sfx shinygold map.cub", "sfx shinyice map.cub"}) {
            if (load_skybox_faces(cub_name)) break;
        }
    }

    if (skydome_texture_ == INVALID_TEXTURE && textures_bar_ && skybox_mesh_ == INVALID_MESH) {
        for (const char* cub_name : {"sfx a shiny water.cub", "sfx shinygold map.cub", "sfx shinyice map.cub"}) {
            if (load_skybox_cube(cub_name)) break;
        }
    }

    if (textures_bar_) {
        load_prt_particles("particles/fire_torch.prt");
        load_prt_particles("particles/combat_hit.prt");
        load_prt_particles("particles/construction_sparks.prt");
        load_prt_particles("particles/death_smoke.prt");
        load_prt_particles("particles/harvest_chips.prt");
    }

    return true;
}

bool Engine::load_skybox_cube(const std::string& cub_name) {
    if (!renderer_) return false;
    current_cubemap_ = load_cub_definition(cub_name, textures_bar_.get());
    if (!current_cubemap_.is_valid()) {
        LOGW("load_skybox_cube: definicao de cubemap '%s' nao encontrada ou invalida", cub_name.c_str());
        return false;
    }

    LOGI("load_skybox_cube: carregando cubemap '%s' (Up='%s', Front='%s')",
         cub_name.c_str(), current_cubemap_.up.c_str(), current_cubemap_.front.c_str());

    std::string tex_name = !current_cubemap_.up.empty() ? current_cubemap_.up : current_cubemap_.front;
    if (textures_bar_ && !tex_name.empty()) {
        for (const std::string& candidate : {tex_name + ".ddt", tex_name + " diffuse.ddt", tex_name}) {
            std::vector<uint8_t> data = textures_bar_->extract(candidate);
            if (!data.empty()) {
                auto ddt = DdtLoader::load(data);
                if (ddt) {
                    skydome_texture_ = renderer_->create_texture(*ddt);
                    LOGI("load_skybox_cube: textura do skydome '%s' carregada com sucesso (handle %u)",
                         candidate.c_str(), skydome_texture_);
                    return true;
                }
            }
        }
    }
    return false;
}

bool Engine::load_skybox_faces(const std::string& cub_name) {
    if (!renderer_ || !textures_bar_) return false;

    current_cubemap_ = load_cub_definition(cub_name, textures_bar_.get());
    if (!current_cubemap_.is_valid()) {
        LOGW("load_skybox_faces: definicao de cubemap '%s' nao encontrada ou invalida", cub_name.c_str());
        return false;
    }

    LOGI("load_skybox_faces: carregando cubemap '%s' com 6 faces", cub_name.c_str());

    struct FaceEntry {
        const std::string& face_name;
        TextureHandle& handle;
    };

    const std::array<FaceEntry, 6> faces = {{
        {current_cubemap_.up,    skybox_faces_[0]},
        {current_cubemap_.down,  skybox_faces_[1]},
        {current_cubemap_.front, skybox_faces_[2]},
        {current_cubemap_.back,  skybox_faces_[3]},
        {current_cubemap_.left,  skybox_faces_[4]},
        {current_cubemap_.right, skybox_faces_[5]},
    }};

    const char* face_names[6] = {"up", "down", "front", "back", "left", "right"};
    int loaded_count = 0;

    for (int i = 0; i < 6; ++i) {
        const auto& fe = faces[i];
        if (fe.face_name.empty()) {
            LOGW("load_skybox_faces: face '%s' sem nome definido no .cub", face_names[i]);
            continue;
        }

        bool loaded = false;
        for (const std::string& cand : {
                 fe.face_name + ".ddt",
                 fe.face_name + " diffuse.ddt",
                 fe.face_name + "_diffuse.ddt",
                 fe.face_name,
             }) {
            std::vector<uint8_t> data = textures_bar_->extract(cand);
            if (!data.empty()) {
                auto ddt = DdtLoader::load(data);
                if (ddt) {
                    fe.handle = renderer_->create_texture(*ddt);
                    LOGI("load_skybox_faces: face '%s' = '%s' (handle %u)",
                         face_names[i], cand.c_str(), fe.handle);
                    loaded = true;
                    break;
                }
            }
        }
        if (!loaded) {
            LOGW("load_skybox_faces: nao foi possivel carregar textura para face '%s'", face_names[i]);
        } else {
            ++loaded_count;
        }
    }

    if (loaded_count == 0) {
        LOGE("load_skybox_faces: nenhuma face de skybox carregada para '%s'", cub_name.c_str());
        return false;
    }

    if (renderer_) {
        std::array<std::vector<uint8_t>, 6> faces_rgba;
        uint32_t w = 0, h = 0;
        bool all_faces_ok = true;

        for (int i = 0; i < 6; ++i) {
            const auto& fe = faces[i];
            if (fe.face_name.empty()) { all_faces_ok = false; break; }

            std::vector<uint8_t> data;
            for (const std::string& cand : {
                     fe.face_name + ".ddt",
                     fe.face_name + " diffuse.ddt",
                     fe.face_name + "_diffuse.ddt",
                     fe.face_name,
                 }) {
                data = textures_bar_->extract(cand);
                if (!data.empty()) break;
            }
            if (data.empty()) { all_faces_ok = false; break; }

            auto ddt = DdtLoader::load(data);
            if (!ddt) { all_faces_ok = false; break; }

            auto rgba = ddt->decode_rgba(0);
            if (rgba.empty()) { all_faces_ok = false; break; }

            if (w == 0) { w = ddt->width; h = ddt->height; }
            else if (w != ddt->width || h != ddt->height) { all_faces_ok = false; break; }

            faces_rgba[i] = std::move(rgba);
        }

        if (all_faces_ok) {
            TextureHandle cubemap = renderer_->create_cubemap_from_rgba(faces_rgba, w, h);
            if (cubemap != INVALID_TEXTURE) {
                skybox_cubemap_ = cubemap;
                LOGI("load_skybox_faces: cubemap texture criada via renderer (handle %u)", skybox_cubemap_);
            } else {
                LOGW("load_skybox_faces: renderer falhou ao criar cubemap");
            }
        } else {
            LOGW("load_skybox_faces: falha ao preparar faces para cubemap, faces 2D ainda disponiveis");
        }
    }

    create_skybox_mesh();
    LOGI("load_skybox_faces: %d/6 faces carregadas, skybox mesh criado", loaded_count);
    return true;
}

void Engine::load_water_body_textures(const std::string& body_name) {
    if (!textures_bar_) return;

    const auto& wb = game_data().water_bodies();
    std::string bank_name, outer_bank_name;

    if (const auto* river = wb.find_river(body_name); river) {
        bank_name = river->bank;
        outer_bank_name = river->outer_bank;
        LOGI("load_water_body_textures: rio '%s' bank='%s' outer_bank='%s'",
             body_name.c_str(), bank_name.c_str(), outer_bank_name.c_str());
    } else if (const auto* ocean = wb.find_ocean(body_name); ocean) {
        bank_name = ocean->bank;
        outer_bank_name = ocean->outer_bank;
        LOGI("load_water_body_textures: oceano '%s' bank='%s' outer_bank='%s'",
             body_name.c_str(), bank_name.c_str(), outer_bank_name.c_str());
    } else if (const auto* lake = wb.find_lake(body_name); lake) {
        bank_name = lake->bank;
        outer_bank_name = lake->outer_bank;
        LOGI("load_water_body_textures: lago '%s' bank='%s' outer_bank='%s'",
             body_name.c_str(), bank_name.c_str(), outer_bank_name.c_str());
    } else {
        LOGW("load_water_body_textures: corpo d'agua '%s' nao encontrado no waterbodies.xmb",
             body_name.c_str());
        return;
    }

    if (!bank_name.empty()) {
        terrain_bank_tex_ = load_ddt_texture(bank_name);
        if (terrain_bank_tex_ != INVALID_TEXTURE) {
            LOGI("load_water_body_textures: bank texture '%s' -> handle %u",
                 bank_name.c_str(), terrain_bank_tex_);
        }
    }
    if (!outer_bank_name.empty()) {
        terrain_outer_bank_tex_ = load_ddt_texture(outer_bank_name);
        if (terrain_outer_bank_tex_ != INVALID_TEXTURE) {
            LOGI("load_water_body_textures: outer_bank texture '%s' -> handle %u",
                 outer_bank_name.c_str(), terrain_outer_bank_tex_);
        }
    }
}

bool Engine::load_prt_particles(const std::string& prt_name) {
    if (!renderer_ || !textures_bar_) return false;

    prt_particles_ = std::make_unique<aom::PrtParticleSystem>();
    if (!prt_particles_->load_definition(prt_name, textures_bar_.get())) {
        LOGW("load_prt_particles: falha ao carregar '%s'", prt_name.c_str());
        prt_particles_.reset();
        return false;
    }

    const auto& def = prt_particles_->definition();
    if (!def.texture_name.empty()) {
        for (const std::string& cand : {
                 def.texture_name + ".ddt",
                 def.texture_name + " diffuse.ddt",
                 def.texture_name + "_diffuse.ddt",
                 def.texture_name,
             }) {
            prt_particle_tex_ = load_ddt_texture(cand);
            if (prt_particle_tex_ != INVALID_TEXTURE) break;
        }
    }

    LOGI("load_prt_particles: '%s' carregado (textura=%u)",
         prt_name.c_str(), prt_particle_tex_);
    return true;
}

bool Engine::load_models() {
    if (!renderer_ || !models_bar_) return false;

    int loaded = 0;
    for (const BarEntry& entry : models_bar_->entries()) {
        if (!ends_with(to_lower(entry.filename), ".brg")) continue;

        auto model = load_brg_model_data(models_bar_.get(), entry.filename);
        if (!model || model->meshes.empty()) continue;

        const auto& m = model->meshes[0];
        MeshHandle mesh = renderer_->create_mesh(
            m.vertices.data(), static_cast<uint32_t>(m.vertices.size()),
            m.indices.data(), static_cast<uint32_t>(m.indices.size()));
        if (mesh == INVALID_MESH) continue;

        TextureHandle tex = INVALID_TEXTURE;
        std::string base = entry.filename;
        if (ends_with(to_lower(base), ".brg")) {
            base = base.substr(0, base.size() - 4);
        }
        for (const std::string& cand : {
                 base + ".ddt",
                 base + " diffuse.ddt",
                 base + "_diffuse.ddt",
                 base + ".bti",
             }) {
            tex = load_ddt_texture(cand);
            if (tex != INVALID_TEXTURE) break;
        }

        const ProtoUnitData* unit_stats = nullptr;
        std::string base_name = entry.filename;
        if (ends_with(to_lower(base_name), ".brg")) {
            base_name = base_name.substr(0, base_name.size() - 4);
        }
        unit_stats = game_data_.proto_units().find(base_name);

        ModelEntry& entry_cache = model_cache_[entry.filename];
        entry_cache.mesh = mesh;
        entry_cache.texture = tex;
        entry_cache.unit_stats = unit_stats;
        model_cache_models_[entry.filename] = std::move(model);
        entry_cache.model = model_cache_models_[entry.filename].get();

        if (loaded == 0) {
            model_mesh_ = mesh;
            model_texture_ = tex;
        }
        ++loaded;
        LOGI("Cached BRG model: %s (mesh=%u, texture=%u)",
             entry.filename.c_str(), mesh, tex);
    }

    LOGI("load_models: %d modelos BRG em cache", loaded);
    return loaded > 0;
}

void Engine::build_texture_key_index() {
    texture_key_index_.clear();
    texture_key_index_built_ = true;
    if (!textures_bar_) return;
    for (const BarEntry& e : textures_bar_->entries()) {
        const std::string& n = e.filename;
        if (n.size() < 4) continue;
        std::string ext = n.substr(n.size() - 4);
        for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (ext != ".ddt") continue;
        texture_key_index_.emplace(texture_key(n), n);
    }
    LOGI("[TEX] índice tolerante de texturas: %zu .ddt de %zu entradas",
         texture_key_index_.size(), textures_bar_->file_count());
}

void Engine::log_bar_names_containing(const std::string& needle, size_t max_lines) const {
    if (!textures_bar_) return;
    const std::string want = texture_key(needle);
    size_t shown = 0;
    for (const BarEntry& e : textures_bar_->entries()) {
        if (texture_key(e.filename).find(want) == std::string::npos) continue;
        LOGI("[TEX] entrada no BAR: '%s'", e.filename.c_str());
        if (++shown >= max_lines) { LOGI("[TEX] ... (limite de %zu linhas)", max_lines); break; }
    }
    if (shown == 0) LOGW("[TEX] nenhuma entrada do textures.bar contém '%s'", needle.c_str());
}

TextureHandle Engine::load_ddt_texture(const std::string& material_name) {
    if (!renderer_ || !textures_bar_) return INVALID_TEXTURE;
    auto it = texture_cache_.find(material_name);
    if (it != texture_cache_.end()) return it->second;

    std::string ddt_name = material_name;
    if (ddt_name.find('.') == std::string::npos) ddt_name += ".ddt";

    std::vector<uint8_t> data = textures_bar_->extract(ddt_name);
    std::string resolved = ddt_name;

    if (data.empty()) {
        if (!texture_key_index_built_) build_texture_key_index();
        const auto k = texture_key_index_.find(texture_key(material_name));
        if (k != texture_key_index_.end()) {
            data = textures_bar_->extract(k->second);
            resolved = k->second;
            if (!data.empty()) {
                LOGI("[TEX] '%s' resolvida por nome aproximado -> '%s'",
                     material_name.c_str(), k->second.c_str());
            }
        }
    }

    if (data.empty()) {
        if (texture_miss_logged_.insert(material_name).second) {
            LOGW("[TEX] NAO ENCONTRADA: '%s' (tentado '%s', chave '%s')",
                 material_name.c_str(), ddt_name.c_str(), texture_key(material_name).c_str());
            std::string probe = material_name;
            const size_t slash = probe.find_last_of("\\/");
            if (slash != std::string::npos) probe.erase(0, slash + 1);
            log_bar_names_containing(probe.substr(0, std::min<size_t>(probe.size(), 14)), 6);
        }
        return INVALID_TEXTURE;
    }

    auto ddt = DdtLoader::load(data);
    if (!ddt) { LOGW("[TEX] '%s' (%s): DdtLoader::load falhou", material_name.c_str(), resolved.c_str()); return INVALID_TEXTURE; }
    auto rgba = ddt->decode_rgba(0);
    if (rgba.empty()) { LOGW("[TEX] '%s' (%s): decode_rgba vazio (formato %d)", material_name.c_str(), resolved.c_str(), int(ddt->format)); return INVALID_TEXTURE; }

    TextureHandle handle = renderer_->create_texture(
        ddt->width, ddt->height, rgba.data());
    if (handle != INVALID_TEXTURE) {
        texture_cache_[material_name] = handle;
        texture_dimensions_[material_name] = glm::ivec2(
            static_cast<int>(ddt->width), static_cast<int>(ddt->height));
    }
    return handle;
}

bool Engine::load_lighting(const std::string& path) {
    (void)path;
    return true;
}

bool Engine::load_strings(const std::string& path) {
    (void)path;
    return true;
}

std::string Engine::tr(const std::string& key) const {
    auto it = strings_.find(key);
    if (it != strings_.end()) return it->second;
    return key;
}

void Engine::load_ui_strings() {
    strings_ = {
        {"MENU_LEARN_TO_PLAY", "Aprender a Jogar"},
        {"MENU_CAMPAIGN", "Campanha"},
        {"MENU_SINGLE_PLAYER", "Jogador Único"},
        {"MENU_MULTIPLAYER", "Multijogador"},
        {"MENU_OPTIONS", "Opções"},
        {"MENU_MORE", "Mais"},
        {"MENU_EXIT", "Sair"},

        {"HUD_FOOD", "Comida"},
        {"HUD_WOOD", "Madeira"},
        {"HUD_GOLD", "Ouro"},
        {"HUD_FAVOR", "Favor"},
        {"HUD_POP", "Pop"},
        {"HUD_SELECT_UNIT", "Toque em uma unidade para selecionar"},
        {"HUD_NONE_SELECTED", "Nenhuma unidade selecionada"},
        {"HUD_SELECTED_COUNT", "selecionadas"},
        {"HUD_BUILD_ON_TERRAIN", "Toque no terreno para construir!"},
        {"HUD_MENU_BUTTON", "MENU"},

        {"CMD_MOVER", "Mover"},
        {"CMD_ATACAR", "Atacar"},
        {"CMD_COLETAR", "Coletar"},
        {"CMD_CONSTRUIR", "Construir"},
        {"CMD_TREINAR", "Treinar"},
        {"CMD_CENTRO", "Centro"},

        {"VICTORY_TITLE", "VITÓRIA"},
        {"DEFEAT_TITLE", "DERROTA"},
        {"BTN_MAIN_MENU", "MENU PRINCIPAL"},
        {"BTN_PLAY_AGAIN", "JOGAR NOVAMENTE"},

        {"SAVE_LOAD_TITLE", "CARREGAR JOGO"},
        {"SAVE_GAME", "Salvar Jogo"},
        {"LOAD_GAME", "Carregar Jogo"},

        {"SETTINGS_AUDIO", "Áudio"},
        {"SETTINGS_MUSIC", "Música"},
        {"SETTINGS_SFX", "SFX"},
    };
    LOGI("load_ui_strings: %zu strings carregadas para localização", strings_.size());
}

ModelEntry* Engine::get_model_entry(const std::string& name) {
    auto it = model_cache_.find(name);
    return (it != model_cache_.end()) ? &it->second : nullptr;
}

} // namespace aom
