#pragma once

/// @file touch_input.h
/// @brief Touch input manager for Android
///
/// Maps touch gestures to RTS controls:
/// - Single tap → Select unit/building
/// - Double tap → Select all units of same type
/// - Long press → Context menu (build, research, etc.)
/// - Drag → Selection box / Camera pan
/// - Two-finger pinch → Zoom
/// - Two-finger rotate → Camera rotation
/// - Swipe from edge → Quick panel access

#include <cstdint>
#include <vector>
#include <functional>
#include <glm/glm.hpp>

namespace aom {

/// Touch pointer state
struct TouchPointer {
    int32_t id;            ///< Pointer ID (finger)
    glm::vec2 position;    ///< Current position in screen coords
    glm::vec2 start;       ///< Start position of this touch
    glm::vec2 previous;    ///< Previous frame position
    float pressure;        ///< Touch pressure (0-1)
    double start_time;     ///< Time when touch started
    bool active;           ///< Currently touching
};

/// Recognized gesture types
enum class GestureType {
    NONE,
    TAP,                   ///< Quick tap
    DOUBLE_TAP,            ///< Double tap
    LONG_PRESS,            ///< Hold finger for > 500ms
    DRAG,                  ///< Single finger drag (pan camera)
    SELECTION_BOX,         ///< Drag after selecting (create selection rect)
    PINCH_ZOOM,            ///< Two-finger pinch
    TWO_FINGER_PAN,        ///< Two-finger drag
    ROTATE,                ///< Two-finger rotation
    EDGE_SWIPE,            ///< Swipe from screen edge
};

/// Gesture event data
struct GestureEvent {
    GestureType type = GestureType::NONE;
    glm::vec2 position;         ///< Primary position
    glm::vec2 position2;        ///< Secondary position (for two-finger gestures)
    glm::vec2 delta;            ///< Movement delta
    float scale = 1.0f;         ///< Pinch scale factor
    float rotation = 0.0f;      ///< Rotation angle in radians
    bool started = false;       ///< True on first frame of gesture
    bool ended = false;         ///< True on last frame of gesture
};

/// Maps gestures to game commands
enum class GameCommand {
    NONE,
    SELECT_UNIT,           ///< Select unit at position
    SELECT_ALL_TYPE,       ///< Select all units of same type
    MOVE_UNITS,            ///< Move selected units to position
    ATTACK_TARGET,         ///< Attack target at position
    BUILD_AT,              ///< Place building at position
    CAMERA_PAN,            ///< Pan camera by delta
    CAMERA_ZOOM,           ///< Zoom camera by scale
    CAMERA_ROTATE,         ///< Rotate camera by angle
    OPEN_CONTEXT_MENU,     ///< Open radial context menu
    OPEN_BUILD_MENU,       ///< Open build menu
    OPEN_TECH_MENU,        ///< Open technology menu
    SELECTION_BOX_START,   ///< Begin box selection
    SELECTION_BOX_UPDATE,  ///< Update box selection
    SELECTION_BOX_END,     ///< End box selection
};

/// Game command event
struct CommandEvent {
    GameCommand command = GameCommand::NONE;
    glm::vec2 screen_pos;       ///< Screen position
    glm::vec3 world_pos;        ///< World position (ray-cast result)
    glm::vec2 delta;            ///< Movement delta
    float value = 0.0f;         ///< Scale/rotation value
    int32_t target_id = -1;     ///< Target entity ID
};

/// Touch input configuration
struct TouchConfig {
    float tap_max_distance = 20.0f;      ///< Max movement for tap (pixels)
    float tap_max_duration = 0.3f;       ///< Max duration for tap (seconds)
    float double_tap_interval = 0.3f;    ///< Max time between taps
    float long_press_duration = 0.5f;    ///< Hold time for long press
    float pinch_threshold = 10.0f;       ///< Min distance change for pinch
    float rotation_threshold = 5.0f;     ///< Min angle for rotation (degrees)
    float edge_margin = 40.0f;           ///< Edge swipe margin (pixels)
    float camera_pan_speed = 2.0f;       ///< Camera pan sensitivity
    float camera_zoom_speed = 1.5f;      ///< Camera zoom sensitivity
    float camera_rotation_speed = 1.0f;  ///< Camera rotation sensitivity
};

/// Touch input manager — handles raw touch events and emits game commands
class TouchInputManager {
public:
    TouchInputManager();
    ~TouchInputManager() = default;

    /// Set configuration
    void set_config(const TouchConfig& config) { config_ = config; }

    /// Set screen dimensions (for edge detection)
    void set_screen_size(int width, int height);

    /// Process a touch down event
    void on_touch_down(int32_t pointer_id, float x, float y, float pressure);

    /// Process a touch move event
    void on_touch_move(int32_t pointer_id, float x, float y, float pressure);

    /// Process a touch up event
    void on_touch_up(int32_t pointer_id, float x, float y);

    /// Update gesture recognition (call each frame)
    /// @param delta_time Time since last frame
    void update(float delta_time);

    /// Register callback for game commands
    using CommandCallback = std::function<void(const CommandEvent&)>;
    void set_command_callback(CommandCallback callback) {
        command_callback_ = std::move(callback);
    }

    /// Register callback for raw gestures (for UI overlay)
    using GestureCallback = std::function<void(const GestureEvent&)>;
    void set_gesture_callback(GestureCallback callback) {
        gesture_callback_ = std::move(callback);
    }

    /// Get current active gesture
    [[nodiscard]] GestureType current_gesture() const { return current_gesture_; }

    /// Get number of active touch pointers
    [[nodiscard]] int active_pointer_count() const;

private:
    void recognize_gesture();
    void emit_command(const CommandEvent& cmd);
    void emit_gesture(const GestureEvent& gesture);

    TouchPointer* find_pointer(int32_t id);
    bool is_edge_swipe(const glm::vec2& start) const;

    TouchConfig config_;
    std::vector<TouchPointer> pointers_;
    GestureType current_gesture_ = GestureType::NONE;
    double current_time_ = 0.0;
    int screen_width_ = 1920;
    int screen_height_ = 1080;

    CommandCallback command_callback_;
    GestureCallback gesture_callback_;
};

} // namespace aom
