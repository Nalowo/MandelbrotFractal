#pragma once

#include <chrono>
#include <vector>

#include "mandelbrot_fractal_utils.hpp"

const constexpr std::uint32_t THREAD_POOL_SIZE{8};

using PixelMatrix = std::vector<std::vector<std::uint32_t>>;
using ColorMatrix = std::vector<std::vector<mandelbrot::RgbColor>>;

struct RenderSettings {
    std::uint32_t width{800};
    std::uint32_t height{600};
    std::uint32_t max_iterations{100};
    double escape_radius{2.0};
};

struct PixelRegion {
    std::uint32_t start_row{};
    std::uint32_t end_row{};
    std::uint32_t start_col{};
    std::uint32_t end_col{};
};

struct RenderResult {
    PixelMatrix pixel_data;
    ColorMatrix color_data;
    mandelbrot::ViewPort viewport;
    RenderSettings settings;
    std::chrono::milliseconds render_time{};
};

struct AppState {
    mandelbrot::ViewPort viewport;
    bool need_rerender{true};
    bool left_mouse_pressed{false};
    bool right_mouse_pressed{false};
    bool should_exit{false};
};
