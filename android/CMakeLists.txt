cmake_minimum_required(VERSION 3.22)
project(aom_native LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(aom_native SHARED
    ../src/ai/ai_system.cpp
    ../src/assets/bar_archive.cpp
    ../src/assets/brg_model.cpp
    ../src/assets/ddt_texture.cpp
    ../src/assets/xmb_parser.cpp
    ../src/audio/opensl_engine.cpp
    ../src/engine/engine.cpp
    ../src/engine/engine_assets.cpp
    ../src/engine/engine_campaign.cpp
    ../src/engine/engine_gameplay.cpp
    ../src/engine/engine_hud.cpp
    ../src/engine/engine_input.cpp
    ../src/engine/engine_menus.cpp
    ../src/engine/engine_multiplayer.cpp
    ../src/engine/engine_options_menu.cpp
    ../src/engine/engine_particles.cpp
    ../src/engine/engine_save_load.cpp
    ../src/engine/engine_states.cpp
    ../src/engine/engine_terrain.cpp
    ../src/engine/ui_from_xmb.cpp
    ../src/renderer/gles_renderer.cpp
    ../src/ui/ui.cpp
    ../src/platform/android/native_activity.cpp
    ../src/platform/touch_input.cpp
    ../third_party/stb_image.c
    ../third_party/stb_truetype.c
)

target_include_directories(aom_native PRIVATE
    ../include
    ../third_party
    ../third_party/glm
    ../third_party/ndk-include
)

target_compile_definitions(aom_native PRIVATE __ANDROID__)

# (stb .c files self-define their implementation macros)

target_link_libraries(aom_native PRIVATE
    android
    log
    EGL
    GLESv3
    z
    OpenSLES
)
