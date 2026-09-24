#include "remote_media/frame.hpp"
#include "remote_media/input.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <variant>
#include <vector>

namespace {

int failures = 0;

#define CHECK(expression) do { if (!(expression)) { \
    std::cerr << __FILE__ << ':' << __LINE__ << ": CHECK failed: " #expression "\n"; \
    ++failures; } } while (false)

#define CHECK_THROWS_AS(expression, exception_type) do { bool caught = false; \
    try { static_cast<void>(expression); } catch (const exception_type&) { caught = true; } \
    catch (...) {} CHECK(caught); } while (false)

using namespace remote_media;

void test_pixel_formats() {
    CHECK(bytes_per_pixel(PixelFormat::rgba8) == 4);
    CHECK(bytes_per_pixel(PixelFormat::rgb8) == 3);
    CHECK(bytes_per_pixel(PixelFormat::gray8) == 1);
    CHECK(!is_valid(static_cast<PixelFormat>(255)));
}

void test_dimensions_stride_and_bounds() {
    CHECK_THROWS_AS(OwnedFrame(0, 1, PixelFormat::rgba8), std::invalid_argument);
    CHECK_THROWS_AS(OwnedFrame(1, 0, PixelFormat::rgba8), std::invalid_argument);
    CHECK_THROWS_AS(OwnedFrame(2, 2, 7, PixelFormat::rgba8), std::invalid_argument);
    CHECK_THROWS_AS(OwnedFrame(1, 1, static_cast<PixelFormat>(99)), std::invalid_argument);
    CHECK_THROWS_AS(Rect(0, 0, 0, 1), std::invalid_argument);

    OwnedFrame padded(3, 2, 16, PixelFormat::rgba8);
    CHECK(padded.size_bytes() == 32);
    auto mutable_view = padded.mutable_view();
    CHECK(mutable_view.row(1) - mutable_view.row(0) == 16);
    CHECK_THROWS_AS(mutable_view.row(2), std::out_of_range);
    CHECK_THROWS_AS(mutable_view.subview(Rect(2, 0, 2, 1)), std::out_of_range);

    mutable_view.row(1)[4] = std::byte{0x5a};
    auto sub = mutable_view.subview(Rect(1, 1, 2, 1));
    CHECK(sub.data() == mutable_view.row(1) + 4);
    CHECK(sub.size_bytes() == 8);
    FrameView immutable = mutable_view;
    CHECK(immutable.row(1)[4] == std::byte{0x5a});

    std::vector<std::byte> small(7);
    CHECK_THROWS_AS(FrameView(small.data(), small.size(), 2, 1, 8,
                              PixelFormat::rgba8), std::invalid_argument);
}

void test_overflow() {
    CHECK_THROWS_AS(OwnedFrame(1, 2, std::numeric_limits<std::size_t>::max(),
                               PixelFormat::gray8), std::overflow_error);
    std::byte byte{};
    CHECK_THROWS_AS(FrameView(&byte, 1, 1, 2,
                              std::numeric_limits<std::size_t>::max(),
                              PixelFormat::gray8), std::overflow_error);
}

void test_copy_and_mailbox_sequence() {
    OwnedFrame source(2, 2, PixelFormat::gray8);
    source.mutable_view().row(0)[0] = std::byte{0x2a};
    OwnedFrame copy(source.view());
    source.mutable_view().row(0)[0] = std::byte{0x11};
    CHECK(copy.view().row(0)[0] == std::byte{0x2a});

    FrameMailbox mailbox;
    CHECK(mailbox.latest_sequence() == 0);
    CHECK(!mailbox.latest_after(0));
    CHECK_THROWS_AS(mailbox.publish(nullptr), std::invalid_argument);
    auto first = std::make_shared<OwnedFrame>(1, 1, PixelFormat::gray8);
    auto second = std::make_shared<OwnedFrame>(2, 1, PixelFormat::gray8);
    CHECK(mailbox.publish(first) == 1);
    CHECK(mailbox.publish(second) == 2);
    auto latest = mailbox.latest_after(0);
    CHECK(latest && latest->sequence == 2 && latest->frame == second);
    CHECK(!mailbox.latest_after(2));
    CHECK(!mailbox.latest_after(3));
}

void test_mailbox_concurrency() {
    FrameMailbox mailbox;
    constexpr std::uint64_t count = 2000;
    std::atomic<bool> done{false};
    std::thread producer([&] {
        for (std::uint64_t i = 0; i < count; ++i) {
            static_cast<void>(mailbox.publish(
                std::make_shared<OwnedFrame>(1, 1, PixelFormat::gray8)));
        }
        done = true;
    });
    std::uint64_t seen = 0;
    while (!done || seen < count) {
        if (auto item = mailbox.latest_after(seen)) {
            CHECK(item->sequence > seen);
            CHECK(item->frame != nullptr);
            seen = item->sequence;
        }
    }
    producer.join();
    CHECK(seen == count);
}

void test_input_dtos() {
    CHECK(static_cast<std::uint16_t>(KeyCode::a) == 0x04);
    CHECK(static_cast<std::uint16_t>(KeyCode::digit1) == 0x1e);
    CHECK(static_cast<std::uint16_t>(KeyCode::enter) == 0x28);
    CHECK(static_cast<std::uint16_t>(KeyCode::left_control) == 0xe0);
    CHECK(is_valid(KeyCode::f12));
    CHECK(is_valid(KeyCode::caps_lock));
    CHECK(!is_valid(static_cast<KeyCode>(0x54)));

    InputEvent event = KeyInput{KeyCode::a, KeyAction::pressed};
    CHECK(std::holds_alternative<KeyInput>(event));
    CHECK(std::get<KeyInput>(event).code == KeyCode::a);
    event = PointerMoveInput{-10, 20};
    CHECK(std::get<PointerMoveInput>(event).x == -10);
    event = PointerButtonInput{PointerButton::secondary, ButtonAction::released};
    CHECK(std::get<PointerButtonInput>(event).action == ButtonAction::released);
    event = ScrollInput{1, -2};
    CHECK(std::get<ScrollInput>(event).delta_y == -2);
    static_assert(std::has_virtual_destructor_v<InputSink>);
    static_assert(std::has_virtual_destructor_v<FrameProvider>);
}

} // namespace

int main() {
    test_pixel_formats();
    test_dimensions_stride_and_bounds();
    test_overflow();
    test_copy_and_mailbox_sequence();
    test_mailbox_concurrency();
    test_input_dtos();
    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "all tests passed\n";
    return 0;
}
