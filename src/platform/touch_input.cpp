#include "platform/touch_input.h"

#include <algorithm>
#include <cmath>

namespace aom {

TouchInputManager::TouchInputManager() = default;

void TouchInputManager::set_screen_size(int width, int height) {
    screen_width_ = width;
    screen_height_ = height;
}

TouchPointer* TouchInputManager::find_pointer(int32_t id) {
    for (auto& p : pointers_) {
        if (p.id == id) return &p;
    }
    return nullptr;
}

void TouchInputManager::on_touch_down(int32_t pointer_id, float x, float y, float pressure) {
    TouchPointer* existing = find_pointer(pointer_id);
    if (existing) {
        existing->position = {x, y};
        existing->previous = existing->position;
        existing->pressure = pressure;
        return;
    }
    TouchPointer p{};
    p.id = pointer_id;
    p.position = {x, y};
    p.start = {x, y};
    p.previous = {x, y};
    p.pressure = pressure;
    p.start_time = current_time_;
    p.active = true;
    pointers_.push_back(p);
}

void TouchInputManager::on_touch_move(int32_t pointer_id, float x, float y, float pressure) {
    TouchPointer* p = find_pointer(pointer_id);
    if (!p || !p->active) return;
    p->previous = p->position;
    p->position = {x, y};
    p->pressure = pressure;
}

void TouchInputManager::on_touch_up(int32_t pointer_id, float x, float y) {
    TouchPointer* p = find_pointer(pointer_id);
    if (!p) return;
    p->previous = p->position;
    p->position = {x, y};
    p->active = false;
}

int TouchInputManager::active_pointer_count() const {
    int n = 0;
    for (const auto& p : pointers_) if (p.active) ++n;
    return n;
}

bool TouchInputManager::is_edge_swipe(const glm::vec2& start) const {
    return start.x < config_.edge_margin || start.x > screen_width_ - config_.edge_margin ||
           start.y < config_.edge_margin || start.y > screen_height_ - config_.edge_margin;
}

void TouchInputManager::recognize_gesture() {
    int active = active_pointer_count();
    if (active == 0) {
        current_gesture_ = GestureType::NONE;
        return;
    }
    if (active == 1) {
        const TouchPointer* p = nullptr;
        for (const auto& pp : pointers_) if (pp.active) { p = &pp; break; }
        if (!p) return;
        const float dist = glm::length(p->position - p->start);
        const double elapsed = current_time_ - p->start_time;
        if (dist > config_.tap_max_distance) {
            current_gesture_ = GestureType::DRAG;
        } else if (elapsed >= config_.long_press_duration) {
            current_gesture_ = GestureType::LONG_PRESS;
        } else {
            current_gesture_ = GestureType::NONE;
        }
    } else if (active >= 2) {
        const TouchPointer* a = nullptr;
        const TouchPointer* b = nullptr;
        for (const auto& pp : pointers_) {
            if (!pp.active) continue;
            if (!a) a = &pp;
            else if (!b) { b = &pp; break; }
        }
        if (!a || !b) return;
        const float d0 = glm::length(a->start - b->start);
        const float d1 = glm::length(a->position - b->position);
        if (std::fabs(d1 - d0) > config_.pinch_threshold) {
            current_gesture_ = GestureType::PINCH_ZOOM;
        } else {
            const glm::vec2 delta = (a->position + b->position) * 0.5f - (a->start + b->start) * 0.5f;
            if (glm::length(delta) > config_.tap_max_distance) {
                current_gesture_ = GestureType::TWO_FINGER_PAN;
            } else {
                current_gesture_ = GestureType::NONE;
            }
        }
    }
}

void TouchInputManager::emit_gesture(const GestureEvent& gesture) {
    if (gesture_callback_) gesture_callback_(gesture);
}

void TouchInputManager::emit_command(const CommandEvent& cmd) {
    if (command_callback_) command_callback_(cmd);
}

void TouchInputManager::update(float delta_time) {
    current_time_ += delta_time;
    recognize_gesture();

    for (auto it = pointers_.begin(); it != pointers_.end();) {
        if (!it->active && (current_time_ - it->start_time) > 1.0) {
            it = pointers_.erase(it);
        } else {
            ++it;
        }
    }

    for (const auto& p : pointers_) {
        if (p.active) continue;
        const float dist = glm::length(p.position - p.start);
        const double elapsed = (current_time_ - p.start_time);
        if (dist <= config_.tap_max_distance && elapsed <= config_.tap_max_duration + 0.1) {
            CommandEvent cmd;
            cmd.command = GameCommand::SELECT_UNIT;
            cmd.screen_pos = p.position;
            emit_command(cmd);
            GestureEvent g;
            g.type = GestureType::TAP;
            g.position = p.position;
            g.ended = true;
            emit_gesture(g);
        }
    }
}

} // namespace aom
