with open('src/engine/engine.cpp', 'r') as f:
    content = f.read()

# 1. Adicionar load_ddt_texture e load_portal_textures após load_core_ui_textures()
# Encontrar o final de load_core_ui_textures()
old = '''    LOGI("UI textures: splash=%u menu_bg=%u main_menu=%u btn=%u btn_pressed=%u",
         splash_tex_, loading_tex_, main_menu_tex_, button_main_tex_, button_clicked_tex_);
    return true;
}'''

new = '''    LOGI("UI textures: splash=%u menu_bg=%u main_menu=%u btn=%u btn_pressed=%u",
         splash_tex_, loading_tex_, main_menu_tex_, button_main_tex_, button_clicked_tex_);
    return true;
}

// ── Carrega textura DDT do textures.bar por nome (usado para portal, botões, etc.) ──
TextureHandle Engine::load_ddt_texture(const std::string& name) {
    if (!textures_bar_ || textures_bar_->file_count() == 0) return INVALID_TEXTURE;
    const std::vector<uint8_t> raw = textures_bar_->extract(name);
    if (raw.empty()) return INVALID_TEXTURE;
    auto ddt = DdtLoader::load(raw);
    if (!ddt) return INVALID_TEXTURE;
    auto rgba = ddt->decode_rgba(0);
    if (rgba.empty()) return INVALID_TEXTURE;
    TextureHandle t = renderer_->create_texture(ddt->width, ddt->height, rgba.data());
    if (t != INVALID_TEXTURE) {
        LOGI("Engine::load_ddt_texture: '%s' carregado (%dx%d)", name.c_str(), ddt->width, ddt->height);
    }
    return t;
}

// ── Carrega texturas do portal e logo do menu principal ──
void Engine::load_portal_textures() {
    if (!textures_bar_) {
        LOGW("load_portal_textures: textures_bar_ não disponível");
        return;
    }

    // Carregar textura do portal (estrutura de pedra)
    portal_tex_ = load_ddt_texture("ui\\portal_main.ddt");
    if (portal_tex_ == INVALID_TEXTURE)
        portal_tex_ = load_ddt_bar("ui\\portal.ddt");
    if (portal_tex_ == INVALID_TEXTURE)
        portal_tex_ = load_ddt_bar("ui\\main_menu_portal.ddt");
    if (portal_tex_ == INVALID_TEXTURE)
        portal_tex_ = load_ddt_bar("ui\\frame_portal.ddt");
    if (portal_tex_ != INVALID_TEXTURE) {
        LOGI("load_portal_textures: portal_tex_ carregado (%u)", portal_tex_);
    } else {
        LOGW("load_portal_textures: nenhuma textura de portal encontrada");
    }

    // Carregar textura do logo "Age of Mythology: The Titans"
    logo_tex_ = load_ddt_bar("ui\\logo_titans.ddt");
    if (logo_tex_ == INVALID_TEXTURE) {
        logo_tex_ = load_ddt_bar("ui\\logo_aom_titans.ddt");
    }
    if (logo_tex_ == INVALID_TEXTURE) {
        logo_tex_ = load_ddt_bar("ui\\title_main.ddt");
    }
    if (logo_tex_ == INVALID_TEXTURE) {
        logo_tex_ = load_ddt_bar("ui\\main_menu_title.ddt");
    }
    if (logo_tex_ != INVALID_TEXTURE) {
        LOGI("load_portal_textures: logo_tex_ carregado (%u)", logo_tex_);
    } else {
        LOGW("load_portal_textures: nenhuma textura de logo encontrada");
    }
}''')

if old not in content:
    print("AVISO: não encontrou o final de load_core_ui_textures para injeção")
else:
    content = content.replace(old, new, 1)

with open('src/engine/engine.cpp', 'w') as f:
    f.write(content)

print("OK: load_ddt_texture e load_portal_textures adicionados")

PYEOF
python3 /data/data/com.termux/files/home/documents/AoM-Native-Port/fix_engine.py