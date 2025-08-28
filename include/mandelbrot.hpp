#pragma once

#include "mandelbrot_renderer.hpp"

class CalculateMandelbrotAsyncSender {
public:
    explicit CalculateMandelbrotAsyncSender(AppState &state, RenderSettings render_settings,
                                            MandelbrotRenderer &renderer)
        : state_(state), render_settings_{render_settings}, renderer_{renderer} {}

    /* Ваш код здесь  */

private:
    RenderSettings render_settings_;
    MandelbrotRenderer &renderer_;
    AppState &state_;
};
