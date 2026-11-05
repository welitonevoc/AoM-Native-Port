/// @file native_activity.cpp
/// @brief Android NativeActivity entry point for AoM Native Port
///
/// M2: creates an EGL/GLES3 surface, loads one real DDT texture from
/// textures.bar and draws it as a fullscreen quad.

#include "engine/engine.h"

#include <android/native_activity.h>
#include <android/native_window.h>
#include <android/input.h>
#include <android/log.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <ctime>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#define LOG_TAG "AoM-Native"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

struct AppState {
    ANativeActivity* activity = nullptr;
    ANativeWindow* window = nullptr;
    std::atomic<AInputQueue*> input_queue{nullptr};

    std::unique_ptr<aom::Engine> engine;

    std::thread game_thread;
    std::mutex mutex;
    std::condition_variable cond;

    std::atomic<bool> running{false};
    std::atomic<bool> has_focus{false};
    std::atomic<bool> has_window{false};
    std::atomic<bool> destroy_requested{false};

    std::string data_path;
    // Pinch‑zoom state
    bool pinch_active = false;
    float last_pinch_distance = 0.0f;
    float last_distance = 0.0f;
};

double get_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

/// Verifica se uma pasta contém os arquivos que a engine usa como ponto de
/// partida. Aceita tanto a estrutura com subpastas (data/data.bar) quanto a
/// estrutura plana (data.bar), usada por instalações antigas.
bool contains_game_data(const std::filesystem::path& dir) {
    std::error_code ec;
    return std::filesystem::is_regular_file(dir / "data" / "data.bar", ec) ||
           std::filesystem::is_regular_file(dir / "textures" / "textures.bar", ec) ||
           std::filesystem::is_regular_file(dir / "data.bar", ec) ||
           std::filesystem::is_regular_file(dir / "textures.bar", ec) ||
           std::filesystem::is_regular_file(dir / "models" / "models.bar", ec);
}

/// Resolve a pasta real dos dados. Além da própria pasta candidata, aceita um
/// único nível de contêiner (por exemplo Download/AoM/Age of Mythology/...),
/// que é o resultado comum de copiar a pasta inteira do jogo manualmente.
std::string resolve_game_data_root(const std::string& candidate) {
    namespace fs = std::filesystem;
    const fs::path root(candidate);
    std::error_code ec;

    if (contains_game_data(root)) return root.string();
    if (!fs::is_directory(root, ec)) return {};

    // Nomes de contêiner mais comuns primeiro (ordem determinística).
    for (const char* nested_name : {"Age of Mythology", "AoM"}) {
        const fs::path nested = root / nested_name;
        if (contains_game_data(nested)) return nested.string();
    }

    // Qualquer subpasta de primeiro nível que contenha a estrutura esperada.
    fs::directory_iterator it(root, ec);
    const fs::directory_iterator end;
    while (!ec && it != end) {
        std::error_code entry_ec;
        if (it->is_directory(entry_ec) && contains_game_data(it->path())) {
            return it->path().string();
        }
        it.increment(ec);
    }
    return {};
}

void process_input_queue(AInputQueue* queue, aom::Engine* engine, AppState* state) {
    if (!queue) return;

    AInputEvent* event = nullptr;
    while (AInputQueue_getEvent(queue, &event) >= 0) {
        bool handled = false;
        if (engine && AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
            const int32_t action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
            const size_t pointer_count = AMotionEvent_getPointerCount(event);
            aom::TouchEvent::State state_enum;
            bool valid = true;
            switch (action) {
                case AMOTION_EVENT_ACTION_DOWN:
                case AMOTION_EVENT_ACTION_POINTER_DOWN:
                    state_enum = aom::TouchEvent::State::DOWN;
                    break;
                case AMOTION_EVENT_ACTION_MOVE:
                    state_enum = aom::TouchEvent::State::MOVE;
                    break;
                case AMOTION_EVENT_ACTION_UP:
                case AMOTION_EVENT_ACTION_POINTER_UP:
                case AMOTION_EVENT_ACTION_CANCEL:
                    state_enum = aom::TouchEvent::State::UP;
                    break;
                default:
                    valid = false;
                    break;
            }

            if (pointer_count >= 2) {
                const float dx = AMotionEvent_getX(event, 1) - AMotionEvent_getX(event, 0);
                const float dy = AMotionEvent_getY(event, 1) - AMotionEvent_getY(event, 0);
                const float distance = std::sqrt(dx * dx + dy * dy);
                if (action == AMOTION_EVENT_ACTION_POINTER_DOWN || state->last_distance <= 0.0f) {
                    state->last_distance = distance;
                } else if (action == AMOTION_EVENT_ACTION_MOVE) {
                    engine->adjust_camera_zoom((distance - state->last_distance) * 0.05f);
                    state->last_distance = distance;
                }
            } else {
                state->last_distance = 0.0f;
            }

            if (valid && pointer_count > 0) {
                engine->handle_touch({AMotionEvent_getX(event, 0),
                                      AMotionEvent_getY(event, 0), state_enum});
                handled = true;
            }
        }
        AInputQueue_finishEvent(queue, event, handled ? 1 : 0);
    }
}

