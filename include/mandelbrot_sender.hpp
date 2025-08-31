#pragma once

#include <algorithm>
#include <stdexec/execution.hpp>

#include "types.hpp"

PixelMatrix ComputePixelMatrixForRegion(const mandelbrot::ViewPort &viewport, const RenderSettings &settings,
                                        const PixelRegion &region) {
    const auto screen_w = settings.width;
    const auto screen_h = settings.height;

    const auto start_r = std::min(region.start_row, screen_h);
    const auto end_r = std::min(region.end_row, screen_h);

    PixelMatrix result;
    result.resize(end_r - start_r);

    for (std::uint32_t r = start_r; r < end_r; ++r) {
        result[r - start_r].resize(screen_w);
        for (std::uint32_t c = 0; c < screen_w; ++c) {
            auto complex_point = mandelbrot::Pixel2DToComplex(c, r, viewport, screen_w, screen_h);
            result[r - start_r][c] =
                mandelbrot::CalculateIterationsForPoint(complex_point, settings.max_iterations, settings.escape_radius);
        }
    }
    return result;
}

template <typename Receiver>
struct MandelbrotOperationState {
    Receiver receiver_;
    mandelbrot::ViewPort viewport_;
    RenderSettings settings_;
    PixelRegion region_;

    void start() noexcept {
        try {
            stdexec::set_value(std::move(receiver_), ComputePixelMatrixForRegion(viewport_, settings_, region_));
        } catch (...) {
            stdexec::set_error(std::move(receiver_), std::current_exception());
        }
    }
};

struct MandelbrotSender {
    using sender_concept = stdexec::sender_t;

    mandelbrot::ViewPort viewport_;
    RenderSettings settings_;
    PixelRegion region_;

    template <typename Receiver>
    auto connect(Receiver &&receiver) const {
        return MandelbrotOperationState<std::decay_t<Receiver>>(std::forward<Receiver>(receiver), viewport_, settings_,
                                                                region_);
    }

    template <typename Env>
    auto get_completion_signatures(Env &&) const {
        return stdexec::completion_signatures<stdexec::set_value_t(PixelMatrix),
                                              stdexec::set_error_t(std::exception_ptr), stdexec::set_stopped_t()>{};
    }
};

[[nodiscard]] inline auto MakeMandelbrotSender(mandelbrot::ViewPort viewport, RenderSettings settings,
                                               PixelRegion region) {
    return MandelbrotSender{viewport, settings, region};
}
