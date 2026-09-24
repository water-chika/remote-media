#include "remote_media/frame.hpp"

#include <cstring>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace remote_media {
namespace {

std::size_t checked_multiply(std::size_t a, std::size_t b) {
    if (a != 0 && b > std::numeric_limits<std::size_t>::max() / a) {
        throw std::overflow_error("frame size overflows size_t");
    }
    return a * b;
}

std::size_t checked_add(std::size_t a, std::size_t b) {
    if (b > std::numeric_limits<std::size_t>::max() - a) {
        throw std::overflow_error("frame size overflows size_t");
    }
    return a + b;
}

std::size_t minimum_stride(std::uint32_t width, PixelFormat format) {
    if (!is_valid(format)) {
        throw std::invalid_argument("invalid pixel format");
    }
    return checked_multiply(width, bytes_per_pixel(format));
}

void validate_view(const void* data, std::size_t data_size, std::uint32_t width,
                   std::uint32_t height, std::size_t stride, PixelFormat format) {
    if (width == 0 || height == 0) {
        throw std::invalid_argument("frame dimensions must be non-zero");
    }
    const auto row_bytes = minimum_stride(width, format);
    if (stride < row_bytes) {
        throw std::invalid_argument("stride is smaller than a pixel row");
    }
    const auto required = checked_add(checked_multiply(stride, height - 1), row_bytes);
    if (data == nullptr) {
        throw std::invalid_argument("frame data must not be null");
    }
    if (data_size < required) {
        throw std::invalid_argument("frame buffer is too small");
    }
}

std::size_t subview_size(std::size_t stride, std::uint32_t width,
                         std::uint32_t height, PixelFormat format) {
    const auto row_bytes = minimum_stride(width, format);
    return checked_add(checked_multiply(stride, height - 1), row_bytes);
}

} // namespace

Rect::Rect(std::uint32_t x_value, std::uint32_t y_value,
           std::uint32_t width_value, std::uint32_t height_value)
    : x(x_value), y(y_value), width(width_value), height(height_value) {
    if (width == 0 || height == 0) {
        throw std::invalid_argument("rectangle dimensions must be non-zero");
    }
}

bool Rect::fits_within(std::uint32_t outer_width,
                       std::uint32_t outer_height) const noexcept {
    return x <= outer_width && y <= outer_height &&
           width <= outer_width - x && height <= outer_height - y;
}

FrameView::FrameView(const std::byte* data, std::size_t data_size,
                     std::uint32_t width, std::uint32_t height,
                     std::size_t stride, PixelFormat format)
    : data_(data), data_size_(data_size), width_(width), height_(height),
      stride_(stride), format_(format) {
    validate_view(data, data_size, width, height, stride, format);
}

FrameView::FrameView(const MutableFrameView& view) noexcept
    : data_(view.data()), data_size_(view.size_bytes()), width_(view.width()),
      height_(view.height()), stride_(view.stride()), format_(view.format()) {}

const std::byte* FrameView::row(std::uint32_t y) const {
    if (y >= height_) {
        throw std::out_of_range("frame row is out of range");
    }
    return data_ + checked_multiply(stride_, y);
}

FrameView FrameView::subview(const Rect& rect) const {
    if (!rect.fits_within(width_, height_)) {
        throw std::out_of_range("rectangle is outside frame");
    }
    const auto offset = checked_add(
        checked_multiply(stride_, rect.y),
        checked_multiply(rect.x, bytes_per_pixel(format_)));
    return FrameView(data_ + offset,
                     subview_size(stride_, rect.width, rect.height, format_),
                     rect.width, rect.height, stride_, format_);
}

MutableFrameView::MutableFrameView(std::byte* data, std::size_t data_size,
                                   std::uint32_t width, std::uint32_t height,
                                   std::size_t stride, PixelFormat format)
    : data_(data), data_size_(data_size), width_(width), height_(height),
      stride_(stride), format_(format) {
    validate_view(data, data_size, width, height, stride, format);
}

std::byte* MutableFrameView::row(std::uint32_t y) const {
    if (y >= height_) {
        throw std::out_of_range("frame row is out of range");
    }
    return data_ + checked_multiply(stride_, y);
}

MutableFrameView MutableFrameView::subview(const Rect& rect) const {
    if (!rect.fits_within(width_, height_)) {
        throw std::out_of_range("rectangle is outside frame");
    }
    const auto offset = checked_add(
        checked_multiply(stride_, rect.y),
        checked_multiply(rect.x, bytes_per_pixel(format_)));
    return MutableFrameView(data_ + offset,
                            subview_size(stride_, rect.width, rect.height, format_),
                            rect.width, rect.height, stride_, format_);
}

OwnedFrame::OwnedFrame(std::uint32_t width, std::uint32_t height,
                       PixelFormat format)
    : OwnedFrame(width, height, minimum_stride(width, format), format) {}

OwnedFrame::OwnedFrame(std::uint32_t width, std::uint32_t height,
                       std::size_t stride, PixelFormat format)
    : width_(width), height_(height), stride_(stride), format_(format) {
    if (width == 0 || height == 0) {
        throw std::invalid_argument("frame dimensions must be non-zero");
    }
    if (stride < minimum_stride(width, format)) {
        throw std::invalid_argument("stride is smaller than a pixel row");
    }
    pixels_.resize(checked_multiply(stride, height));
}

OwnedFrame::OwnedFrame(FrameView source)
    : OwnedFrame(source.width(), source.height(), source.stride(), source.format()) {
    const auto row_bytes = minimum_stride(source.width(), source.format());
    for (std::uint32_t y = 0; y < source.height(); ++y) {
        std::memcpy(pixels_.data() + checked_multiply(stride_, y),
                    source.row(y), row_bytes);
    }
}

FrameView OwnedFrame::view() const {
    return FrameView(pixels_.data(), pixels_.size(), width_, height_, stride_, format_);
}

MutableFrameView OwnedFrame::mutable_view() {
    return MutableFrameView(pixels_.data(), pixels_.size(), width_, height_, stride_, format_);
}

class FrameMailbox::Impl {
public:
    mutable std::mutex mutex;
    std::uint64_t sequence{};
    std::shared_ptr<const OwnedFrame> frame;
};

FrameMailbox::FrameMailbox() : impl_(std::make_unique<Impl>()) {}
FrameMailbox::~FrameMailbox() = default;

std::uint64_t FrameMailbox::publish(std::shared_ptr<const OwnedFrame> frame) {
    if (!frame) {
        throw std::invalid_argument("published frame must not be null");
    }
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->sequence == std::numeric_limits<std::uint64_t>::max()) {
        throw std::overflow_error("frame sequence exhausted");
    }
    impl_->frame = std::move(frame);
    return ++impl_->sequence;
}

std::optional<SequencedFrame>
FrameMailbox::latest_after(std::uint64_t sequence) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->frame || impl_->sequence <= sequence) {
        return std::nullopt;
    }
    return SequencedFrame{impl_->sequence, impl_->frame};
}

std::uint64_t FrameMailbox::latest_sequence() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->sequence;
}

} // namespace remote_media
