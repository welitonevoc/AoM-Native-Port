cmake_minimum_required(VERSION 3.22)
project(aom_native VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Fontes do Motor AoM
set(AOM_SOURCES
    src/assets/bar_archive.cpp
    src/assets/brg_model.cpp
    src/assets/ddt_texture.cpp
    src/assets/cube_map.cpp
    src/assets/xmb_parser.cpp
    src/audio/opensl_engine.cpp
    src/engine/engine.cpp
    src/engine/ui_from_xmb.cpp
    src/renderer/gles_renderer.cpp
    src/renderer/camera_track.cpp
    src/ui/ui.cpp
    src/ui/ui_layout.cpp
    src/ai/ai_system.cpp
    src/video/video_player.cpp
    src/video/media_decoder.cpp
    src/anim/animation_system.cpp
    src/anim/grn_animation.cpp
    src/anim/prt_particle.cpp
    src/graphics/lighting.cpp
    src/campaign/campaign.cpp
    src/campaign/campaign_mission.cpp
    src/data/game_data.cpp
    src/data/water_bodies.cpp
    src/data/unit_transform.cpp
    src/data/proto_unit.cpp
    src/data/major_god.cpp
    src/data/sound_def.cpp
    src/fonts/font_system.cpp
    src/graphics/gfx_config.cpp
    src/game/god_powers.cpp
    src/game/scenario_trigger.cpp
    src/game/game_systems.cpp
    src/game/save_load.cpp
    src/game/victory.cpp
    src/locale/localization.cpp
    src/network/network_manager.cpp
    src/platform/android/native_activity.cpp
    src/platform/touch_input.cpp

    # Third Party Implementation
    third_party/stb_image.c
)

add_library(aom_native SHARED ${AOM_SOURCES})

# Configuração de defines para STB
# (stb .c files self-define their implementation macros)

target_include_directories(aom_native PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ndk-include
)

target_compile_options(aom_native PRIVATE
    -fPIC
    -Wall
    -Wextra
    -Wno-unused-parameter
    -D__ANDROID__
)

set(CMAKE_SKIP_RPATH TRUE)

find_library(SYSTEM_Z_LIB NAMES z PATHS /system/lib64 /system/lib NO_DEFAULT_PATH)
if(NOT SYSTEM_Z_LIB)
    set(SYSTEM_Z_LIB z)
endif()

# Media NDK (libmediandk.so) — sistema em API 24+; stub do Termux como fallback.
find_library(SYSTEM_MEDIA_NDK_LIB NAMES mediandk PATHS /system/lib64 /system/lib NO_DEFAULT_PATH)
if(NOT SYSTEM_MEDIA_NDK_LIB)
    set(SYSTEM_MEDIA_NDK_LIB mediandk)
endif()

# Linkagem correta para Android NDK
target_link_libraries(aom_native PRIVATE
    EGL
    GLESv3
    android
    log
    ${SYSTEM_MEDIA_NDK_LIB}
    ${SYSTEM_Z_LIB}
    OpenSLES
)

set_target_properties(aom_native PROPERTIES
    PREFIX "lib"
    OUTPUT_NAME "aom_native"
)

target_link_options(aom_native PRIVATE
    -Wl,--no-undefined
)

option(BUILD_TESTING "Build test executables" ON)

if(BUILD_TESTING)
# Testes Automatizados
add_executable(gameplay_test
    src/tests/gameplay_test.cpp
    src/assets/xmb_parser.cpp
    src/assets/bar_archive.cpp
    src/anim/animation_system.cpp
    src/game/save_load.cpp
    src/game/victory.cpp
    src/assets/cube_map.cpp
)
target_include_directories(gameplay_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm
)
target_link_libraries(gameplay_test PRIVATE
    z
    log
)

add_executable(ui_test
    src/tests/ui_test.cpp
    src/ui/ui.cpp
    third_party/stb_image.c
)
target_include_directories(ui_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm
)

add_executable(test_camera_track
    src/tests/test_camera_track.cpp
    src/renderer/camera_track.cpp
)
target_include_directories(test_camera_track PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm
)
target_link_libraries(test_camera_track PRIVATE
    log
)

add_executable(test_network
    src/tests/test_network.cpp
    src/network/network_manager.cpp
)
target_include_directories(test_network PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm
)
target_link_libraries(test_network PRIVATE
    log
)

add_executable(test_ai_system
    src/tests/test_ai_system.cpp
    src/ai/ai_system.cpp
    src/assets/xmb_parser.cpp
    src/anim/animation_system.cpp
)
target_include_directories(test_ai_system PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm
)
target_link_libraries(test_ai_system PRIVATE
    z
    log
)

add_executable(test_anim_video
    src/tests/test_anim_video.cpp
    src/video/video_player.cpp
    src/anim/animation_system.cpp
    src/assets/bar_archive.cpp
)
target_include_directories(test_anim_video PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm
)
target_link_libraries(test_anim_video PRIVATE
    z
    log
)

add_executable(test_data_docs
    src/tests/test_data_docs.cpp
    src/graphics/lighting.cpp
    src/campaign/campaign.cpp
    src/data/game_data.cpp
    src/assets/bar_archive.cpp
    src/assets/xmb_parser.cpp
)
target_include_directories(test_data_docs PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm
)
target_link_libraries(test_data_docs PRIVATE
    z
    log
)

add_executable(test_fonts_gfx
    src/tests/test_fonts_gfx.cpp
    src/fonts/font_system.cpp
    src/graphics/gfx_config.cpp
)
target_include_directories(test_fonts_gfx PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm
)
target_link_libraries(test_fonts_gfx PRIVATE
    log
)

add_executable(test_godpowers_scenarios
    src/tests/test_godpowers_scenarios.cpp
    src/game/god_powers.cpp
    src/game/scenario_trigger.cpp
    src/game/game_systems.cpp
    src/game/save_load.cpp
    src/game/victory.cpp
    src/assets/cube_map.cpp
    src/assets/bar_archive.cpp
    src/assets/xmb_parser.cpp
)
target_include_directories(test_godpowers_scenarios PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm
)
target_link_libraries(test_godpowers_scenarios PRIVATE
    ${SYSTEM_Z_LIB}
    log
)

add_executable(test_localization_lcids
    src/tests/test_localization_lcids.cpp
    src/locale/localization.cpp
)
target_include_directories(test_localization_lcids PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm
)
target_link_libraries(test_localization_lcids PRIVATE
    log
)

add_executable(test_terrain_water
    src/tests/test_terrain_water.cpp
    src/assets/bar_archive.cpp
    src/assets/ddt_texture.cpp
)
target_include_directories(test_terrain_water PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm
)
target_link_libraries(test_terrain_water PRIVATE
    ${SYSTEM_Z_LIB}
    log
)

add_executable(test_particles_anim
    src/tests/test_particles_anim.cpp
    src/assets/xmb_parser.cpp
    src/anim/animation_system.cpp
    src/anim/grn_animation.cpp
    src/anim/prt_particle.cpp
)
target_include_directories(test_particles_anim PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm
)
target_link_libraries(test_particles_anim PRIVATE
    ${SYSTEM_Z_LIB}
    log
)

add_executable(test_prt_particle
    src/tests/test_prt_particle.cpp
    src/anim/prt_particle.cpp
    src/assets/bar_archive.cpp
)
target_include_directories(test_prt_particle PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm
)
target_link_libraries(test_prt_particle PRIVATE
    ${SYSTEM_Z_LIB}
    log
)

add_executable(test_campaign_mission
    src/tests/test_campaign_mission.cpp
    src/campaign/campaign.cpp
    src/campaign/campaign_mission.cpp
    src/game/scenario_trigger.cpp
    src/renderer/camera_track.cpp
    src/assets/bar_archive.cpp
    src/assets/xmb_parser.cpp
    src/anim/animation_system.cpp
)
target_include_directories(test_campaign_mission PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm
)
target_link_libraries(test_campaign_mission PRIVATE
    ${SYSTEM_Z_LIB}
    log
)

add_executable(test_audio_3d
    src/tests/test_audio_3d.cpp
    src/audio/opensl_engine.cpp
    src/assets/bar_archive.cpp
)
target_include_directories(test_audio_3d PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ndk-include
)
target_link_libraries(test_audio_3d PRIVATE
    OpenSLES
    android
    ${SYSTEM_Z_LIB}
    log
)

add_executable(test_bar_overlay
    src/tests/test_bar_overlay.cpp
    src/assets/bar_archive.cpp
    src/assets/xmb_parser.cpp
    src/data/game_data.cpp
    src/campaign/campaign.cpp
)
target_include_directories(test_bar_overlay PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm
)
target_link_libraries(test_bar_overlay PRIVATE
    ${SYSTEM_Z_LIB}
    log
)

add_executable(test_water_bodies
    src/tests/test_water_bodies.cpp
    src/data/water_bodies.cpp
    src/data/unit_transform.cpp
    src/assets/bar_archive.cpp
    src/assets/xmb_parser.cpp
)
target_include_directories(test_water_bodies PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm
)
target_link_libraries(test_water_bodies PRIVATE
    ${SYSTEM_Z_LIB}
    log
)

add_executable(test_ui_layout
    src/tests/test_ui_layout.cpp
    src/ui/ui_layout.cpp
    src/assets/bar_archive.cpp
    src/assets/xmb_parser.cpp
)
target_include_directories(test_ui_layout PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm
)
target_link_libraries(test_ui_layout PRIVATE
    log
)

add_executable(test_data_integration
    src/tests/test_data_integration.cpp
    src/data/game_data.cpp
    src/data/water_bodies.cpp
    src/data/unit_transform.cpp
    src/data/proto_unit.cpp
    src/data/major_god.cpp
    src/data/sound_def.cpp
    src/assets/bar_archive.cpp
    src/assets/xmb_parser.cpp
)
target_include_directories(test_data_integration PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm
)
target_link_libraries(test_data_integration PRIVATE
    ${SYSTEM_Z_LIB}
    log
)

add_executable(test_ddt_reference
    src/tests/test_ddt_reference.cpp
    src/assets/bar_archive.cpp
    src/assets/ddt_texture.cpp
)
target_include_directories(test_ddt_reference PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm
)
target_link_libraries(test_ddt_reference PRIVATE
    ${SYSTEM_Z_LIB}
    log
)

endif(BUILD_TESTING)