void drain_input_queue(AInputQueue* queue) {
    process_input_queue(queue, nullptr, nullptr);
}

void game_loop(AppState* state) {
    LOGI("Game thread started");

    {
        std::unique_lock<std::mutex> lock(state->mutex);
        state->cond.wait(lock, [state] {
            return state->has_window.load() || state->destroy_requested.load();
        });
    }

    if (state->destroy_requested.load()) {
        return;
    }

    aom::EngineConfig config;
    config.data_path = state->data_path;
    config.use_vulkan = false;

    {
        std::lock_guard<std::mutex> lock(state->mutex);
        if (!state->window) return;
        config.window_width = ANativeWindow_getWidth(state->window);
        config.window_height = ANativeWindow_getHeight(state->window);
    }

    auto engine = std::make_unique<aom::Engine>();
    if (state->activity) {
        engine->set_asset_manager(state->activity->assetManager);
        engine->set_native_activity(state->activity);
    }
    if (!engine->initialize(config, state->window)) {
        LOGE("Failed to initialize engine, keeping safe input loop active to prevent ANR");
        while (!state->destroy_requested.load()) {
            drain_input_queue(state->input_queue.load());
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        return;
    }
    state->engine = std::move(engine);

    state->running = true;
    double last_frame_time = get_time();
    ANativeWindow* active_window = state->window;
    int last_width = 0;
    int last_height = 0;

    LOGI("Engine initialized, entering game loop");

    while (!state->destroy_requested.load()) {
        std::unique_lock<std::mutex> lock(state->mutex);

        if (state->destroy_requested.load()) break;

        if (!state->has_window.load() || !state->window) {
            if (active_window) {
                state->engine->release_renderer();
                active_window = nullptr;
                last_width = 0;
                last_height = 0;
            }
            lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        ANativeWindow* window = state->window;
        const int width = ANativeWindow_getWidth(window);
        const int height = ANativeWindow_getHeight(window);

        if (window != active_window) {
            state->engine->release_renderer();
            if (!state->engine->recreate_renderer(window)) {
                LOGE("Renderer recreation failed");
            }
            active_window = window;
            last_width = 0;
            last_height = 0;
        }

        if (width > 0 && height > 0 && (width != last_width || height != last_height)) {
            state->engine->resize(width, height);
            last_width = width;
            last_height = height;
        }

        process_input_queue(state->input_queue.load(), state->engine.get(), state);

        const double now = get_time();
        const float delta = static_cast<float>(
            std::min(now - last_frame_time, 0.1));
        last_frame_time = now;

        if (state->has_focus.load()) {
            state->engine->update(delta);
            state->engine->render();
        }

        lock.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    state->engine->shutdown();
    state->engine.reset();
    state->running = false;

    LOGI("Game thread exiting");
}

void on_start(ANativeActivity*) {
    LOGI("onStart");
}

void on_resume(ANativeActivity* activity) {
    LOGI("onResume");
    auto* state = static_cast<AppState*>(activity->instance);
    state->has_focus = true;
}

void on_pause(ANativeActivity* activity) {
    LOGI("onPause");
    auto* state = static_cast<AppState*>(activity->instance);
    state->has_focus = false;
}

void on_stop(ANativeActivity*) {
    LOGI("onStop");
}

void on_destroy(ANativeActivity* activity) {
    LOGI("onDestroy");
    auto* state = static_cast<AppState*>(activity->instance);
    if (!state) return;

    state->destroy_requested = true;
    state->cond.notify_all();

    if (state->game_thread.joinable()) {
        state->game_thread.join();
    }

    delete state;
    activity->instance = nullptr;
}

void on_window_focus_changed(ANativeActivity* activity, int has_focus) {
    LOGI("onWindowFocusChanged: %d", has_focus);
    auto* state = static_cast<AppState*>(activity->instance);
    if (state) state->has_focus = (has_focus != 0);
}

void on_native_window_created(ANativeActivity* activity, ANativeWindow* window) {
    LOGI("onNativeWindowCreated: %dx%d",
         ANativeWindow_getWidth(window), ANativeWindow_getHeight(window));

    auto* state = static_cast<AppState*>(activity->instance);
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->window = window;
        state->has_window = true;
    }
    state->cond.notify_all();
}

void on_native_window_destroyed(ANativeActivity* activity, ANativeWindow*) {
    LOGI("onNativeWindowDestroyed");
    auto* state = static_cast<AppState*>(activity->instance);
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->window = nullptr;
        state->has_window = false;
    }
}

void on_native_window_resized(ANativeActivity* activity, ANativeWindow* window) {
    LOGI("onNativeWindowResized: %dx%d",
         ANativeWindow_getWidth(window), ANativeWindow_getHeight(window));
    (void)activity;
}

void on_input_queue_created(ANativeActivity* activity, AInputQueue* queue) {
    auto* state = static_cast<AppState*>(activity->instance);
    state->input_queue = queue;
}

void on_input_queue_destroyed(ANativeActivity* activity, AInputQueue* queue) {
    auto* state = static_cast<AppState*>(activity->instance);
    AInputQueue* expected = queue;
    state->input_queue.compare_exchange_strong(expected, nullptr);
}

void on_content_rect_changed(ANativeActivity*, const ARect*) {}
void on_low_memory(ANativeActivity*) {
    LOGI("onLowMemory");
}

} // anonymous namespace

