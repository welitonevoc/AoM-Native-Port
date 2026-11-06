#include "anim/grn_animation.h"
#include <iostream>
#include <cmath>

namespace {

static bool check(bool condition, const char* message) {
    std::cout << (condition ? "PASS: " : "FAIL: ") << message << '\n';
    return condition;
}

// Cria uma animação GRN sintética para teste
aom::anim::GrnAnimation make_test_animation() {
    aom::anim::GrnAnimation anim;
    anim.name = "test_walk.grn";
    anim.duration = 1.0f;
    anim.fps = 30.0f;

    // 3 ossos: raiz, quadril, joelho
    aom::anim::GrnBone root;
    root.name = "Root";
    root.parent_index = -1;
    root.bind_pose = glm::mat4(1.0f);
    anim.bones.push_back(root);

    aom::anim::GrnBone hip;
    hip.name = "Hip";
    hip.parent_index = 0;
    hip.bind_pose = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    anim.bones.push_back(hip);

    aom::anim::GrnBone knee;
    knee.name = "Knee";
    knee.parent_index = 1;
    knee.bind_pose = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    anim.bones.push_back(knee);
    anim.num_bones = 3;

    // Track do Root: translação ao longo do tempo
    aom::anim::GrnTrack root_track;
    root_track.bone_name = "Root";
    root_track.translation_keys = {
        {0.0f, {0.0f, 0.0f, 0.0f}},
        {0.5f, {1.0f, 0.0f, 0.0f}},
        {1.0f, {2.0f, 0.0f, 0.0f}},
    };
    anim.tracks.push_back(root_track);

    // Track do Hip: rotação (quaternão identidade, sem movimento)
    aom::anim::GrnTrack hip_track;
    hip_track.bone_name = "Hip";
    hip_track.rotation_keys = {
        {0.0f, glm::quat(1.0f, 0.0f, 0.0f, 0.0f)},
        {1.0f, glm::quat(1.0f, 0.0f, 0.0f, 0.0f)},
    };
    anim.tracks.push_back(hip_track);

    // Track do Knee: escala pulsante
    aom::anim::GrnTrack knee_track;
    knee_track.bone_name = "Knee";
    knee_track.scale_keys = {
        {0.0f, {1.0f, 1.0f, 1.0f}},
        {0.5f, {1.2f, 1.0f, 1.0f}},
        {1.0f, {1.0f, 1.0f, 1.0f}},
    };
    anim.tracks.push_back(knee_track);

    return anim;
}

} // namespace

int main() {
    std::cout << "=== P3.3 — GRN Animation Playback Test Suite ===\n";
    bool ok = true;

    // ── Test 1: Criação e avaliação de animação GRN sintética ──
    {
        auto anim = make_test_animation();
        ok &= check(anim.num_bones == 3, "animação tem 3 ossos");
        ok &= check(std::abs(anim.duration - 1.0f) < 0.001f, "duração é 1.0s");
        ok &= check(anim.find_bone("Root") == 0, "encontra osso Root no índice 0");
        ok &= check(anim.find_bone("Hip") == 1, "encontra osso Hip no índice 1");
        ok &= check(anim.find_bone("Knee") == 2, "encontra osso Knee no índice 2");
        ok &= check(anim.find_bone("Missing") == -1, "osso inexistente retorna -1");

        aom::anim::GrnPlayer player;
        ok &= check(player.set_animation(anim), "player carrega animação");
        ok &= check(std::abs(player.duration() - 1.0f) < 0.001f, "player reporta duração correta");

        std::vector<glm::mat4> transforms;
        ok &= check(player.evaluate(0.0f, transforms), "avalia em t=0");
        ok &= check(transforms.size() == 3, "retorna 3 matrizes de transformação");

        // Root deve estar na origem em t=0
        glm::vec3 root_pos = glm::vec3(transforms[0][3]);
        ok &= check(std::abs(root_pos.x) < 0.001f && std::abs(root_pos.y) < 0.001f &&
                    std::abs(root_pos.z) < 0.001f,
                    "Root na origem em t=0");

        // Avalia em t=0.5: Root deve estar em x=1.0
        ok &= check(player.evaluate(0.5f, transforms), "avalia em t=0.5");
        root_pos = glm::vec3(transforms[0][3]);
        ok &= check(std::abs(root_pos.x - 1.0f) < 0.01f,
                    "Root em x=1.0 em t=0.5 (interpolação linear)");

        // Avalia em t=1.0: Root deve estar em x=2.0
        ok &= check(player.evaluate(1.0f, transforms), "avalia em t=1.0");
        root_pos = glm::vec3(transforms[0][3]);
        ok &= check(std::abs(root_pos.x - 2.0f) < 0.01f,
                    "Root em x=2.0 em t=1.0");
    }

    // ── Test 2: Looping de animação ──
    {
        auto anim = make_test_animation();
        aom::anim::GrnPlayer player;
        player.set_animation(anim);

        std::vector<glm::mat4> transforms;
        // t=1.5 deve fazer loop para t=0.5 (duração 1.0)
        ok &= check(player.evaluate(1.5f, transforms), "avalia em t=1.5 com looping");
        glm::vec3 root_pos = glm::vec3(transforms[0][3]);
        ok &= check(std::abs(root_pos.x - 1.0f) < 0.01f,
                    "looping: Root em x=1.0 em t=1.5 (equivale a t=0.5)");
    }

    // ── Test 3: Hierarquia de ossos ──
    {
        auto anim = make_test_animation();
        aom::anim::GrnPlayer player;
        player.set_animation(anim);

        std::vector<glm::mat4> transforms;
        player.evaluate(0.0f, transforms);

        // Hip é filho de Root, então sua transformação deve incluir a do Root
        // Em t=0, Root está na origem e Hip está traduzido em y=1.0
        glm::vec3 hip_pos = glm::vec3(transforms[1][3]);
        ok &= check(std::abs(hip_pos.y - 1.0f) < 0.01f,
                    "Hip (filho de Root) tem posição y=1.0 em t=0");

        // Knee é filho de Hip
        glm::vec3 knee_pos = glm::vec3(transforms[2][3]);
        ok &= check(std::abs(knee_pos.z - 1.0f) < 0.01f,
                    "Knee (filho de Hip) tem posição z=1.0 em t=0");
    }

    // ── Test 4: Parse de arquivo GRN inválido ──
    {
        const uint8_t invalid_data[] = {0x00, 0x00, 0x00, 0x00};  // não é "GRN"
        auto anim = aom::anim::parse_grn(invalid_data, sizeof(invalid_data), "invalid.grn");
        ok &= check(anim == nullptr, "arquivo sem magic GRN retorna nullptr");
    }

    // ── Test 5: Dados muito pequenos ──
    {
        const uint8_t tiny_data[] = {'G', 'R', 'N'};
        auto anim = aom::anim::parse_grn(tiny_data, sizeof(tiny_data), "tiny.grn");
        ok &= check(anim == nullptr, "dados muito pequenos retornam nullptr");
    }

    std::cout << "\n=== P3.3 GRN Tests: " << (ok ? "ALL PASS (100%)" : "FAILURES DETECTED") << " ===\n";
    return ok ? 0 : 1;
}
