#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace remote_media {

enum class PixelFormat : std::uint8_t {
    rgba8,
    bgra8,
    rgb8,
    bgr8,
    gray8,
};

[[nodiscard]] constexpr bool is_valid(PixelFormat format) noexcept {
    switch (format) {
    case PixelFormat::rgba8:
    case PixelFormat::bgra8:
    case PixelFormat::rgb8:
    case PixelFormat::bgr8:
    case PixelFormat::gray8:
        return true;
    }
    return false;
}

[[nodiscard]] constexpr std::size_t bytes_per_pixel(PixelFormat format) noexcept {
    switch (format) {
    case PixelFormat::rgba8:
    case PixelFormat::bgra8:
        return 4;
    case PixelFormat::rgb8:
    case PixelFormat::bgr8:
        return 3;
    case PixelFormat::gray8:
        return 1;
    }
    return 0;
}

struct Rect {
    std::uint32_t x{};
    std::uint32_t y{};
    std::uint32_t width{};
    std::uint32_t height{};

    Rect(std::uint32_t x, std::uint32_t y, std::uint32_t width,
         std::uint32_t height);

    [[nodiscard]] bool fits_within(std::uint32_t outer_width,
                                   std::uint32_t outer_height) const noexcept;
};

class MutableFrameView;

class FrameView {
public:
    FrameView(const std::byte* data, std::size_t data_size,
              std::uint32_t width, std::uint32_t height,
              std::size_t stride, PixelFormat format);
    FrameView(const MutableFrameView& view) noexcept;

    [[nodiscard]] const std::byte* data() const noexcept { return data_; }
    [[nodiscard]] std::size_t size_bytes() const noexcept { return data_size_; }
    [[nodiscard]] std::uint32_t width() const noexcept { return width_; }
    [[nodiscard]] std::uint32_t height() const noexcept { return height_; }
    [[nodiscard]] std::size_t stride() const noexcept { return stride_; }
    [[nodiscard]] PixelFormat format() const noexcept { return format_; }
    [[nodiscard]] const std::byte* row(std::uint32_t y) const;
    [[nodiscard]] FrameView subview(const Rect& rect) const;

private:
    const std::byte* data_{};
    std::size_t data_size_{};
    std::uint32_t width_{};
    std::uint32_t height_{};
    std::size_t stride_{};
    PixelFormat format_{PixelFormat::rgba8};
};

class MutableFrameView {
public:
    MutableFrameView(std::byte* data, std::size_t data_size,
                     std::uint32_t width, std::uint32_t height,
                     std::size_t stride, PixelFormat format);

    [[nodiscard]] std::byte* data() const noexcept { return data_; }
    [[nodiscard]] std::size_t size_bytes() const noexcept { return data_size_; }
    [[nodiscard]] std::uint32_t width() const noexcept { return width_; }
    [[nodiscard]] std::uint32_t height() const noexcept { return height_; }
    [[nodiscard]] std::size_t stride() const noexcept { return stride_; }
    [[nodiscard]] PixelFormat format() const noexcept { return format_; }
    [[nodiscard]] std::byte* row(std::uint32_t y) const;
    [[nodiscard]] MutableFrameView subview(const Rect& rect) const;

private:
    std::byte* data_{};
    std::size_t data_size_{};
    std::uint32_t width_{};
    std::uint32_t height_{};
    std::size_t stride_{};
    PixelFormat format_{PixelFormat::rgba8};
};

class OwnedFrame {
public:
    OwnedFrame(std::uint32_t width, std::uint32_t height, PixelFormat format);
    OwnedFrame(std::uint32_t width, std::uint32_t height,
               std::size_t stride, PixelFormat format);
    explicit OwnedFrame(FrameView source);

    [[nodiscard]] FrameView view() const;
    [[nodiscard]] MutableFrameView mutable_view();
    [[nodiscard]] std::uint32_t width() const noexcept { return width_; }
    [[nodiscard]] std::uint32_t height() const noexcept { return height_; }
    [[nodiscard]] std::size_t stride() const noexcept { return stride_; }
    [[nodiscard]] PixelFormat format() const noexcept { return format_; }
    [[nodiscard]] std::size_t size_bytes() const noexcept { return pixels_.size(); }

private:
    std::uint32_t width_{};
    std::uint32_t height_{};
    std::size_t stride_{};
    PixelFormat format_{PixelFormat::rgba8};
    std::vector<std::byte> pixels_;
};

struct SequencedFrame {
    std::uint64_t sequence{};
    std::shared_ptr<const OwnedFrame> frame;
};

class FrameProvider {
public:
    virtual ~FrameProvider() = default;
    [[nodiscard]] virtual std::optional<SequencedFrame>
    latest_after(std::uint64_t sequence) const = 0;
};

class FrameMailbox final : public FrameProvider {
public:
    FrameMailbox();
    ~FrameMailbox();
    FrameMailbox(const FrameMailbox&) = delete;
    FrameMailbox& operator=(const FrameMailbox&) = delete;

    [[nodiscard]] std::uint64_t publish(std::shared_ptr<const OwnedFrame> frame);
    [[nodiscard]] std::optional<SequencedFrame>
    latest_after(std::uint64_t sequence) const override;
    [[nodiscard]] std::uint64_t latest_sequence() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace remote_media