void ANativeActivity_onCreate(ANativeActivity* activity,
                              void* saved_state, size_t saved_state_size) {
    (void)saved_state;
    (void)saved_state_size;

    LOGI("ANativeActivity_onCreate");

    activity->callbacks->onStart = on_start;
    activity->callbacks->onResume = on_resume;
    activity->callbacks->onPause = on_pause;
    activity->callbacks->onStop = on_stop;
    activity->callbacks->onDestroy = on_destroy;
    activity->callbacks->onWindowFocusChanged = on_window_focus_changed;
    activity->callbacks->onNativeWindowCreated = on_native_window_created;
    activity->callbacks->onNativeWindowDestroyed = on_native_window_destroyed;
    activity->callbacks->onNativeWindowResized = on_native_window_resized;
    activity->callbacks->onInputQueueCreated = on_input_queue_created;
    activity->callbacks->onInputQueueDestroyed = on_input_queue_destroyed;
    activity->callbacks->onContentRectChanged = on_content_rect_changed;
    activity->callbacks->onLowMemory = on_low_memory;

    auto* state = new AppState();
    state->activity = activity;

    // Pastas candidatas, em ordem de prioridade. A pasta privada do app vem
    // primeiro porque é gravável sem MANAGE_EXTERNAL_STORAGE e é o destino do
    // importador interno (OPCOES > IMPORTAR DADOS DO JOGO).
    std::vector<std::string> candidates;
    std::string fallback_path;
    if (activity->externalDataPath) {
        const std::string external = activity->externalDataPath;
        fallback_path = external + "/AoM";
        candidates.push_back(fallback_path);
        candidates.push_back(external);
    }
    if (activity->internalDataPath) {
        const std::string internal = activity->internalDataPath;
        candidates.push_back(internal + "/AoM");
        candidates.push_back(internal);
    }
    candidates.emplace_back("/storage/emulated/0/Documents/AoM");
    candidates.emplace_back("/sdcard/Documents/AoM");
    candidates.emplace_back("/storage/emulated/0/Download/AoM");
    candidates.emplace_back("/sdcard/Download/AoM");
    candidates.emplace_back("/storage/emulated/0/AoM");
    candidates.emplace_back("/sdcard/AoM");
    candidates.emplace_back("/storage/emulated/0/Documents/AoM-Native-Port/data");
    candidates.emplace_back("/storage/emulated/0/Download/Games/PC/Age of Mythology/Age of Mythology");
    candidates.emplace_back("/sdcard/Download/Games/PC/Age of Mythology/Age of Mythology");
    candidates.emplace_back("/storage/emulated/0/Download/Age of Mythology");
    candidates.emplace_back("/sdcard/Download/Age of Mythology");

    for (const std::string& candidate : candidates) {
        const std::string resolved = resolve_game_data_root(candidate);
        if (!resolved.empty()) {
            state->data_path = resolved;
            LOGI("Discovered AoM data at: %s", resolved.c_str());
            break;
        }
        LOGI("AoM data not found at: %s", candidate.c_str());
    }
    if (state->data_path.empty()) {
        state->data_path = fallback_path.empty()
            ? "/sdcard/Download/AoM"
            : fallback_path;
        std::error_code ec;
        std::filesystem::create_directories(state->data_path, ec);
        LOGW("No AoM .bar archives found. Fallback directory: %s", state->data_path.c_str());
        LOGW("Use OPCOES > IMPORTAR DADOS DO JOGO or copy data/data.bar + textures/textures.bar there.");
    }
    activity->instance = state;

    LOGI("Game data path: %s", state->data_path.c_str());

    state->game_thread = std::thread(game_loop, state);
}
