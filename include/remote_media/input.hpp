#pragma once

#include <cstdint>
#include <variant>

namespace remote_media {

enum class KeyCode : std::uint16_t {
    unknown = 0,
    a = 0x04, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z,
    digit1 = 0x1e, digit2, digit3, digit4, digit5, digit6, digit7, digit8, digit9, digit0,
    enter = 0x28, escape, backspace, tab, space,
    minus, equal, left_bracket, right_bracket, backslash,
    semicolon = 0x33, apostrophe, grave, comma, period, slash, caps_lock,
    f1 = 0x3a, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12,
    print_screen, scroll_lock, pause, insert, home, page_up, delete_forward, end, page_down,
    right, left, down, up,
    num_lock = 0x53,
    left_control = 0xe0, left_shift, left_alt, left_meta,
    right_control, right_shift, right_alt, right_meta,
};

[[nodiscard]] constexpr bool is_valid(KeyCode code) noexcept {
    const auto value = static_cast<std::uint16_t>(code);
    return value == 0 || (value >= 0x04 && value <= 0x1d) ||
           (value >= 0x1e && value <= 0x39) ||
           (value >= 0x3a && value <= 0x52) || value == 0x53 ||
           (value >= 0xe0 && value <= 0xe7);
}

enum class KeyAction : std::uint8_t { pressed, released, repeated };
enum class PointerButton : std::uint8_t { primary = 1, secondary, middle, back, forward };
enum class ButtonAction : std::uint8_t { pressed, released };

struct KeyInput {
    KeyCode code{KeyCode::unknown};
    KeyAction action{KeyAction::pressed};
};

struct PointerMoveInput {
    std::int32_t x{};
    std::int32_t y{};
};

struct PointerButtonInput {
    PointerButton button{PointerButton::primary};
    ButtonAction action{ButtonAction::pressed};
};

struct ScrollInput {
    std::int32_t delta_x{};
    std::int32_t delta_y{};
};

using InputEvent = std::variant<KeyInput, PointerMoveInput, PointerButtonInput, ScrollInput>;

class InputSink {
public:
    virtual ~InputSink() = default;
    virtual void submit(const InputEvent& event) = 0;
};

} // namespace remote_media
